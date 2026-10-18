#include "VaesenMannequinSplitter.h"

#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "BoneWeights.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/DynamicVertexSkinWeightsAttribute.h"
#include "DynamicMeshToMeshDescription.h"
#include "MeshBoundaryLoops.h"
#include "MeshDescription.h"
#include "Engine/SkeletalMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "IAssetTools.h"
#include "LODUtilities.h"
#include "Misc/PackageName.h"
#include "SkeletalMeshAttributes.h"
#include "UDynamicMesh.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenMannequinSplitter, Log, All);

namespace VaesenMannequinSplitter
{
	const TCHAR* const PartsFolder = TEXT("/Game/Characters/Mannequins/Meshes/Modular");
	const TCHAR* const SourceMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny");

	enum class EPart : uint8 { Head, Torso, Arms, Legs, Feet };

	struct FPartInfo
	{
		EPart Part;
		const TCHAR* AssetName;
	};

	const FPartInfo Parts[] = {
		{ EPart::Head, TEXT("SKM_Manny_Head") },
		{ EPart::Torso, TEXT("SKM_Manny_Torso") },
		{ EPart::Arms, TEXT("SKM_Manny_Arms") },
		{ EPart::Legs, TEXT("SKM_Manny_Legs") },
		{ EPart::Feet, TEXT("SKM_Manny_Feet") },
	};

	/** Which part a bone with this name starts, if any (UE5 mannequin bone names). */
	TOptional<EPart> PartOfBoneName(const FName BoneName)
	{
		const FString Name = BoneName.ToString().ToLower();
		const auto StartsWithAny = [&Name](std::initializer_list<const TCHAR*> Prefixes)
		{
			for (const TCHAR* Prefix : Prefixes)
			{
				if (Name.StartsWith(Prefix))
				{
					return true;
				}
			}
			return false;
		};

		if (StartsWithAny({ TEXT("head"), TEXT("neck") }))
		{
			return EPart::Head;
		}
		if (StartsWithAny({ TEXT("foot"), TEXT("ball") }))
		{
			return EPart::Feet;
		}
		if (StartsWithAny({ TEXT("pelvis"), TEXT("thigh"), TEXT("calf") }))
		{
			return EPart::Legs;
		}
		if (StartsWithAny({ TEXT("upperarm"), TEXT("lowerarm"), TEXT("hand"), TEXT("thumb"), TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky") }))
		{
			return EPart::Arms;
		}
		if (StartsWithAny({ TEXT("spine"), TEXT("clavicle") }))
		{
			return EPart::Torso;
		}
		return {};
	}

	/**
	 * Which part a bone belongs to: the first bone up its parent chain (itself included) whose name says it.
	 * Helper/corrective bones (e.g. ankle_bck_l under calf_l) follow their parent's part; anything else is Torso.
	 */
	EPart ClassifyBone(const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
	{
		for (int32 Index = BoneIndex; Index != INDEX_NONE; Index = RefSkeleton.GetParentIndex(Index))
		{
			if (const TOptional<EPart> Part = PartOfBoneName(RefSkeleton.GetBoneName(Index)))
			{
				return *Part;
			}
		}
		return EPart::Torso;
	}

	bool SavePackageOf(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		return UPackage::SavePackage(Package, Asset, *FileName, Args);
	}

	/**
	 * Writes Mesh into LOD 0 of Target. Same steps as GeometryScript's CopyMeshToSkeletalMesh, minus its undo
	 * transaction (which asserts in commandlets, where there's no undo history).
	 */
	void WriteToSkeletalMesh(const UDynamicMesh* Mesh, USkeletalMesh* Target)
	{
		constexpr int32 LODIndex = 0;
		FlushRenderingCommands();

		FMeshDescription* MeshDescription = Target->CreateMeshDescription(LODIndex);
		check(MeshDescription);
		FSkeletalMeshAttributes MeshAttributes(*MeshDescription);
		MeshAttributes.Register();
		Target->ModifyMeshDescription(LODIndex);

		FDynamicMeshToMeshDescription Converter;
		Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
		{
			Converter.Convert(&ReadMesh, *MeshDescription, /*bCopyTangents*/ false);
		});

		FSkeletalMeshLODInfo* LODInfo = Target->GetLODInfo(LODIndex);
		LODInfo->BuildSettings.bRecomputeNormals = false;
		LODInfo->BuildSettings.bRecomputeTangents = true;
		LODInfo->ReductionSettings.NumOfTrianglesPercentage = 1.0f;
		LODInfo->ReductionSettings.NumOfVertPercentage = 1.0f;
		LODInfo->ReductionSettings.MaxNumOfTriangles = MAX_int32;
		LODInfo->ReductionSettings.MaxNumOfVerts = MAX_int32;
		LODInfo->ReductionSettings.BaseLOD = LODIndex;

		Target->CommitMeshDescription(LODIndex);
		Target->PostEditChange();
	}

	/**
	 * Hole filling may add vertices inside a cap; they have no skin weights, so they wouldn't follow the animation.
	 * Give each new vertex (ID >= FirstNewVertex) the weights of the nearest original vertex.
	 */
	int32 SkinNewVertices(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstNewVertex)
	{
		using namespace UE::Geometry;
		FDynamicMeshVertexSkinWeightsAttribute* Weights = Mesh.HasAttributes()
			? Mesh.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName)
			: nullptr;
		if (!Weights)
		{
			return 0;
		}

		int32 Skinned = 0;
		for (const int32 NewVertex : Mesh.VertexIndicesItr())
		{
			if (NewVertex < FirstNewVertex)
			{
				continue;
			}

			const FVector3d Position = Mesh.GetVertex(NewVertex);
			int32 Nearest = INDEX_NONE;
			double NearestDistance = TNumericLimits<double>::Max();
			for (const int32 Original : Mesh.VertexIndicesItr())
			{
				if (Original >= FirstNewVertex)
				{
					continue;
				}
				const double Distance = FVector3d::DistSquared(Position, Mesh.GetVertex(Original));
				if (Distance < NearestDistance)
				{
					NearestDistance = Distance;
					Nearest = Original;
				}
			}

			if (Nearest != INDEX_NONE)
			{
				UE::AnimationCore::FBoneWeights NearestWeights;
				Weights->GetValue(Nearest, NearestWeights);
				Weights->SetValue(NewVertex, NearestWeights);
				++Skinned;
			}
		}
		return Skinned;
	}

	/** Removes every triangle that doesn't belong to Part. Triangles go to the part most of their corners belong to. */
	void KeepOnlyPart(UE::Geometry::FDynamicMesh3& Mesh, const TArray<EPart>& BoneParts, EPart Part, int32& OutKept, int32& OutRemoved)
	{
		using namespace UE::Geometry;

		const FDynamicMeshVertexSkinWeightsAttribute* Weights = Mesh.HasAttributes()
			? Mesh.Attributes()->GetSkinWeightsAttribute(FSkeletalMeshAttributes::DefaultSkinWeightProfileName)
			: nullptr;

		TMap<int32, EPart> VertexParts;
		for (const int32 VertexID : Mesh.VertexIndicesItr())
		{
			EPart VertexPart = EPart::Torso;
			if (Weights)
			{
				UE::AnimationCore::FBoneWeights BoneWeights;
				Weights->GetValue(VertexID, BoneWeights);
				float BestWeight = -1.0f;
				for (int32 Index = 0; Index < BoneWeights.Num(); ++Index)
				{
					const UE::AnimationCore::FBoneWeight& BoneWeight = BoneWeights[Index];
					if (BoneWeight.GetWeight() > BestWeight && BoneParts.IsValidIndex(BoneWeight.GetBoneIndex()))
					{
						BestWeight = BoneWeight.GetWeight();
						VertexPart = BoneParts[BoneWeight.GetBoneIndex()];
					}
				}
			}
			VertexParts.Add(VertexID, VertexPart);
		}

		TArray<int32> ToRemove;
		OutKept = 0;
		for (const int32 TriangleID : Mesh.TriangleIndicesItr())
		{
			const FIndex3i Corners = Mesh.GetTriangle(TriangleID);
			const EPart A = VertexParts[Corners.A];
			const EPart B = VertexParts[Corners.B];
			const EPart C = VertexParts[Corners.C];
			// Majority of the corners; if all three differ, the first corner decides.
			const EPart TrianglePart = (B == C) ? B : A;
			if (TrianglePart == Part)
			{
				++OutKept;
			}
			else
			{
				ToRemove.Add(TriangleID);
			}
		}

		for (const int32 TriangleID : ToRemove)
		{
			Mesh.RemoveTriangle(TriangleID, /*bRemoveIsolatedVertices*/ true);
		}
		OutRemoved = ToRemove.Num();
	}

	bool SplitMannequin()
	{
		USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, SourceMeshPath);
		if (!Source)
		{
			UE_LOG(LogVaesenMannequinSplitter, Error, TEXT("%s not found."), SourceMeshPath);
			return false;
		}

		// Bone index -> part, from the source's reference skeleton (the dynamic mesh's bone indices use it).
		const FReferenceSkeleton& RefSkeleton = Source->GetRefSkeleton();
		TArray<EPart> BoneParts;
		for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
		{
			BoneParts.Add(ClassifyBone(RefSkeleton, BoneIndex));
		}

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		bool bAllOk = true;

		for (const FPartInfo& Info : Parts)
		{
			const FString PackagePath = FString(PartsFolder) / Info.AssetName;
			if (FPackageName::DoesPackageExist(PackagePath))
			{
				UE_LOG(LogVaesenMannequinSplitter, Display, TEXT("%s already exists; leaving it alone."), *PackagePath);
				continue;
			}

			// Start from a copy of Manny (materials, skeleton, physics asset), keep only LOD0.
			USkeletalMesh* Target = Cast<USkeletalMesh>(AssetTools.DuplicateAsset(Info.AssetName, PartsFolder, Source));
			if (!Target)
			{
				UE_LOG(LogVaesenMannequinSplitter, Error, TEXT("Couldn't create %s."), *PackagePath);
				bAllOk = false;
				continue;
			}
			FSkeletalMeshUpdateContext UpdateContext;
			UpdateContext.SkeletalMesh = Target;
			for (int32 LODIndex = Target->GetLODNum() - 1; LODIndex > 0; --LODIndex)
			{
				FLODUtilities::RemoveLOD(UpdateContext, LODIndex);
			}
			// Manny asks for LOD 2 on some platforms; the part only has LOD 0.
			Target->SetMinLod(FPerPlatformInt(0));
			Target->SetQualityLevelMinLod(FPerQualityLevelInt(0));

			UDynamicMesh* Mesh = NewObject<UDynamicMesh>();
			EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
			FGeometryScriptCopyMeshFromAssetOptions ReadOptions;
			FGeometryScriptMeshReadLOD ReadLOD;
			ReadLOD.LODType = EGeometryScriptLODType::SourceModel;
			ReadLOD.LODIndex = 0;
			UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(Source, Mesh, ReadOptions, ReadLOD, Outcome);
			if (Outcome != EGeometryScriptOutcomePins::Success)
			{
				UE_LOG(LogVaesenMannequinSplitter, Error, TEXT("Couldn't read %s."), SourceMeshPath);
				return false;
			}

			int32 Kept = 0;
			int32 Removed = 0;
			Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh)
			{
				KeepOnlyPart(EditMesh, BoneParts, Info.Part, Kept, Removed);
			});

			// Close the seams (the engine picks the best fill per opening; curved ones like the neck need more than a
			// flat triangulation). Vertices it adds get the weights of their nearest neighbour, so caps move with the body.
			int32 FirstNewVertex = 0;
			Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh) { FirstNewVertex = ReadMesh.MaxVertexID(); });
			FGeometryScriptFillHolesOptions FillOptions;
			FillOptions.FillMethod = EGeometryScriptFillHolesMethod::Automatic;
			FillOptions.bDeleteIsolatedTriangles = false;
			int32 Filled = 0;
			int32 FailedFills = 0;
			UGeometryScriptLibrary_MeshRepairFunctions::FillAllMeshHoles(Mesh, FillOptions, Filled, FailedFills);
			// Height range of the part (Manny stands on Z = 0), to spot stray pieces: e.g. the torso should stay between the hips and the neck.
			Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
			{
				UE::Geometry::FAxisAlignedBox3d Bounds = UE::Geometry::FAxisAlignedBox3d::Empty();
				int32 LooseVertices = 0;
				for (const int32 TriangleID : ReadMesh.TriangleIndicesItr())
				{
					const UE::Geometry::FIndex3i Corners = ReadMesh.GetTriangle(TriangleID);
					Bounds.Contain(ReadMesh.GetVertex(Corners.A));
					Bounds.Contain(ReadMesh.GetVertex(Corners.B));
					Bounds.Contain(ReadMesh.GetVertex(Corners.C));
				}
				for (const int32 VertexID : ReadMesh.VertexIndicesItr())
				{
					LooseVertices += ReadMesh.GetVtxEdgeCount(VertexID) == 0 ? 1 : 0;
				}
				UE_LOG(LogVaesenMannequinSplitter, Display, TEXT("%s: %d loose vertices."), Info.AssetName, LooseVertices);
				UE_LOG(LogVaesenMannequinSplitter, Display, TEXT("%s: spans Z %.0f..%.0f cm, X %.0f..%.0f, Y %.0f..%.0f."),
					Info.AssetName, Bounds.Min.Z, Bounds.Max.Z, Bounds.Min.X, Bounds.Max.X, Bounds.Min.Y, Bounds.Max.Y);
			});

			int32 NewVertices = 0;
			Mesh->EditMesh([&](UE::Geometry::FDynamicMesh3& EditMesh) { NewVertices = SkinNewVertices(EditMesh, FirstNewVertex); });

			// Report openings that stayed open, by height (Manny stands on Z = 0; the neck is around 150 cm).
			Mesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& ReadMesh)
			{
				UE::Geometry::FMeshBoundaryLoops Loops(&ReadMesh, true);
				for (const UE::Geometry::FEdgeLoop& Loop : Loops.Loops)
				{
					FVector3d Center = FVector3d::Zero();
					for (const int32 VertexID : Loop.Vertices)
					{
						Center += ReadMesh.GetVertex(VertexID);
					}
					Center /= FMath::Max(1, Loop.Vertices.Num());
					UE_LOG(LogVaesenMannequinSplitter, Display, TEXT("%s: still open: %d edges around (%.0f, %.0f, %.0f)."),
						Info.AssetName, Loop.Edges.Num(), Center.X, Center.Y, Center.Z);
				}
			});

			WriteToSkeletalMesh(Mesh, Target);

			const bool bSaved = SavePackageOf(Target);
			UE_LOG(LogVaesenMannequinSplitter, Display, TEXT("%s: %d triangles kept, %d removed, %d seams closed (%d failed, %d cap vertices skinned), LODs %d, %s."),
				Info.AssetName, Kept, Removed, Filled, FailedFills, NewVertices, Target->GetLODNum(), bSaved ? TEXT("saved") : TEXT("SAVE FAILED"));
			bAllOk &= bSaved && Kept > 0;
		}

		return bAllOk;
	}
}
