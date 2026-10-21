#include "Spike/VaesenSpikeSliderLibrary.h"

#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_Root.h"
#include "AnimationGraph.h"
#include "AnimationRuntime.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "FileHelpers.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Spike/AnimGraphNode_VaesenFaceJointOffsets.h"
#include "VaesenMannequinSplitter.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "DNAReader.h"
#include "DNAUtils.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/FileHelper.h"
#include "ReferenceSkeleton.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkeletalMeshAttributes.h"

namespace VaesenSpikeSlider
{
	/** A signed axis permutation: Out[i] = Sign[i] * In[Axis[i]]. */
	struct FAxisMap
	{
		int32 Axis[3] = { 0, 1, 2 };
		double Sign[3] = { 1, 1, 1 };
		double Scale = 1.0;
		/** Added after the permutation (positions only; direction vectors use ApplyToVector). */
		FVector Offset = FVector::ZeroVector;

		FVector ApplyToVector(const FVector& V) const
		{
			return FVector(Sign[0] * V[Axis[0]], Sign[1] * V[Axis[1]], Sign[2] * V[Axis[2]]) * Scale;
		}

		FVector Apply(const FVector& V) const
		{
			return ApplyToVector(V) + Offset;
		}
	};

	TArray<FAxisMap> AllAxisMaps(double Scale)
	{
		TArray<FAxisMap> Maps;
		const int32 Perms[6][3] = { {0,1,2}, {0,2,1}, {1,0,2}, {1,2,0}, {2,0,1}, {2,1,0} };
		for (const auto& Perm : Perms)
		{
			for (int32 Signs = 0; Signs < 8; ++Signs)
			{
				FAxisMap Map;
				for (int32 I = 0; I < 3; ++I)
				{
					Map.Axis[I] = Perm[I];
					Map.Sign[I] = (Signs & (1 << I)) ? -1.0 : 1.0;
				}
				Map.Scale = Scale;
				Maps.Add(Map);
			}
		}
		return Maps;
	}

	/** Uniform grid for nearest-point lookups. */
	struct FGrid
	{
		double Cell = 0.5;
		TMap<FIntVector, TArray<int32>> Cells;
		const TArray<FVector>* Points = nullptr;

		FIntVector Key(const FVector& P) const { return FIntVector(FMath::FloorToInt32(P.X / Cell), FMath::FloorToInt32(P.Y / Cell), FMath::FloorToInt32(P.Z / Cell)); }

		void Build(const TArray<FVector>& InPoints, double InCell)
		{
			Points = &InPoints;
			Cell = InCell;
			Cells.Reset();
			for (int32 I = 0; I < InPoints.Num(); ++I)
			{
				Cells.FindOrAdd(Key(InPoints[I])).Add(I);
			}
		}

		int32 Nearest(const FVector& P, double& OutDistance, int32 MaxRing = 3) const
		{
			const FIntVector Center = Key(P);
			int32 Best = INDEX_NONE;
			double BestSq = TNumericLimits<double>::Max();
			for (int32 Ring = 0; Ring <= MaxRing && Best == INDEX_NONE; ++Ring)
			{
				for (int32 X = -Ring; X <= Ring; ++X)
				for (int32 Y = -Ring; Y <= Ring; ++Y)
				for (int32 Z = -Ring; Z <= Ring; ++Z)
				{
					if (const TArray<int32>* Bucket = Cells.Find(Center + FIntVector(X, Y, Z)))
					{
						for (const int32 Index : *Bucket)
						{
							const double Sq = FVector::DistSquared((*Points)[Index], P);
							if (Sq < BestSq)
							{
								BestSq = Sq;
								Best = Index;
							}
						}
					}
				}
			}
			OutDistance = Best == INDEX_NONE ? TNumericLimits<double>::Max() : FMath::Sqrt(BestSq);
			return Best;
		}
	};

	TArray<FVector> ReadVertices(const IDNAReader& Reader, uint16 Mesh)
	{
		TArray<FVector> Out;
		const uint32 Count = Reader.GetVertexPositionCount(Mesh);
		Out.Reserve(Count);
		for (uint32 I = 0; I < Count; ++I)
		{
			Out.Add(Reader.GetVertexPosition(Mesh, I));
		}
		return Out;
	}

	double MeanNearest(const TArray<FVector>& Samples, const FAxisMap& Map, const FGrid& Grid)
	{
		double Sum = 0.0;
		for (const FVector& S : Samples)
		{
			double Distance = 0.0;
			if (Grid.Nearest(Map.Apply(S), Distance, 2) == INDEX_NONE)
			{
				Distance = 100.0; // Nothing nearby: a wrong mapping.
			}
			Sum += Distance;
		}
		return Sum / FMath::Max(1, Samples.Num());
	}
}

FString UVaesenSpikeSliderLibrary::AddSliderMorphFromDNA(USkeletalMesh* HeadMesh, const FString& BaseDNAPath, const FString& VariantDNAPath, FName MorphName,
	const FString& JointOffsetsJsonPath, bool bCompensateJoints)
{
	using namespace VaesenSpikeSlider;
	TArray<FString> Report;
	auto Fail = [&Report](const FString& Why) { Report.Insert(TEXT("FAILED ") + Why, 0); return FString::Join(Report, TEXT("\n")); };

	if (!HeadMesh)
	{
		return Fail(TEXT("no head mesh"));
	}
	const TSharedPtr<IDNAReader> Base = ReadDNAFromFile(BaseDNAPath, EDNADataLayer::All);
	const TSharedPtr<IDNAReader> Variant = ReadDNAFromFile(VariantDNAPath, EDNADataLayer::All);
	if (!Base || !Variant)
	{
		return Fail(FString::Printf(TEXT("couldn't read DNA (%s, %s)"), *BaseDNAPath, *VariantDNAPath));
	}

	// The built head mesh's LOD0 source geometry.
	FMeshDescription* MeshDescription = HeadMesh->GetMeshDescription(0);
	if (!MeshDescription)
	{
		return Fail(TEXT("head mesh has no LOD0 mesh description"));
	}
	FSkeletalMeshAttributes Attributes(*MeshDescription);
	const TVertexAttributesConstRef<FVector3f> Positions = Attributes.GetVertexPositions();
	TArray<FVector> MeshVerts;
	TArray<FVertexID> MeshVertIds;
	for (const FVertexID Id : MeshDescription->Vertices().GetElementIDs())
	{
		MeshVerts.Add(FVector(Positions[Id]));
		MeshVertIds.Add(Id);
	}
	FGrid MeshGrid;
	MeshGrid.Build(MeshVerts, 0.5);

	// Which DNA mesh the built head is (the build may not start at DNA LOD0), and how the DNA's axes and units map to
	// the mesh: the best fit over every head mesh and axis mapping.
	const double Scale = Base->GetTranslationUnit() == ETranslationUnit::M ? 100.0 : 1.0;
	uint16 DnaMesh = 0;
	FAxisMap Map;
	double BestError = TNumericLimits<double>::Max();
	TArray<FVector> BaseVerts, Samples;
	for (uint16 M = 0; M < Base->GetMeshCount(); ++M)
	{
		if (!Base->GetMeshName(M).StartsWith(TEXT("head_lod")))
		{
			continue;
		}
		const TArray<FVector> Verts = ReadVertices(*Base, M);
		TArray<FVector> MeshSamples;
		for (int32 I = 0; I < Verts.Num(); I += FMath::Max(1, Verts.Num() / 400))
		{
			MeshSamples.Add(Verts[I]);
		}
		for (const FAxisMap& Candidate : AllAxisMaps(Scale))
		{
			const double Error = MeanNearest(MeshSamples, Candidate, MeshGrid);
			if (Error < BestError)
			{
				BestError = Error;
				Map = Candidate;
				DnaMesh = M;
				BaseVerts = Verts;
				Samples = MeshSamples;
			}
		}
		Report.Add(FString::Printf(TEXT("DNA mesh %d '%s': %d vertices"), M, *Base->GetMeshName(M), Verts.Num()));
	}
	const TArray<FVector> VariantVerts = ReadVertices(*Variant, DnaMesh);
	if (BaseVerts.Num() == 0 || BaseVerts.Num() != VariantVerts.Num())
	{
		return Fail(FString::Printf(TEXT("vertex counts differ or empty (%d vs %d)"), BaseVerts.Num(), VariantVerts.Num()));
	}
	Report.Add(FString::Printf(TEXT("built head: %d vertices; best DNA mesh %d '%s'; %d joints; rotation sequence %d"),
		MeshVerts.Num(), DnaMesh, *Base->GetMeshName(DnaMesh), Base->GetJointCount(), (int32)Base->GetRotationSequence()));
	Report.Add(FString::Printf(TEXT("axis map: out = (%+.0f*in[%d], %+.0f*in[%d], %+.0f*in[%d]) x%.0f, mean fit error %.4f cm"),
		Map.Sign[0], Map.Axis[0], Map.Sign[1], Map.Axis[1], Map.Sign[2], Map.Axis[2], Map.Scale, BestError));

	// The built mesh may sit slightly shifted from the DNA: fit a translation (a few rounds of mean nearest offset).
	for (int32 Round = 0; Round < 4; ++Round)
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FVector& S : Samples)
		{
			double Distance = 0.0;
			const FVector Mapped = Map.Apply(S);
			const int32 Nearest = MeshGrid.Nearest(Mapped, Distance, 2);
			if (Nearest != INDEX_NONE)
			{
				Sum += MeshVerts[Nearest] - Mapped;
				++Count;
			}
		}
		if (Count)
		{
			Map.Offset += Sum / Count;
		}
	}
	BestError = MeanNearest(Samples, Map, MeshGrid);
	Report.Add(FString::Printf(TEXT("offset fit: (%.4f, %.4f, %.4f) cm, mean fit error now %.4f cm"), Map.Offset.X, Map.Offset.Y, Map.Offset.Z, BestError));
	if (BestError > 0.05)
	{
		return Fail(FString::Printf(TEXT("DNA and mesh don't line up (mean error %.3f cm)"), BestError));
	}

	const FReferenceSkeleton& RefSkeleton = HeadMesh->GetRefSkeleton();
	TArray<FTransform> RefComponent;
	FAnimationRuntime::FillUpComponentSpaceTransforms(RefSkeleton, RefSkeleton.GetRefBonePose(), RefComponent);

	// 1. Each mesh vertex's move: its matching DNA vertex's move. The built head also holds the eyes, teeth, lashes and
	//    so on (other DNA meshes); those stay unmatched, get no morph and follow the joints.
	FGrid DnaGrid;
	TArray<FVector> BaseMapped;
	for (const FVector& V : BaseVerts)
	{
		BaseMapped.Add(Map.Apply(V));
	}
	DnaGrid.Build(BaseMapped, 0.5);
	TArray<FVector> RawDelta;
	TArray<bool> Matched;
	RawDelta.SetNumZeroed(MeshVerts.Num());
	Matched.SetNumZeroed(MeshVerts.Num());
	int32 Unmatched = 0;
	for (int32 I = 0; I < MeshVerts.Num(); ++I)
	{
		double Distance = 0.0;
		const int32 DnaVertex = DnaGrid.Nearest(MeshVerts[I], Distance, 1);
		if (DnaVertex == INDEX_NONE || Distance > 0.01)
		{
			++Unmatched;
			continue;
		}
		RawDelta[I] = Map.ApplyToVector(VariantVerts[DnaVertex] - BaseVerts[DnaVertex]);
		Matched[I] = true;
	}
	if (Unmatched > MeshVerts.Num() / 2)
	{
		return Fail(TEXT("most mesh vertices have no DNA vertex"));
	}

	// 2. Each face joint's move: the skin-weighted average move of the skin it drives. (The DNA's own joint positions
	//    use a convention that doesn't line up with the mesh skeleton in 5.8, so they aren't used.)
	const FSkinWeightsVertexAttributesRef SkinWeights = Attributes.GetVertexSkinWeights();
	TMap<int32, TPair<FVector, double>> JointAccumulator; // Bone -> (sum of weight * move, sum of weight)
	for (int32 I = 0; I < MeshVerts.Num(); ++I)
	{
		if (!Matched[I])
		{
			continue;
		}
		for (const UE::AnimationCore::FBoneWeight& Weight : SkinWeights.Get(MeshVertIds[I]))
		{
			TPair<FVector, double>& Acc = JointAccumulator.FindOrAdd(Weight.GetBoneIndex(), { FVector::ZeroVector, 0.0 });
			Acc.Key += RawDelta[I] * Weight.GetWeight();
			Acc.Value += Weight.GetWeight();
		}
	}
	TMap<int32, FVector> JointWorldDelta;
	if (bCompensateJoints)
	{
		for (const TPair<int32, TPair<FVector, double>>& Pair : JointAccumulator)
		{
			// Only joints that really drive skin (at least the weight of one full vertex), and not the head or neck
			// joints: those move the whole head, which a sculpt slider must not.
			const FString Name = RefSkeleton.GetBoneName(Pair.Key).ToString();
			if (Pair.Value.Value >= 1.0 && Name.StartsWith(TEXT("FACIAL_")))
			{
				JointWorldDelta.Add(Pair.Key, Pair.Value.Key / Pair.Value.Value);
			}
		}
	}

	// 3. Joint offsets in bone-parent space (rotations are unchanged by the sculpt): the inverse parent rotation applied
	//    to (this joint's world move - its parent's world move).
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	int32 MovedJoints = 0;
	double MaxJointMove = 0.0;
	for (const TPair<int32, FVector>& Pair : JointWorldDelta)
	{
		const int32 Parent = RefSkeleton.GetParentIndex(Pair.Key);
		const FVector* ParentDelta = Parent != INDEX_NONE ? JointWorldDelta.Find(Parent) : nullptr;
		const FQuat ParentRotation = Parent != INDEX_NONE ? RefComponent[Parent].GetRotation() : FQuat::Identity;
		const FVector Local = ParentRotation.UnrotateVector(Pair.Value - (ParentDelta ? *ParentDelta : FVector::ZeroVector));
		MaxJointMove = FMath::Max(MaxJointMove, Pair.Value.Size());
		if (Local.SizeSquared() > FMath::Square(0.001))
		{
			Json->SetArrayField(RefSkeleton.GetBoneName(Pair.Key).ToString(),
				{ MakeShared<FJsonValueNumber>(Local.X), MakeShared<FJsonValueNumber>(Local.Y), MakeShared<FJsonValueNumber>(Local.Z) });
			++MovedJoints;
		}
	}
	Report.Add(FString::Printf(TEXT("joints: %d moved (largest move %.3f cm)"), MovedJoints, MaxJointMove));

	// 4. The morph: each vertex's move minus what the moved joints already cause through its skin weights.
	if (!Attributes.GetMorphTargetNames().Contains(MorphName))
	{
		Attributes.RegisterMorphTargetAttribute(MorphName, false);
	}
	TVertexAttributesRef<FVector3f> Deltas = Attributes.GetVertexMorphPositionDelta(MorphName);
	int32 Moved = 0;
	double MaxMove = 0.0, MaxCompensation = 0.0, MaxRaw = 0.0;
	for (int32 I = 0; I < MeshVerts.Num(); ++I)
	{
		FVector FromJoints = FVector::ZeroVector;
		for (const UE::AnimationCore::FBoneWeight& Weight : SkinWeights.Get(MeshVertIds[I]))
		{
			if (const FVector* JointDelta = JointWorldDelta.Find(Weight.GetBoneIndex()))
			{
				FromJoints += *JointDelta * Weight.GetWeight();
			}
		}
		// Unmatched parts (eyes, teeth...) get no morph of their own; they move only with the joints.
		const FVector Delta = Matched[I] ? RawDelta[I] - FromJoints : FVector::ZeroVector;
		Deltas[MeshVertIds[I]] = FVector3f(Delta);
		MaxRaw = FMath::Max(MaxRaw, RawDelta[I].Size());
		MaxCompensation = FMath::Max(MaxCompensation, FromJoints.Size());
		MaxMove = FMath::Max(MaxMove, Delta.Size());
		Moved += Delta.SizeSquared() > FMath::Square(0.001) ? 1 : 0;
	}
	Report.Add(FString::Printf(TEXT("morph '%s': %d of %d vertices moved; largest sculpt move %.3f cm, of which joints cover up to %.3f cm, morph up to %.3f cm; %d unmatched (eyes, teeth...)"),
		*MorphName.ToString(), Moved, MeshVerts.Num(), MaxRaw, MaxCompensation, MaxMove, Unmatched));

	HeadMesh->CommitMeshDescription(0);

	// 5. The same morph on every lower LOD, so the slider holds at gameplay distance: each built LOD matched to its own
	//    DNA head mesh (best fit with the axis map found above), the joint moves taken from LOD 0 (lower LODs drive
	//    fewer joints; those that remain move the same way).
	for (int32 Lod = 1; Lod < HeadMesh->GetLODNum(); ++Lod)
	{
		FMeshDescription* LodDescription = HeadMesh->GetMeshDescription(Lod);
		if (!LodDescription)
		{
			Report.Add(FString::Printf(TEXT("LOD %d: no mesh description (generated LOD), skipped"), Lod));
			continue;
		}
		FSkeletalMeshAttributes LodAttributes(*LodDescription);
		const TVertexAttributesConstRef<FVector3f> LodPositions = LodAttributes.GetVertexPositions();
		TArray<FVector> LodVerts;
		TArray<FVertexID> LodIds;
		for (const FVertexID Id : LodDescription->Vertices().GetElementIDs())
		{
			LodVerts.Add(FVector(LodPositions[Id]));
			LodIds.Add(Id);
		}
		FGrid LodGrid;
		LodGrid.Build(LodVerts, 0.5);

		uint16 LodDnaMesh = 0;
		double LodError = TNumericLimits<double>::Max();
		for (uint16 M = 0; M < Base->GetMeshCount(); ++M)
		{
			if (!Base->GetMeshName(M).StartsWith(TEXT("head_lod")))
			{
				continue;
			}
			const TArray<FVector> Verts = ReadVertices(*Base, M);
			TArray<FVector> LodSamples;
			for (int32 I = 0; I < Verts.Num(); I += FMath::Max(1, Verts.Num() / 300))
			{
				LodSamples.Add(Verts[I]);
			}
			const double Error = MeanNearest(LodSamples, Map, LodGrid);
			if (Error < LodError)
			{
				LodError = Error;
				LodDnaMesh = M;
			}
		}
		if (LodError > 0.05)
		{
			Report.Add(FString::Printf(TEXT("LOD %d: no DNA head mesh lines up (best %.3f cm), skipped"), Lod, LodError));
			continue;
		}

		const TArray<FVector> LodBase = ReadVertices(*Base, LodDnaMesh);
		const TArray<FVector> LodVariant = ReadVertices(*Variant, LodDnaMesh);
		TArray<FVector> LodBaseMapped;
		for (const FVector& V : LodBase)
		{
			LodBaseMapped.Add(Map.Apply(V));
		}
		FGrid LodDnaGrid;
		LodDnaGrid.Build(LodBaseMapped, 0.5);

		if (!LodAttributes.GetMorphTargetNames().Contains(MorphName))
		{
			LodAttributes.RegisterMorphTargetAttribute(MorphName, false);
		}
		TVertexAttributesRef<FVector3f> LodDeltas = LodAttributes.GetVertexMorphPositionDelta(MorphName);
		const FSkinWeightsVertexAttributesRef LodWeights = LodAttributes.GetVertexSkinWeights();
		int32 LodMoved = 0;
		double LodMax = 0.0;
		for (int32 I = 0; I < LodVerts.Num(); ++I)
		{
			double Distance = 0.0;
			const int32 DnaVertex = LodDnaGrid.Nearest(LodVerts[I], Distance, 1);
			FVector Delta = FVector::ZeroVector;
			if (DnaVertex != INDEX_NONE && Distance <= 0.01 && LodVariant.IsValidIndex(DnaVertex))
			{
				Delta = Map.ApplyToVector(LodVariant[DnaVertex] - LodBase[DnaVertex]);
				for (const UE::AnimationCore::FBoneWeight& Weight : LodWeights.Get(LodIds[I]))
				{
					if (const FVector* JointDelta = JointWorldDelta.Find(Weight.GetBoneIndex()))
					{
						Delta -= *JointDelta * Weight.GetWeight();
					}
				}
			}
			LodDeltas[LodIds[I]] = FVector3f(Delta);
			LodMax = FMath::Max(LodMax, Delta.Size());
			LodMoved += Delta.SizeSquared() > FMath::Square(0.001) ? 1 : 0;
		}
		HeadMesh->CommitMeshDescription(Lod);
		Report.Add(FString::Printf(TEXT("LOD %d: DNA mesh '%s' (fit %.4f cm), %d of %d vertices moved, largest %.3f cm"),
			Lod, *Base->GetMeshName(LodDnaMesh), LodError, LodMoved, LodVerts.Num(), LodMax));
	}

	HeadMesh->PostEditChange();
	HeadMesh->MarkPackageDirty();

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	FJsonSerializer::Serialize(Json, Writer);
	FFileHelper::SaveStringToFile(JsonText, *JointOffsetsJsonPath);
	Report.Add(FString::Printf(TEXT("joint offsets written to %s"), *JointOffsetsJsonPath));

	Report.Insert(TEXT("OK"), 0);
	return FString::Join(Report, TEXT("\n"));
}

FString UVaesenSpikeSliderLibrary::InsertFaceJointOffsetsNode(UAnimBlueprint* AnimBlueprint)
{
	if (!AnimBlueprint)
	{
		return TEXT("FAILED no animation Blueprint");
	}

	// The main AnimGraph and its Output Pose (root) node.
	UEdGraph* AnimGraph = nullptr;
	for (UEdGraph* Graph : AnimBlueprint->FunctionGraphs)
	{
		if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			AnimGraph = Graph;
			break;
		}
	}
	if (!AnimGraph)
	{
		return TEXT("FAILED no AnimGraph");
	}
	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : AnimGraph->Nodes)
	{
		if (Cast<UAnimGraphNode_VaesenFaceJointOffsets>(Node))
		{
			return TEXT("OK already has a Vaesen Face Joint Offsets node");
		}
		if (UAnimGraphNode_Root* AsRoot = Cast<UAnimGraphNode_Root>(Node))
		{
			Root = AsRoot;
		}
	}
	UEdGraphPin* RootIn = Root ? Root->FindPin(TEXT("Result"), EGPD_Input) : nullptr;
	if (!RootIn)
	{
		return TEXT("FAILED no Output Pose input pin");
	}
	if (RootIn->LinkedTo.Num() != 1)
	{
		return FString::Printf(TEXT("FAILED Output Pose has %d inputs, expected 1"), RootIn->LinkedTo.Num());
	}
	UEdGraphPin* LastOut = RootIn->LinkedTo[0];
	const FString LastNode = LastOut->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString();

	// The new node between the last node and Output Pose, placed just left of Output Pose.
	FGraphNodeCreator<UAnimGraphNode_VaesenFaceJointOffsets> Creator(*AnimGraph);
	UAnimGraphNode_VaesenFaceJointOffsets* Offsets = Creator.CreateNode();
	Offsets->NodePosX = Root->NodePosX - 280;
	Offsets->NodePosY = Root->NodePosY;
	Creator.Finalize();

	UEdGraphPin* OffsetsIn = Offsets->FindPin(TEXT("Source"), EGPD_Input);
	UEdGraphPin* OffsetsOut = nullptr;
	for (UEdGraphPin* Pin : Offsets->Pins)
	{
		if (Pin->Direction == EGPD_Output)
		{
			OffsetsOut = Pin;
			break;
		}
	}
	if (!OffsetsIn || !OffsetsOut)
	{
		AnimGraph->RemoveNode(Offsets);
		return TEXT("FAILED the new node has no pose pins");
	}
	RootIn->BreakAllPinLinks();
	LastOut->MakeLinkTo(OffsetsIn);
	OffsetsOut->MakeLinkTo(RootIn);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	const bool bErrors = AnimBlueprint->Status == BS_Error;
	UEditorLoadingAndSavingUtils::SavePackages({ AnimBlueprint->GetPackage() }, false);
	return FString::Printf(TEXT("%s inserted after '%s', compiled%s, saved"), bErrors ? TEXT("FAILED") : TEXT("OK"), *LastNode, bErrors ? TEXT(" WITH ERRORS") : TEXT(""));
}
UTexture2D* UVaesenSpikeSliderLibrary::RetoneSkinTexture(UTexture2D* Source, const FString& DestPath, FVector SrcBias, FVector SrcGain, FVector DstBias, FVector DstGain)
{
	if (!Source || Source->Source.GetFormat() != TSF_BGRA8 || Source->Source.GetNumBlocks() != 1 || Source->Source.GetNumLayers() != 1 || SrcGain.GetMin() <= 0.0)
	{
		UE_LOG(LogTemp, Error, TEXT("[P0-5] Can't retone %s (needs a single 8-bit BGRA image source and positive gains; format %d, %d blocks, %d layers)"),
			*GetNameSafe(Source), Source ? (int32)Source->Source.GetFormat() : -1, Source ? Source->Source.GetNumBlocks() : 0, Source ? Source->Source.GetNumLayers() : 0);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*DestPath);
	UTexture2D* Copy = DuplicateObject<UTexture2D>(Source, Package, FName(*FPackageName::GetShortName(DestPath)));
	Copy->SetFlags(RF_Public | RF_Standalone);

	// Every mip the source has (usually just the top one; the rest are built from it).
	const bool bSRGB = Source->SRGB;
	for (int32 Mip = 0; Mip < Copy->Source.GetNumMips(); ++Mip)
	{
		uint8* Data = Copy->Source.LockMip(Mip);
		const int64 Pixels = Copy->Source.GetSizeX() * Copy->Source.GetSizeY() >> (2 * Mip);
		for (int64 P = 0; P < Pixels; ++P)
		{
			uint8* Px = Data + P * 4; // B, G, R, A
			const FColor In(Px[2], Px[1], Px[0], Px[3]);
			const FLinearColor Linear = bSRGB ? FLinearColor::FromSRGBColor(In) : FLinearColor(In.R / 255.f, In.G / 255.f, In.B / 255.f);
			FLinearColor Out = Linear;
			for (int32 C = 0; C < 3; ++C)
			{
				const double Value = C == 0 ? Linear.R : C == 1 ? Linear.G : Linear.B;
				const double Detail = (Value * 255.0 - SrcBias[C]) / SrcGain[C];
				const float NewValue = (float)FMath::Clamp((DstBias[C] + DstGain[C] * Detail) / 255.0, 0.0, 1.0);
				(C == 0 ? Out.R : C == 1 ? Out.G : Out.B) = NewValue;
			}
			const FColor Result = bSRGB ? Out.ToFColorSRGB() : Out.ToFColor(false);
			Px[0] = Result.B;
			Px[1] = Result.G;
			Px[2] = Result.R;
		}
		Copy->Source.UnlockMip(Mip);
	}
	Copy->PostEditChange();
	FAssetRegistryModule::AssetCreated(Copy);
	Copy->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Package }, false);
	return Copy;
}
bool UVaesenSpikeSliderLibrary::SplitBodyForArmour(USkeletalMesh* Body, const FString& OutFolder, const FString& Prefix)
{
	return Body && VaesenMannequinSplitter::SplitSkeletalMesh(Body, OutFolder, Prefix, /*bKeepHead*/ false);
}
FString UVaesenSpikeSliderLibrary::AddSkinToneParameters(UMaterial* Material, const TArray<FName>& ColorTextureParameters)
{
	if (!Material || !Material->bUseMaterialAttributes)
	{
		return TEXT("FAILED no material, or it doesn't use material attributes");
	}
	static const FName ScaleName(TEXT("VaesenToneScale"));
	static const FName OffsetName(TEXT("VaesenToneOffset"));
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression);
		if (Vector && Vector->ParameterName == ScaleName)
		{
			return TEXT("OK already has the skin tone parameters");
		}
	}
	// The final base colour, at the material's output: what feeds Material Attributes is broken open, its BaseColor
	// toned (colour * scale + offset) and set back. (The colour textures themselves are texture objects sampled deep in
	// functions, and the base colour is assembled from several of them, so the output is the one safe place.)
	FExpressionInput& Output = Material->GetEditorOnlyData()->MaterialAttributes;
	UMaterialExpression* Source = Output.Expression;
	const int32 SourceOutput = Output.OutputIndex;
	if (!Source)
	{
		return TEXT("FAILED nothing feeds Material Attributes");
	}

	Material->Modify();
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	auto Make = [&Collection, Material](UClass* Class, int32 X, int32 Y)
	{
		UMaterialExpression* Expression = NewObject<UMaterialExpression>(Material, Class, NAME_None, RF_Transactional);
		Collection.AddExpression(Expression);
		Expression->Material = Material;
		Expression->MaterialExpressionEditorX = Material->EditorX + X;
		Expression->MaterialExpressionEditorY = Material->EditorY + Y;
		return Expression;
	};

	UMaterialExpressionVectorParameter* Scale = Cast<UMaterialExpressionVectorParameter>(Make(UMaterialExpressionVectorParameter::StaticClass(), -900, 300));
	Scale->ParameterName = ScaleName;
	Scale->DefaultValue = FLinearColor(1, 1, 1, 1);
	Scale->Group = TEXT("Vaesen Skin Tone");
	UMaterialExpressionVectorParameter* Offset = Cast<UMaterialExpressionVectorParameter>(Make(UMaterialExpressionVectorParameter::StaticClass(), -900, 450));
	Offset->ParameterName = OffsetName;
	Offset->DefaultValue = FLinearColor(0, 0, 0, 0);
	Offset->Group = TEXT("Vaesen Skin Tone");
	auto Rgb = [&Make](UMaterialExpression* Param, int32 Y)
	{
		UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Make(UMaterialExpressionComponentMask::StaticClass(), -700, Y));
		Mask->R = Mask->G = Mask->B = 1;
		Mask->A = 0;
		Mask->Input.Connect(0, Param);
		return Mask;
	};
	UMaterialExpression* ScaleRgb = Rgb(Scale, 300);
	UMaterialExpression* OffsetRgb = Rgb(Offset, 450);

	UMaterialExpressionBreakMaterialAttributes* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(Make(UMaterialExpressionBreakMaterialAttributes::StaticClass(), -900, 0));
	Break->MaterialAttributes.Connect(SourceOutput, Source);
	UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Make(UMaterialExpressionMultiply::StaticClass(), -550, 100));
	Multiply->A.Connect(0, Break); // BaseColor is the break node's first output.
	Multiply->B.Connect(0, ScaleRgb);
	UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Make(UMaterialExpressionAdd::StaticClass(), -400, 100));
	Add->A.Connect(0, Multiply);
	Add->B.Connect(0, OffsetRgb);
	UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Make(UMaterialExpressionSetMaterialAttributes::StaticClass(), -250, 0));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->AttributeSetTypes.Add(FMaterialAttributeDefinitionMap::GetID(MP_BaseColor));
	Set->Inputs.AddDefaulted();
	Set->Inputs[1].Connect(0, Add);
	Output.Connect(0, Set);

	Material->PostEditChange();
	Material->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Material->GetPackage() }, false);
	return FString::Printf(TEXT("OK base colour toned at the output of %s"), *Material->GetName());
}
FString UVaesenSpikeSliderLibrary::AddSkinAgeLayer(UMaterial* Material, UTexture* YoungNormal, UTexture* OldNormal)
{
	if (!Material || !Material->bUseMaterialAttributes || !YoungNormal || !OldNormal)
	{
		return TEXT("FAILED no material (with material attributes), or a texture is missing");
	}
	static const FName YoungWeightName(TEXT("VaesenAgeYoung"));
	static const FName OldWeightName(TEXT("VaesenAgeOld"));
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		const UMaterialExpressionScalarParameter* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression);
		if (Scalar && Scalar->ParameterName == OldWeightName)
		{
			// A first version normalised the blended normal, which turns a material permutation without a normal (the
			// body's) into NaN, rendered black. Feed the normal attribute straight from the blend instead.
			for (UMaterialExpression* Other : Material->GetExpressions())
			{
				UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Other);
				UMaterialExpressionNormalize* Normalize = Set && Set->Inputs.Num() > 1 ? Cast<UMaterialExpressionNormalize>(Set->Inputs[1].Expression) : nullptr;
				if (Normalize && Normalize->VectorInput.Expression)
				{
					Material->Modify();
					Set->Inputs[1].Connect(Normalize->VectorInput.OutputIndex, Normalize->VectorInput.Expression);
					Material->GetExpressionCollection().RemoveExpression(Normalize);
					Material->PostEditChange();
					Material->MarkPackageDirty();
					UEditorLoadingAndSavingUtils::SavePackages({ Material->GetPackage() }, false);
					return TEXT("OK age skin layer fixed (no normalize)");
				}
			}
			return TEXT("OK already has the age skin layer");
		}
	}
	FExpressionInput& Output = Material->GetEditorOnlyData()->MaterialAttributes;
	UMaterialExpression* Source = Output.Expression;
	const int32 SourceOutput = Output.OutputIndex;
	if (!Source)
	{
		return TEXT("FAILED nothing feeds Material Attributes");
	}

	Material->Modify();
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	auto Make = [&Collection, Material](UClass* Class, int32 X, int32 Y)
	{
		UMaterialExpression* Expression = NewObject<UMaterialExpression>(Material, Class, NAME_None, RF_Transactional);
		Collection.AddExpression(Expression);
		Expression->Material = Material;
		Expression->MaterialExpressionEditorX = Material->EditorX + X;
		Expression->MaterialExpressionEditorY = Material->EditorY + Y;
		return Expression;
	};
	auto Weight = [&Make](FName Name, int32 Y)
	{
		UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(Make(UMaterialExpressionScalarParameter::StaticClass(), -600, Y));
		Param->ParameterName = Name;
		Param->DefaultValue = 0.f;
		Param->Group = TEXT("Vaesen Age");
		return Param;
	};
	auto Sample = [&Make](FName Name, UTexture* Texture, int32 Y)
	{
		UMaterialExpressionTextureSampleParameter2D* Param = Cast<UMaterialExpressionTextureSampleParameter2D>(Make(UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, Y));
		Param->ParameterName = Name;
		Param->Texture = Texture;
		Param->SamplerType = Texture->VirtualTextureStreaming ? SAMPLERTYPE_VirtualNormal : SAMPLERTYPE_Normal;
		Param->Group = TEXT("Vaesen Age");
		return Param;
	};

	UMaterialExpressionBreakMaterialAttributes* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(Make(UMaterialExpressionBreakMaterialAttributes::StaticClass(), -600, 700));
	Break->MaterialAttributes.Connect(SourceOutput, Source);
	const int32 NormalOutput = Break->GetOutputs().IndexOfByPredicate([](const FExpressionOutput& Out) { return Out.OutputName == TEXT("Normal"); });
	if (NormalOutput == INDEX_NONE)
	{
		return TEXT("FAILED the Break Material Attributes node has no Normal output");
	}
	UMaterialExpressionLinearInterpolate* ToYoung = Cast<UMaterialExpressionLinearInterpolate>(Make(UMaterialExpressionLinearInterpolate::StaticClass(), -350, 800));
	ToYoung->A.Connect(NormalOutput, Break);
	ToYoung->B.Connect(0, Sample(TEXT("VaesenAgeYoungNormal"), YoungNormal, 850));
	ToYoung->Alpha.Connect(0, Weight(YoungWeightName, 1100));
	UMaterialExpressionLinearInterpolate* ToOld = Cast<UMaterialExpressionLinearInterpolate>(Make(UMaterialExpressionLinearInterpolate::StaticClass(), -200, 900));
	ToOld->A.Connect(0, ToYoung);
	ToOld->B.Connect(0, Sample(TEXT("VaesenAgeOldNormal"), OldNormal, 1200));
	ToOld->Alpha.Connect(0, Weight(OldWeightName, 1450));

	UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Make(UMaterialExpressionSetMaterialAttributes::StaticClass(), -50, 700));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->AttributeSetTypes.Add(FMaterialAttributeDefinitionMap::GetID(MP_Normal));
	Set->Inputs.AddDefaulted();
	// Not normalised here: the engine normalises the material's normal, and a permutation without one stays zero.
	Set->Inputs[1].Connect(0, ToOld);
	Output.Connect(0, Set);

	Material->PostEditChange();
	Material->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Material->GetPackage() }, false);
	return FString::Printf(TEXT("OK age skin layer added to %s"), *Material->GetName());
}

FString UVaesenSpikeSliderLibrary::AddSkinToneFloor(UMaterial* Material)
{
	if (!Material)
	{
		return TEXT("FAILED no material");
	}
	// The tone: Set(BaseColor = Add(Multiply(Break.BaseColor, ScaleRgb), OffsetRgb)).
	UMaterialExpressionSetMaterialAttributes* ToneSet = nullptr;
	UMaterialExpressionAdd* ToneAdd = nullptr;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression); Vector && Vector->ParameterName == TEXT("VaesenToneRatio"))
		{
			return TEXT("OK already has the skin tone floor");
		}
		UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expression);
		UMaterialExpressionAdd* Add = Set && Set->Inputs.Num() > 1 ? Cast<UMaterialExpressionAdd>(Set->Inputs[1].Expression) : nullptr;
		const UMaterialExpressionComponentMask* Mask = Add ? Cast<UMaterialExpressionComponentMask>(Add->B.Expression) : nullptr;
		const UMaterialExpressionVectorParameter* Offset = Mask ? Cast<UMaterialExpressionVectorParameter>(Mask->Input.Expression) : nullptr;
		if (Offset && Offset->ParameterName == TEXT("VaesenToneOffset"))
		{
			ToneSet = Set;
			ToneAdd = Add;
		}
	}
	UMaterialExpressionMultiply* ToneMultiply = ToneAdd ? Cast<UMaterialExpressionMultiply>(ToneAdd->A.Expression) : nullptr;
	if (!ToneSet || !ToneMultiply || !ToneMultiply->A.Expression)
	{
		return TEXT("FAILED no skin tone insertion found (run AddSkinToneParameters first)");
	}

	Material->Modify();
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	auto Make = [&Collection, Material](UClass* Class, int32 X, int32 Y)
	{
		UMaterialExpression* Expression = NewObject<UMaterialExpression>(Material, Class, NAME_None, RF_Transactional);
		Collection.AddExpression(Expression);
		Expression->Material = Material;
		Expression->MaterialExpressionEditorX = Material->EditorX + X;
		Expression->MaterialExpressionEditorY = Material->EditorY + Y;
		return Expression;
	};
	UMaterialExpressionVectorParameter* Ratio = Cast<UMaterialExpressionVectorParameter>(Make(UMaterialExpressionVectorParameter::StaticClass(), -900, 600));
	Ratio->ParameterName = TEXT("VaesenToneRatio");
	Ratio->DefaultValue = FLinearColor(1, 1, 1, 1);
	Ratio->Group = TEXT("Vaesen Skin Tone");
	UMaterialExpressionComponentMask* RatioRgb = Cast<UMaterialExpressionComponentMask>(Make(UMaterialExpressionComponentMask::StaticClass(), -700, 600));
	RatioRgb->R = RatioRgb->G = RatioRgb->B = 1;
	RatioRgb->A = 0;
	RatioRgb->Input.Connect(0, Ratio);
	UMaterialExpressionMultiply* Floor = Cast<UMaterialExpressionMultiply>(Make(UMaterialExpressionMultiply::StaticClass(), -550, 250));
	Floor->A.Connect(ToneMultiply->A.OutputIndex, ToneMultiply->A.Expression);
	Floor->B.Connect(0, RatioRgb);
	UMaterialExpressionMax* Max = Cast<UMaterialExpressionMax>(Make(UMaterialExpressionMax::StaticClass(), -320, 150));
	Max->A.Connect(0, ToneAdd);
	Max->B.Connect(0, Floor);
	ToneSet->Inputs[1].Connect(0, Max);

	Material->PostEditChange();
	Material->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Material->GetPackage() }, false);
	return FString::Printf(TEXT("OK skin tone floor added to %s"), *Material->GetName());
}

FString UVaesenSpikeSliderLibrary::AddSkinAgeColor(UMaterial* Material, UTexture* YoungColor, UTexture* OldColor)
{
	if (!Material || !YoungColor || !OldColor)
	{
		return TEXT("FAILED no material, or a texture is missing");
	}
	// The tone's break node: what the tone multiply reads.
	UMaterialExpressionBreakMaterialAttributes* ToneBreak = nullptr;
	UMaterialExpressionSetMaterialAttributes* ToneSet = nullptr;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (const UMaterialExpressionTextureSampleParameter2D* Texture = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
		{
			if (Texture->ParameterName == TEXT("VaesenAgeOldColor"))
			{
				return TEXT("OK already has the age skin colour");
			}
		}
		if (const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression); Vector && Vector->ParameterName == TEXT("VaesenToneScale"))
		{
			for (UMaterialExpression* Other : Material->GetExpressions())
			{
				UMaterialExpressionMultiply* Multiply = Cast<UMaterialExpressionMultiply>(Other);
				const UMaterialExpressionComponentMask* Mask = Multiply ? Cast<UMaterialExpressionComponentMask>(Multiply->B.Expression) : nullptr;
				if (Mask && Mask->Input.Expression == Vector)
				{
					ToneBreak = Cast<UMaterialExpressionBreakMaterialAttributes>(Multiply->A.Expression);
				}
			}
		}
	}
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expression);
		if (Set && ToneBreak && Set->Inputs.Num() > 0 && Set->Inputs[0].Expression == ToneBreak->MaterialAttributes.Expression
			&& Set->AttributeSetTypes.Contains(FMaterialAttributeDefinitionMap::GetID(MP_BaseColor)))
		{
			ToneSet = Set;
		}
	}
	if (!ToneBreak || !ToneSet)
	{
		return TEXT("FAILED no skin tone insertion found (run AddSkinToneParameters first)");
	}
	UMaterialExpression* const Source = ToneBreak->MaterialAttributes.Expression;
	const int32 SourceOutput = ToneBreak->MaterialAttributes.OutputIndex;

	Material->Modify();
	FMaterialExpressionCollection& Collection = Material->GetExpressionCollection();
	auto Make = [&Collection, Material](UClass* Class, int32 X, int32 Y)
	{
		UMaterialExpression* Expression = NewObject<UMaterialExpression>(Material, Class, NAME_None, RF_Transactional);
		Collection.AddExpression(Expression);
		Expression->Material = Material;
		Expression->MaterialExpressionEditorX = Material->EditorX + X;
		Expression->MaterialExpressionEditorY = Material->EditorY + Y;
		return Expression;
	};
	auto Weight = [&Make](FName Name, int32 Y)
	{
		UMaterialExpressionScalarParameter* Param = Cast<UMaterialExpressionScalarParameter>(Make(UMaterialExpressionScalarParameter::StaticClass(), -1500, Y));
		Param->ParameterName = Name;
		Param->DefaultValue = 0.f;
		Param->Group = TEXT("Vaesen Age");
		return Param;
	};
	auto Sample = [&Make](FName Name, UTexture* Texture, int32 Y)
	{
		UMaterialExpressionTextureSampleParameter2D* Param = Cast<UMaterialExpressionTextureSampleParameter2D>(Make(UMaterialExpressionTextureSampleParameter2D::StaticClass(), -1500, Y));
		Param->ParameterName = Name;
		Param->Texture = Texture;
		Param->SamplerType = Texture->VirtualTextureStreaming ? SAMPLERTYPE_VirtualColor : SAMPLERTYPE_Color;
		Param->Group = TEXT("Vaesen Age");
		return Param;
	};
	// Same-named parameters are one parameter: these weights are the normal layer's.
	UMaterialExpressionBreakMaterialAttributes* Break = Cast<UMaterialExpressionBreakMaterialAttributes>(Make(UMaterialExpressionBreakMaterialAttributes::StaticClass(), -1500, -300));
	Break->MaterialAttributes.Connect(SourceOutput, Source);
	UMaterialExpressionLinearInterpolate* ToYoung = Cast<UMaterialExpressionLinearInterpolate>(Make(UMaterialExpressionLinearInterpolate::StaticClass(), -1250, -250));
	ToYoung->A.Connect(0, Break); // BaseColor is the break node's first output.
	ToYoung->B.Connect(0, Sample(TEXT("VaesenAgeYoungColor"), YoungColor, -200));
	ToYoung->Alpha.Connect(0, Weight(TEXT("VaesenAgeYoung"), 50));
	UMaterialExpressionLinearInterpolate* ToOld = Cast<UMaterialExpressionLinearInterpolate>(Make(UMaterialExpressionLinearInterpolate::StaticClass(), -1100, -200));
	ToOld->A.Connect(0, ToYoung);
	ToOld->B.Connect(0, Sample(TEXT("VaesenAgeOldColor"), OldColor, 150));
	ToOld->Alpha.Connect(0, Weight(TEXT("VaesenAgeOld"), 400));
	UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Make(UMaterialExpressionSetMaterialAttributes::StaticClass(), -1000, -300));
	Set->Inputs[0].Connect(SourceOutput, Source);
	Set->AttributeSetTypes.Add(FMaterialAttributeDefinitionMap::GetID(MP_BaseColor));
	Set->Inputs.AddDefaulted();
	Set->Inputs[1].Connect(0, ToOld);
	// The tone now reads the aged colour.
	ToneBreak->MaterialAttributes.Connect(0, Set);
	ToneSet->Inputs[0].Connect(0, Set);

	Material->PostEditChange();
	Material->MarkPackageDirty();
	UEditorLoadingAndSavingUtils::SavePackages({ Material->GetPackage() }, false);
	return FString::Printf(TEXT("OK age skin colour added to %s, before the tone"), *Material->GetName());
}

FString UVaesenSpikeSliderLibrary::RemoveSkinToneParameters(UMaterial* Material)
{
	if (!Material)
	{
		return TEXT("FAILED no material");
	}
	static const FName ScaleName(TEXT("VaesenToneScale"));
	static const FName OffsetName(TEXT("VaesenToneOffset"));

	// The material and every function it uses.
	TArray<TPair<UObject*, UMaterialFunction*>> Containers;
	TSet<UObject*> Seen;
	TFunction<void(UObject*, UMaterialFunction*)> Visit = [&](UObject* Owner, UMaterialFunction* Function)
	{
		if (Seen.Contains(Owner))
		{
			return;
		}
		Seen.Add(Owner);
		Containers.Emplace(Owner, Function);
		for (UMaterialExpression* Expression : Function ? Function->GetExpressions() : Material->GetExpressions())
		{
			if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
			{
				if (UMaterialFunction* Called = Cast<UMaterialFunction>(Call->MaterialFunction))
				{
					Visit(Called, Called);
				}
			}
		}
	};
	Visit(Material, nullptr);

	TArray<FString> Report;
	TArray<UPackage*> ToSave;
	for (const TPair<UObject*, UMaterialFunction*>& Container : Containers)
	{
		UMaterialFunction* Function = Container.Value;
		TArray<UMaterialExpression*> Expressions;
		for (UMaterialExpression* Expression : Function ? Function->GetExpressions() : Material->GetExpressions())
		{
			Expressions.Add(Expression);
		}

		// The inserted parameters and their masks.
		TSet<UMaterialExpression*> Remove;
		for (UMaterialExpression* Expression : Expressions)
		{
			const UMaterialExpressionVectorParameter* Vector = Cast<UMaterialExpressionVectorParameter>(Expression);
			if (Vector && (Vector->ParameterName == ScaleName || Vector->ParameterName == OffsetName))
			{
				Remove.Add(Expression);
			}
		}
		if (Remove.IsEmpty())
		{
			continue;
		}
		for (UMaterialExpression* Expression : Expressions)
		{
			const UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression);
			if (Mask && Remove.Contains(Mask->Input.Expression))
			{
				Remove.Add(Expression);
			}
		}

		// Each inserted Add (B from an offset mask) with its Multiply (B from a scale mask): reconnect the Add's readers
		// to what the Multiply read.
		int32 Restored = 0;
		for (UMaterialExpression* Expression : Expressions)
		{
			UMaterialExpressionAdd* Add = Cast<UMaterialExpressionAdd>(Expression);
			UMaterialExpressionMultiply* Multiply = Add ? Cast<UMaterialExpressionMultiply>(Add->A.Expression) : nullptr;
			if (!Add || !Multiply || !Remove.Contains(Add->B.Expression) || !Remove.Contains(Multiply->B.Expression))
			{
				continue;
			}
			UMaterialExpression* Original = Multiply->A.Expression;
			const int32 OriginalOutput = Multiply->A.OutputIndex;
			for (UMaterialExpression* Reader : Expressions)
			{
				if (!Reader || Reader == Add || Reader == Multiply || Remove.Contains(Reader))
				{
					continue;
				}
				for (FExpressionInputIterator It{ Reader }; It; ++It)
				{
					if (It->Expression == Add)
					{
						It->Connect(OriginalOutput, Original);
						++Restored;
					}
				}
			}
			Remove.Add(Add);
			Remove.Add(Multiply);
		}

		Container.Key->Modify();
		FMaterialExpressionCollection& Collection = Function ? Function->GetExpressionCollection() : Material->GetExpressionCollection();
		for (UMaterialExpression* Expression : Remove)
		{
			Collection.RemoveExpression(Expression);
			Expression->MarkAsGarbage();
		}
		if (Function)
		{
			Function->PostEditChange();
		}
		Container.Key->MarkPackageDirty();
		ToSave.AddUnique(Container.Key->GetPackage());
		Report.Add(FString::Printf(TEXT("%s: %d inputs restored, %d nodes removed"), *Container.Key->GetName(), Restored, Remove.Num()));
	}

	// The output insertion (AddSkinToneParameters): Material Attributes fed by a Set node whose BaseColor comes from the
	// toned colour; feed the output from what the Set node was given instead, and remove the Set and Break nodes.
	FExpressionInput& Output = Material->GetEditorOnlyData()->MaterialAttributes;
	if (UMaterialExpressionSetMaterialAttributes* Set = Cast<UMaterialExpressionSetMaterialAttributes>(Output.Expression))
	{
		const UMaterialExpressionAdd* Add = Set->Inputs.Num() > 1 ? Cast<UMaterialExpressionAdd>(Set->Inputs[1].Expression) : nullptr;
		const UMaterialExpressionMultiply* Multiply = Add ? Cast<UMaterialExpressionMultiply>(Add->A.Expression) : nullptr;
		UMaterialExpressionBreakMaterialAttributes* Break = Multiply ? Cast<UMaterialExpressionBreakMaterialAttributes>(Multiply->A.Expression) : nullptr;
		if (Break)
		{
			Material->Modify();
			Output.Connect(Set->Inputs[0].OutputIndex, Set->Inputs[0].Expression);
			Material->GetExpressionCollection().RemoveExpression(Set);
			Material->GetExpressionCollection().RemoveExpression(Break);
			Set->MarkAsGarbage();
			Break->MarkAsGarbage();
			ToSave.AddUnique(Material->GetPackage());
			Report.Add(TEXT("output restored"));
		}
	}

	if (ToSave.IsEmpty())
	{
		return TEXT("OK nothing to remove");
	}
	Material->PostEditChange();
	Material->MarkPackageDirty();
	ToSave.AddUnique(Material->GetPackage());
	UEditorLoadingAndSavingUtils::SavePackages(ToSave, false);
	Report.Insert(TEXT("OK"), 0);
	return FString::Join(Report, TEXT("\n"));
}