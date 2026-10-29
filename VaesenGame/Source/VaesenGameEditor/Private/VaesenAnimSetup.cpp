#include "VaesenAnimSetup.h"

#include "AnimPose.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/PackageName.h"
#include "RetargetEditor/IKRetargetBatchOperation.h"
#include "RetargetEditor/IKRetargeterController.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PinBoneOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "Rig/IKRigDefinition.h"
#include "RigEditor/IKRigAutoCharacterizer.h"
#include "RigEditor/IKRigAutoFBIK.h"
#include "RigEditor/IKRigController.h"
#include "UObject/SavePackage.h"
#include "VaesenAnimSetupCommandlet.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_TwoBoneIK.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_VaesenShoulderAim.h"
#include "Anim/VaesenAnimInstance.h"
#include "Anim/VaesenAnimNotifies.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AnimationRuntime.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "EdGraphSchema_K2.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Weapons/VaesenWeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenAnimSetup, Log, All);

namespace VaesenAnimSetup
{
	const TCHAR* RifleFolder = TEXT("/Game/Vaesen/Animations/Rifle");
	const TCHAR* RigFolder = TEXT("/Game/Vaesen/Animations/Retargeting");
	const TCHAR* RiflePackFolder = TEXT("/Game/RifleAnims/Animations");
	// The rifle pack's UE4 mannequin (the retargeting source), kept in the project (the pack itself isn't in git).
	const TCHAR* UE4MannequinMesh = TEXT("/Game/Vaesen/ThirdParty/RifleAnims/ShowcaseAssets/Meshes/Mannequin/SK_Mannequin.SK_Mannequin");
	const TCHAR* MannyMesh = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny");

	bool Exists(const FString& PackagePath)
	{
		return FPackageName::DoesPackageExist(PackagePath);
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, Args);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *FileName);
		return bSaved;
	}

	template <typename T>
	T* NewAsset(const FString& PackagePath)
	{
		UPackage* Package = CreatePackage(*PackagePath);
		T* Asset = NewObject<T>(Package, FName(FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	template <typename T>
	T* LoadAt(const FString& PackagePath)
	{
		return LoadObject<T>(nullptr, *(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** An IK rig with auto-generated retarget chains and full-body IK for Mesh (as the editor's auto-retarget makes them). */
	UIKRigDefinition* MakeIKRig(const FString& PackagePath, USkeletalMesh* Mesh, FAutoCharacterizeResults& OutCharacterization)
	{
		UIKRigDefinition* Rig = LoadAt<UIKRigDefinition>(PackagePath);
		const bool bNew = !Rig;
		if (bNew)
		{
			Rig = NewAsset<UIKRigDefinition>(PackagePath);
		}
		const UIKRigController* Controller = UIKRigController::GetController(Rig);
		Controller->SetSkeletalMesh(Mesh);
		Controller->AutoGenerateRetargetDefinition(OutCharacterization);
		if (bNew)
		{
			Controller->SetRetargetDefinition(OutCharacterization.AutoRetargetDefinition.RetargetDefinition);
			FAutoFBIKResults IKResults;
			Controller->AutoGenerateFBIK(IKResults);
			Save(Rig);
		}
		return Rig;
	}

	/** The UE4 mannequin -> UE5 Manny retargeter (auto-generated like the editor's Retarget Animations window). */
	UIKRetargeter* MakeRetargeter(USkeletalMesh* SourceMesh, USkeletalMesh* TargetMesh)
	{
		const FString Path = FString(RigFolder) / TEXT("RTG_UE4Mannequin_To_Manny");
		if (UIKRetargeter* Existing = LoadAt<UIKRetargeter>(Path))
		{
			return Existing;
		}

		FAutoCharacterizeResults SourceCharacterization, TargetCharacterization;
		UIKRigDefinition* SourceRig = MakeIKRig(FString(RigFolder) / TEXT("IK_UE4Mannequin"), SourceMesh, SourceCharacterization);
		UIKRigDefinition* TargetRig = MakeIKRig(FString(RigFolder) / TEXT("IK_Manny_Auto"), TargetMesh, TargetCharacterization);

		UIKRetargeter* Retargeter = NewAsset<UIKRetargeter>(Path);
		const UIKRetargeterController* Controller = UIKRetargeterController::GetController(Retargeter);
		{
			FScopedReinitializeIKRetargeter Reinitialize(Controller, ERetargetRefreshMode::ProcessorAndOpStack);
			Controller->SetIKRig(ERetargetSourceOrTarget::Source, SourceRig);
			Controller->SetIKRig(ERetargetSourceOrTarget::Target, TargetRig);
			Controller->RemoveAllOps();
			Controller->AddDefaultOps();

			// Exact chain names only (fuzzy can map toes to fingers).
			if (FIKRetargetFKChainsOp* FKChains = Controller->GetFirstRetargetOpOfType<FIKRetargetFKChainsOp>())
			{
				FKChains->Settings.ChainMapping.AutoMapChains(EAutoMapChainType::Exact, true);
			}

			const FName TargetPose = Controller->GetCurrentRetargetPoseName(ERetargetSourceOrTarget::Target);
			Controller->ResetRetargetPose(TargetPose, TArray<FName>(), ERetargetSourceOrTarget::Target);
			Controller->AutoAlignAllBones(ERetargetSourceOrTarget::Target);
			const TArray<FName>& Excluded = TargetCharacterization.AutoRetargetDefinition.BonesToExcludeFromAutoPose;
			if (!Excluded.IsEmpty())
			{
				Controller->ResetRetargetPose(TargetPose, Excluded, ERetargetSourceOrTarget::Target);
			}

			if (FIKRetargetRunIKRigOp* RunIK = Controller->GetFirstRetargetOpOfType<FIKRetargetRunIKRigOp>())
			{
				RunIK->SetEnabled(false);
			}

			// Both mannequins have a root bone above the pelvis: copy root motion from the source root.
			if (FIKRetargetRootMotionOp* Root = Controller->GetFirstRetargetOpOfType<FIKRetargetRootMotionOp>())
			{
				const FName TargetRoot = TargetRig->GetSkeleton().BoneNames[0];
				const FName TargetPelvis = TargetCharacterization.AutoRetargetDefinition.RetargetDefinition.PelvisBone;
				Root->SetEnabled(TargetRoot != TargetPelvis);
			}

			// The IK bones (ik_hand_gun, ik_hand_r, ik_foot_l...) follow the bones they belong to.
			Controller->AddRetargetOp(FIKRetargetPinBoneOp::StaticStruct());
			if (FIKRetargetPinBoneOp* Pin = Controller->GetFirstRetargetOpOfType<FIKRetargetPinBoneOp>())
			{
				Pin->Settings.TranslationMode = EPinBoneTranslationMode::CopyGlobalPosition;
				Pin->Settings.RotationMode = EPinBoneRotationMode::CopyGlobalRotation;
				for (const FBoneToPin& Bones : TargetCharacterization.AutoRetargetDefinition.BonesToPin.GetBonesToPin())
				{
					FPinBoneData Data;
					Data.BoneToCopyFrom.BoneName = Bones.BoneToPinTo;
					Data.BoneToCopyTo.BoneName = Bones.BoneToPin;
					Pin->Settings.BonesToPin.Add(Data);
				}
			}
		}
		Save(Retargeter);
		return Retargeter;
	}

	bool RetargetRiflePack()
	{
		const FString Marker = FString(RifleFolder) / TEXT("BS_Rifle");
		if (Exists(Marker))
		{
			return true;
		}
		USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, UE4MannequinMesh);
		USkeletalMesh* Target = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
		if (!Source || !Target)
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Rifle pack (/Game/RifleAnims) not in the project; skipping the rifle."));
			return false;
		}

		UIKRetargeter* Retargeter = MakeRetargeter(Source, Target);

		// Everything under Animations except the unused 2D aim offsets; blend spaces bring their sequences along.
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Found;
		Registry.GetAssetsByPath(FName(RiflePackFolder), Found, true);
		FIKRetargetBatchOperationInputs Inputs;
		for (const FAssetData& Asset : Found)
		{
			const FString Path = Asset.PackagePath.ToString();
			const bool bAnimation = Asset.IsInstanceOf(UAnimSequence::StaticClass()) || Asset.IsInstanceOf(UBlendSpace::StaticClass()) || Asset.IsInstanceOf(UAnimMontage::StaticClass());
			if (bAnimation && !Path.Contains(TEXT("NotUsed")))
			{
				Inputs.AssetsToRetarget.Add(Asset);
			}
		}
		Inputs.SourceMesh = Source;
		Inputs.TargetMesh = Target;
		Inputs.IKRetargetAsset = Retargeter;
		Inputs.TargetPath = RifleFolder;
		Inputs.bUseSourcePath = false;
		Inputs.bIncludeReferencedAssets = true;
		Inputs.bOverwriteExistingFiles = true;
		const TArray<FAssetData> Results = UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Retargeted %d of %d rifle assets to Manny."), Results.Num(), Inputs.AssetsToRetarget.Num());

		// The batch leaves them in memory; write them out.
		for (const FAssetData& Result : Results)
		{
			if (UObject* Asset = Result.GetAsset())
			{
				Save(Asset);
			}
		}
		return Exists(Marker);
	}

	FString Describe(const FTransform& T)
	{
		return FString::Printf(TEXT("loc %s rot %s"), *T.GetLocation().ToCompactString(), *T.Rotator().ToCompactString());
	}

	void LogPose(const UAnimSequenceBase* Anim, double Time)
	{
		FAnimPose Pose;
		FAnimPoseEvaluationOptions Options;
		UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Time, Options, Pose);
		for (const TCHAR* Bone : { TEXT("hand_r"), TEXT("hand_l"), TEXT("ik_hand_gun"), TEXT("head"), TEXT("spine_05"), TEXT("pelvis") })
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("    t=%.2f %-12s world %s"), Time, Bone, *Describe(UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::World)));
		}
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("    t=%.2f hand_l relative to hand_r: %s"), Time,
			*Describe(UAnimPoseExtensions::GetRelativeTransform(Pose, TEXT("hand_r"), TEXT("hand_l"), EAnimPoseSpaces::World)));
	}

	FAnimPose PoseAt(const UAnimSequenceBase* Anim, double Time);

	/**
	 * -analyzebow: how the GhostSamurai pack rigs its bow: its character Blueprint's components (what's attached where),
	 * the bow's and arrow's bones, and through the key animations where the weapon bone (ik_hand_gun) is from each hand,
	 * how far the hands are apart (the draw), and their curves and notifies.
	 */
	void AnalyzeBow()
	{
		const FString Pack = TEXT("/Game/GhostSamurai_Bundle/GhostSamurai_Bundle_UE5");
		if (const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *(Pack / TEXT("Blueprints/GhostSamurai_Bow.GhostSamurai_Bow"))))
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Blueprint %s (parent %s)"), *Blueprint->GetName(), *GetNameSafe(Blueprint->ParentClass));
			if (Blueprint->SimpleConstructionScript)
			{
				for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
				{
					const USceneComponent* Scene = Cast<USceneComponent>(Node->ComponentTemplate);
					FString Asset;
					if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(Node->ComponentTemplate))
					{
						Asset = GetPathNameSafe(Static->GetStaticMesh());
					}
					else if (const USkeletalMeshComponent* Skinned = Cast<USkeletalMeshComponent>(Node->ComponentTemplate))
					{
						Asset = GetPathNameSafe(Skinned->GetSkeletalMeshAsset()) + TEXT(" anim ") + GetPathNameSafe(const_cast<USkeletalMeshComponent*>(Skinned)->GetAnimClass());
					}
					UE_LOG(LogVaesenAnimSetup, Display, TEXT("  %s [%s] on %s/%s socket %s  %s  %s"), *Node->GetVariableName().ToString(), *GetNameSafe(Node->ComponentClass),
						*Node->ParentComponentOrVariableName.ToString(), *GetNameSafe(Node->GetParentComponentTemplate(const_cast<UBlueprint*>(Blueprint))), *Node->AttachToName.ToString(),
						Scene ? *Describe(Scene->GetRelativeTransform()) : TEXT("-"), *Asset);
				}
			}
			if (const ACharacter* Defaults = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("  character mesh %s anim %s"), *GetPathNameSafe(Defaults->GetMesh()->GetSkeletalMeshAsset()), *GetPathNameSafe(Defaults->GetMesh()->GetAnimClass()));
			}
		}
		for (const TCHAR* Mesh : { TEXT("Weapon/Mesh/Bow/SM_Bow_01.SM_Bow_01"), TEXT("Weapon/Mesh/Bow/SM_Bow_02.SM_Bow_02"), TEXT("Weapon/Mesh/Bow/SM_Arrow_01.SM_Arrow_01"),
			TEXT("Weapon/Mesh/Bow/SM_Quiver_01.SM_Quiver_01") })
		{
			if (const UStaticMesh* Static = LoadObject<UStaticMesh>(nullptr, *(Pack / Mesh)))
			{
				const FBox Box = Static->GetBoundingBox();
				FString Sockets;
				for (const UStaticMeshSocket* Socket : Static->Sockets)
				{
					Sockets += Socket->SocketName.ToString() + TEXT(" ");
				}
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s box %s .. %s  sockets: %s"), *Static->GetName(), *Box.Min.ToCompactString(), *Box.Max.ToCompactString(), *Sockets);
			}
		}
		if (const USkeleton* PackSkeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin")))
		{
			for (const USkeletalMeshSocket* Socket : PackSkeleton->Sockets)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pack skeleton socket %s on %s: %s"), *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
					*Describe(Socket->GetSocketLocalTransform()));
			}
		}
		for (const TCHAR* Mesh : { TEXT("Weapon/Mesh/Bow/SK_Bow_01.SK_Bow_01"), TEXT("Weapon/Mesh/Bow/SK_Arrow_01.SK_Arrow_01") })
		{
			if (const USkeletalMesh* Skinned = LoadObject<USkeletalMesh>(nullptr, *(Pack / Mesh)))
			{
				const FReferenceSkeleton& Ref = Skinned->GetRefSkeleton();
				FString Bones;
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					// Only the extra bones (the bow, string, arrow, weapons), with their parents.
					const FString Name = Ref.GetBoneName(Bone).ToString();
					if (Name.Contains(TEXT("bow")) || Name.Contains(TEXT("String")) || Name.Contains(TEXT("arrow")) || Name.StartsWith(TEXT("weapon"), ESearchCase::IgnoreCase))
					{
						Bones += FString::Printf(TEXT("%s<-%s(%s) "), *Name, *Ref.GetBoneName(FMath::Max(Ref.GetParentIndex(Bone), 0)).ToString(),
							*FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Bone).GetLocation().ToCompactString());
					}
				}
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s [skeleton %s] bounds %s: %s"), *Skinned->GetName(), *GetPathNameSafe(Skinned->GetSkeleton()),
					*Skinned->GetBounds().BoxExtent.ToCompactString(), *Bones);
			}
		}
		for (const TCHAR* Name : { TEXT("Bow/Idle_Root"), TEXT("Bow/Common/Common_Idle_Root"), TEXT("Bow/Common/EquipAndUnarm/Common_Equip_Root"),
			TEXT("Bow/Common/EquipAndUnarm/Common_Unarm_Root"), TEXT("Bow/Attack/Shoot_Start_Root"), TEXT("Bow/Attack/Shoot_Aim_Idle_Root"),
			TEXT("Bow/Attack/Shoot_End_Root"), TEXT("Bow/Attack/Shoot_Loop_Root"), TEXT("Bow/Attack/Shoot_Cancel_Root"), TEXT("Bow/Attack/AimPose/Shoot_AimPose_F") })
		{
			const FString Short = FPaths::GetBaseFilename(Name);
			const FString Path = Pack / FPaths::GetPath(Name) / TEXT("GhostSamurai_Bow_") + Short + TEXT(".GhostSamurai_Bow_") + Short;
			const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *Path);
			if (!Anim)
			{
				UE_LOG(LogVaesenAnimSetup, Warning, TEXT("No %s"), *Path);
				continue;
			}
			FString Curves;
			for (const FFloatCurve& Curve : Anim->GetDataModel()->GetFloatCurves())
			{
				Curves += Curve.GetName().ToString() + TEXT(" ");
			}
			FString Notifies;
			for (const FAnimNotifyEvent& Event : Anim->Notifies)
			{
				Notifies += FString::Printf(TEXT("%s@%.2f "), *Event.NotifyName.ToString(), Event.GetTriggerTime());
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s [%s] %.2f s  curves: %s  notifies: %s"), *Anim->GetName(), *GetPathNameSafe(Anim->GetSkeleton()), Anim->GetPlayLength(), *Curves, *Notifies);
			const int32 Steps = 8;
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const double Time = Anim->GetPlayLength() * Step / Steps;
				const FAnimPose Pose = PoseAt(Anim, Time);
				const FTransform Gun = UAnimPoseExtensions::GetBonePose(Pose, TEXT("ik_hand_gun"), EAnimPoseSpaces::World);
				const FTransform Left = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_l"), EAnimPoseSpaces::World);
				const FTransform Right = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World);
				const FTransform Bow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("bow"), EAnimPoseSpaces::World);
				const FTransform Arrow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("arrow"), EAnimPoseSpaces::World);
				const FTransform String = UAnimPoseExtensions::GetBonePose(Pose, TEXT("String"), EAnimPoseSpaces::World);
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("   t=%.2f  hands apart %.1f | bow from hand_l %s | arrow from hand_r %s scale %s | string from bow %s"), Time,
					FVector::Dist(Left.GetLocation(), Right.GetLocation()), *Describe(Bow.GetRelativeTransform(Left)), *Describe(Arrow.GetRelativeTransform(Right)),
					*Arrow.GetScale3D().ToCompactString(), *String.GetRelativeTransform(Bow).GetLocation().ToCompactString());
			}
		}
	}

	/**
	 * -analyzefolder=/Game/...: every animation there, against Manny's plain idle (MM_Idle): its skeleton, length, how
	 * far the root travels, and through it the hands (in the body's frame: ahead, to his right, up from the pelvis) and
	 * how closed each fist is (the index finger's curl), to judge whether they suit other characters and weapons.
	 */
	void AnalyzeFolder(const FString& Folder)
	{
		TArray<FAssetData> Assets;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*Folder), Assets, true);
		const auto Report = [](const UAnimSequence* Anim)
		{
			const double Length = Anim->GetPlayLength();
			const FAnimPose Start = PoseAt(Anim, 0.0);
			const FAnimPose End = PoseAt(Anim, Length);
			const FVector Travel = UAnimPoseExtensions::GetBonePose(End, TEXT("root"), EAnimPoseSpaces::World).GetLocation()
				- UAnimPoseExtensions::GetBonePose(Start, TEXT("root"), EAnimPoseSpaces::World).GetLocation();
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s  [%s] %.2f s, root travels %s (Manny: +Y ahead, -X his right)"), *Anim->GetName(),
				*GetNameSafe(Anim->GetSkeleton()), Length, *Travel.ToCompactString());
			for (const double Share : { 0.0, 0.3, 0.6, 1.0 })
			{
				const FAnimPose Pose = PoseAt(Anim, Length * Share);
				const FTransform Pelvis = UAnimPoseExtensions::GetBonePose(Pose, TEXT("pelvis"), EAnimPoseSpaces::World);
				const auto Hand = [&](const TCHAR* Bone)
				{
					const FVector At = UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::World).GetLocation() - Pelvis.GetLocation();
					return FVector(At.Y, -At.X, At.Z);
				};
				// The index finger's curl: its middle joint's bend (degrees), 0 open.
				const auto Curl = [&](const TCHAR* Bone)
				{
					return UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::Local).Rotator().GetManhattanDistance(FRotator::ZeroRotator);
				};
				const FVector Spine = UAnimPoseExtensions::GetBonePose(Pose, TEXT("spine_05"), EAnimPoseSpaces::World).GetLocation() - Pelvis.GetLocation();
				// A bow pack's: whether the arrow's shown, and where it (and the bow's limbs) point (pitch, yaw from ahead).
				FString Bow;
				if (Anim->GetSkeleton() && Anim->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(TEXT("arrow")) != INDEX_NONE)
				{
					const FTransform Arrow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("arrow"), EAnimPoseSpaces::World);
					const FTransform Limbs = UAnimPoseExtensions::GetBonePose(Pose, TEXT("bow"), EAnimPoseSpaces::World);
					const auto Angles = [](const FVector& Dir)
					{
						const FVector Ours(Dir.Y, -Dir.X, Dir.Z);
						return FString::Printf(TEXT("pitch %.0f yaw %.0f"), FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Ours.Z, -1.0, 1.0))),
							FMath::RadiansToDegrees(FMath::Atan2(Ours.Y, Ours.X)));
					};
					Bow = FString::Printf(TEXT("  arrow %s (%s)  bow limbs %s"), Arrow.GetScale3D().X > 0.5 ? TEXT("shown") : TEXT("hidden"),
						*Angles(Arrow.GetUnitAxis(EAxis::Y)), *Angles(Limbs.GetUnitAxis(EAxis::Y)));
				}
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("    %3.0f%%  hand_r %s  hand_l %s  (ahead, right, up)  index curl r %.0f l %.0f  chest lean %s%s"),
					Share * 100.0, *Hand(TEXT("hand_r")).ToCompactString(), *Hand(TEXT("hand_l")).ToCompactString(),
					Curl(TEXT("index_02_r")), Curl(TEXT("index_02_l")), *FVector(Spine.Y, -Spine.X, Spine.Z).ToCompactString(), *Bow);
			}
		};
		if (const UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle")))
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Reference (plain, unarmed):"));
			Report(Idle);
		}
		for (const FAssetData& Asset : Assets)
		{
			if (const UAnimSequence* Anim = Cast<UAnimSequence>(Asset.GetAsset()))
			{
				Report(Anim);
			}
		}
	}

	void AnalyzeRifle()
	{
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("=== Rifle pack analysis ==="));
		if (const USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, UE4MannequinMesh))
		{
			const USkeleton* Skeleton = Source->GetSkeleton();
			for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("UE4 socket %s on %s: loc %s rot %s"), *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
					*Socket->RelativeLocation.ToCompactString(), *Socket->RelativeRotation.ToCompactString());
			}
			for (const FVirtualBone& Bone : Skeleton->GetVirtualBones())
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("UE4 virtual bone %s: %s -> %s"), *Bone.VirtualBoneName.ToString(), *Bone.SourceBoneName.ToString(), *Bone.TargetBoneName.ToString());
			}
		}
		if (const UStaticMesh* Rifle = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Vaesen/ThirdParty/RifleAnims/ShowcaseAssets/Meshes/Rifle/SM_Rifle.SM_Rifle")))
		{
			const FBox Box = Rifle->GetBoundingBox();
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("SM_Rifle bounds min %s max %s"), *Box.Min.ToCompactString(), *Box.Max.ToCompactString());
			for (const UStaticMeshSocket* Socket : Rifle->Sockets)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("SM_Rifle socket %s loc %s rot %s"), *Socket->SocketName.ToString(), *Socket->RelativeLocation.ToCompactString(), *Socket->RelativeRotation.ToCompactString());
			}
		}

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Found;
		Registry.GetAssetsByPath(FName(RifleFolder), Found, true);
		Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Data : Found)
		{
			UObject* Asset = Data.GetAsset();
			if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(Asset))
			{
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					const FBlendParameter& Parameter = BlendSpace->GetBlendParameter(Axis);
					UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s axis %d '%s' %.0f..%.0f"), *Data.AssetName.ToString(), Axis, *Parameter.DisplayName, Parameter.Min, Parameter.Max);
				}
				for (const FBlendSample& Sample : BlendSpace->GetBlendSamples())
				{
					UE_LOG(LogVaesenAnimSetup, Display, TEXT("    sample %s at %s"), *GetNameSafe(Sample.Animation), *Sample.SampleValue.ToCompactString());
				}
			}
			else if (const UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Asset))
			{
				const bool bMontage = Anim->IsA<UAnimMontage>();
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s %s length %.2f s skeleton %s"), bMontage ? TEXT("Montage") : TEXT("Anim"), *Data.AssetName.ToString(),
					Anim->GetPlayLength(), *GetNameSafe(Anim->GetSkeleton()));
				if (const UAnimMontage* Montage = Cast<UAnimMontage>(Anim))
				{
					for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
					{
						UE_LOG(LogVaesenAnimSetup, Display, TEXT("    slot %s"), *Track.SlotName.ToString());
					}
				}
				const FString Name = Data.AssetName.ToString();
				const bool bKeyPose = Name == TEXT("AS_Rifle_Idle") || Name == TEXT("AS_Rifle_Aim") || Name == TEXT("AS_Rifle_IdleAim_FwdCenter");
				const bool bTimeline = Name.Contains(TEXT("Equip")) || Name.Contains(TEXT("Reload")) || Name == TEXT("AS_Rifle_Fire_Aim");
				if (bKeyPose)
				{
					LogPose(Anim, 0.0);
				}
				else if (bTimeline && !bMontage)
				{
					const double Length = Anim->GetPlayLength();
					for (int32 Step = 0; Step <= 10; ++Step)
					{
						LogPose(Anim, Length * Step / 10.0);
					}
				}
			}
		}

		// Where the sheath and hand sockets are on Manny now.
		if (const USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMesh))
		{
			for (const USkeletalMeshSocket* Socket : Manny->GetSkeleton()->Sockets)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Manny socket %s on %s: loc %s rot %s"), *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
					*Socket->RelativeLocation.ToCompactString(), *Socket->RelativeRotation.ToCompactString());
			}
		}
	}

	// --- One-handed sword pack (RamsterZ "One Hand Sword Attacks And Finishers") ---

	const TCHAR* SwordPackFolder = TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations");
	const TCHAR* SwordPackMesh = TEXT("/Game/1HM_Sword_Attacks_Finishers/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin");
	const TCHAR* SwordFolder = TEXT("/Game/Vaesen/Animations/Sword");

	/** How the sword hand moves through an animation: when it's fastest, and the stretch around it above half that speed. */
	struct FSwingTiming
	{
		float Length = 0.0f;
		float PeakTime = 0.0f;
		float PeakSpeed = 0.0f;
		float WindowStart = 0.0f;
		float WindowEnd = 0.0f;
		FVector RootMove = FVector::ZeroVector;
	};

	FSwingTiming MeasureSwing(const UAnimSequenceBase* Anim, FName Hand = TEXT("hand_r"))
	{
		FSwingTiming Timing;
		Timing.Length = Anim->GetPlayLength();
		const int32 Steps = FMath::Max(FMath::CeilToInt(Timing.Length * 60.0f), 2);
		TArray<float> Speeds;
		FVector Previous = FVector::ZeroVector;
		FVector FirstRoot = FVector::ZeroVector;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Time = Timing.Length * Step / Steps;
			FAnimPose Pose;
			FAnimPoseEvaluationOptions Options;
			Options.bExtractRootMotion = false;
			UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Time, Options, Pose);
			const FVector Root = UAnimPoseExtensions::GetBonePose(Pose, TEXT("root"), EAnimPoseSpaces::World).GetLocation();
			// The hand relative to the root, so walking forward isn't a swing.
			const FVector HandAt = UAnimPoseExtensions::GetBonePose(Pose, Hand, EAnimPoseSpaces::World).GetLocation() - Root;
			if (Step == 0)
			{
				FirstRoot = Root;
			}
			else
			{
				Speeds.Add((HandAt - Previous).Size() * Steps / Timing.Length);
			}
			Previous = HandAt;
			Timing.RootMove = Root - FirstRoot;
		}
		int32 Peak = 0;
		for (int32 Index = 0; Index < Speeds.Num(); ++Index)
		{
			if (Speeds[Index] > Speeds[Peak])
			{
				Peak = Index;
			}
		}
		Timing.PeakSpeed = Speeds.IsValidIndex(Peak) ? Speeds[Peak] : 0.0f;
		int32 First = Peak, Last = Peak;
		while (First > 0 && Speeds[First - 1] > Timing.PeakSpeed * 0.5f)
		{
			--First;
		}
		while (Last + 1 < Speeds.Num() && Speeds[Last + 1] > Timing.PeakSpeed * 0.5f)
		{
			++Last;
		}
		const float StepTime = Timing.Length / Steps;
		Timing.PeakTime = (Peak + 0.5f) * StepTime;
		Timing.WindowStart = First * StepTime;
		Timing.WindowEnd = (Last + 1) * StepTime;
		return Timing;
	}

	/** The time in [From, To] the bone is slowest (relative to the root): where a block settles into its guard. */
	float FindSettle(const UAnimSequenceBase* Anim, float From, float To, FName Bone = TEXT("hand_r"))
	{
		const float Step = 1.0f / 60.0f;
		float Best = From, BestSpeed = TNumericLimits<float>::Max();
		FVector Previous = FVector::ZeroVector;
		for (float Time = From; Time <= FMath::Min(To, Anim->GetPlayLength()); Time += Step)
		{
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Time, FAnimPoseEvaluationOptions(), Pose);
			const FVector At = UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::World).GetLocation()
				- UAnimPoseExtensions::GetBonePose(Pose, TEXT("root"), EAnimPoseSpaces::World).GetLocation();
			const float Speed = (At - Previous).Size() / Step;
			if (Time > From && Speed < BestSpeed)
			{
				BestSpeed = Speed;
				Best = Time;
			}
			Previous = At;
		}
		return Best;
	}

	/** The time in [From, To] Bone comes nearest Target (the hand at the hip where the sword hangs). */
	float FindNearest(const UAnimSequenceBase* Anim, FName Bone, FName Target, float From, float To)
	{
		float Best = From, BestDistance = TNumericLimits<float>::Max();
		for (float Time = From; Time <= FMath::Min(To, Anim->GetPlayLength()); Time += 1.0f / 60.0f)
		{
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Time, FAnimPoseEvaluationOptions(), Pose);
			const float Distance = FVector::Dist(UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::World).GetLocation(),
				UAnimPoseExtensions::GetBonePose(Pose, Target, EAnimPoseSpaces::World).GetLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Time;
			}
		}
		return Best;
	}

	bool BladeInHand(FVector& OutBlade, FVector& OutEdge, FVector& OutGrip);

	// --- Pistol pack (MandB Animation "Pistol Locomotion Pack") ---

	const TCHAR* PistolPackFolder = TEXT("/Game/PistolLocomotionPack");

	void AnalyzePistol()
	{
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("=== Pistol pack analysis ==="));
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Found;
		Registry.GetAssetsByPath(FName(FString(PistolPackFolder) / TEXT("Animations")), Found, true);
		Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Data : Found)
		{
			const UAnimSequence* Anim = Cast<UAnimSequence>(Data.GetAsset());
			if (!Anim)
			{
				continue;
			}
			const FSwingTiming Timing = MeasureSwing(Anim);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%-48s %.2f s root motion %d lock %d, root moves %s (%.0f cm/s); hand fastest %.0f cm/s at %.2f; skeleton %s"),
				*Data.AssetName.ToString(), Timing.Length, Anim->bEnableRootMotion ? 1 : 0, Anim->bForceRootLock ? 1 : 0, *Timing.RootMove.ToCompactString(),
				Timing.RootMove.Size2D() / FMath::Max(Timing.Length, 0.01f), Timing.PeakSpeed, Timing.PeakTime, *GetPathNameSafe(Anim->GetSkeleton()));
		}

		// The pistol meshes, and how the demo character holds one.
		TArray<FAssetData> Demo;
		Registry.GetAssetsByPath(FName(FString(PistolPackFolder) / TEXT("Demo")), Demo, true);
		for (const FAssetData& Data : Demo)
		{
			UObject* Asset = Data.GetAsset();
			if (const USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset); Mesh && Data.PackagePath.ToString().Contains(TEXT("pistol")))
			{
				const FBox Box = Mesh->GetImportedBounds().GetBox();
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Mesh %s (skeletal, %d bones) bounds %s .. %s"), *Data.AssetName.ToString(), Mesh->GetRefSkeleton().GetNum(),
					*Box.Min.ToCompactString(), *Box.Max.ToCompactString());
				for (int32 Bone = 0; Bone < Mesh->GetRefSkeleton().GetNum(); ++Bone)
				{
					UE_LOG(LogVaesenAnimSetup, Display, TEXT("    bone %s"), *Mesh->GetRefSkeleton().GetBoneName(Bone).ToString());
				}
			}
			if (const UStaticMesh* Static = Cast<UStaticMesh>(Asset); Static && Data.PackagePath.ToString().Contains(TEXT("pistol")))
			{
				const FBox Box = Static->GetBoundingBox();
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Mesh %s (static) bounds %s .. %s"), *Data.AssetName.ToString(), *Box.Min.ToCompactString(), *Box.Max.ToCompactString());
			}
			if (const USkeleton* Skeleton = Cast<USkeleton>(Asset))
			{
				for (const USkeletalMeshSocket* Socket : Skeleton->Sockets)
				{
					UE_LOG(LogVaesenAnimSetup, Display, TEXT("Skeleton %s socket %s on %s: loc %s rot %s"), *Data.AssetName.ToString(), *Socket->SocketName.ToString(),
						*Socket->BoneName.ToString(), *Socket->RelativeLocation.ToCompactString(), *Socket->RelativeRotation.ToCompactString());
				}
			}
			if (const UBlueprint* Blueprint = Cast<UBlueprint>(Asset); Blueprint && Blueprint->SimpleConstructionScript)
			{
				for (const USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
				{
					if (const USceneComponent* Component = Node ? Cast<USceneComponent>(Node->ComponentTemplate) : nullptr)
					{
						UE_LOG(LogVaesenAnimSetup, Display, TEXT("BP %s: %s (%s) on %s socket '%s' at %s"), *Data.AssetName.ToString(), *Node->GetVariableName().ToString(),
							*Component->GetClass()->GetName(), *Node->ParentComponentOrVariableName.ToString(), *Node->AttachToName.ToString(), *Describe(Component->GetRelativeTransform()));
					}
				}
			}
		}
	}

	void AnalyzeSword()
	{
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("=== Sword pack analysis ==="));
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Found;
		Registry.GetAssetsByPath(FName(SwordPackFolder), Found, true);
		Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
		for (const FAssetData& Data : Found)
		{
			const FString Name = Data.AssetName.ToString();
			const UAnimSequence* Anim = Cast<UAnimSequence>(Data.GetAsset());
			if (!Anim || Name.Contains(TEXT("Mirrored")))
			{
				continue;
			}
			const FSwingTiming Timing = MeasureSwing(Anim);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%-40s %.2f s root motion %d, root moves %s; hand fastest %.0f cm/s at %.2f (%.2f..%.2f) skeleton %s"),
				*Name, Timing.Length, Anim->bEnableRootMotion ? 1 : 0, *Timing.RootMove.ToCompactString(), Timing.PeakSpeed, Timing.PeakTime,
				Timing.WindowStart, Timing.WindowEnd, *GetNameSafe(Anim->GetSkeleton()));
		}
		if (const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, SwordPackMesh))
		{
			for (const USkeletalMeshSocket* Socket : Mesh->GetSkeleton()->Sockets)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Sword pack socket %s on %s: loc %s rot %s"), *Socket->SocketName.ToString(), *Socket->BoneName.ToString(),
					*Socket->RelativeLocation.ToCompactString(), *Socket->RelativeRotation.ToCompactString());
			}
		}
		// Where the pack's sword points in a few poses (the mannequin faces +Y; his right is -X).
		const USkeletalMesh* PackMesh = LoadObject<USkeletalMesh>(nullptr, SwordPackMesh);
		const USkeletalMeshSocket* Socket = PackMesh ? PackMesh->FindSocket(TEXT("WeaponSocket")) : nullptr;
		struct FLook { const TCHAR* Folder; const TCHAR* Name; float Time; };
		for (const FLook& Look : { FLook{ TEXT("1HM_Idle"), TEXT("1HM_Idle_Standing"), 0.0f }, FLook{ TEXT("1HM_Block"), TEXT("1HM_BlockRight"), 0.54f },
				 FLook{ TEXT("1HM_Slash"), TEXT("1HM_Slash01"), 0.34f }, FLook{ TEXT("1HM_Stab"), TEXT("1HM_Stab01"), 0.3f }, FLook{ TEXT("1HM_Walk"), TEXT("1HM_WalkForward"), 0.0f } })
		{
			const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/%s/%s.%s"), SwordPackFolder, Look.Folder, Look.Name, Look.Name));
			if (!Anim || !Socket)
			{
				continue;
			}
			FAnimPose Pose;
			UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Look.Time, FAnimPoseEvaluationOptions(), Pose);
			const FTransform Hand = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World);
			const FVector Root = UAnimPoseExtensions::GetBonePose(Pose, TEXT("root"), EAnimPoseSpaces::World).GetLocation();
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s hand axes X %s Y %s Z %s; forearm %s"), Look.Name, *Hand.GetUnitAxis(EAxis::X).ToCompactString(), *Hand.GetUnitAxis(EAxis::Y).ToCompactString(), *Hand.GetUnitAxis(EAxis::Z).ToCompactString(),
				*(Hand.GetLocation() - UAnimPoseExtensions::GetBonePose(Pose, TEXT("lowerarm_r"), EAnimPoseSpaces::World).GetLocation()).GetSafeNormal().ToCompactString());
			FVector BladeLocal, EdgeLocal, GripLocal;
			if (BladeInHand(BladeLocal, EdgeLocal, GripLocal))
			{
				const FVector Blade = Hand.TransformVectorNoScale(BladeLocal);
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s at %.2f: hand %s, blade points %s, tip at %s (from the root)"), Look.Name, Look.Time,
					*(Hand.GetLocation() - Root).ToCompactString(), *Blade.ToCompactString(), *(Hand.TransformPosition(GripLocal) + Blade * 80.0 - Root).ToCompactString());
			}
		}
		// The draw from the hip, frame by frame: where the hand and the pack's blade are.
		FVector BladeLocal, EdgeLocal, GripLocal;
		const UAnimSequence* Equip = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Idle/1HM_Idle_Equip.1HM_Idle_Equip"));
		if (Equip && BladeInHand(BladeLocal, EdgeLocal, GripLocal))
		{
			for (float Time = 0.0f; Time <= Equip->GetPlayLength(); Time += 0.1f)
			{
				FAnimPose Pose;
				UAnimPoseExtensions::GetAnimPoseAtTime(Equip, Time, FAnimPoseEvaluationOptions(), Pose);
				const FTransform Hand = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World);
				const FVector Root = UAnimPoseExtensions::GetBonePose(Pose, TEXT("root"), EAnimPoseSpaces::World).GetLocation();
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Equip %.1f: grip %s blade %s"), Time, *(Hand.TransformPosition(GripLocal) - Root).ToCompactString(),
					*Hand.TransformVectorNoScale(BladeLocal).ToCompactString());
			}
		}
		if (const UStaticMesh* Sword = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/1HM_Sword_Attacks_Finishers/Weapons/OneHandSword_Mesh.OneHandSword_Mesh")))
		{
			const FBox Box = Sword->GetBoundingBox();
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("OneHandSword_Mesh bounds min %s max %s"), *Box.Min.ToCompactString(), *Box.Max.ToCompactString());
		}
	}

	// --- Building blocks ---

	const TCHAR* UpperBodySlot = TEXT("UpperBody");
	const TCHAR* MannySkeletonPath = TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin");
	const TCHAR* UnarmedGraphPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C");
	const TCHAR* CharacterGraphPath = TEXT("/Game/Vaesen/Animations/ABP_VaesenCharacter");
	const TCHAR* RifleGraphPath = TEXT("/Game/Vaesen/Animations/Rifle/ABP_RifleLocomotion");

	UAnimSequence* RifleAnim(const TCHAR* Name)
	{
		UAnimSequence* Anim = LoadAt<UAnimSequence>(FString(RifleFolder) / Name);
		if (!Anim)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Retargeted %s is missing."), Name);
		}
		return Anim;
	}

	template <typename T>
	T* RifleAsset(const TCHAR* Name)
	{
		T* Asset = LoadAt<T>(FString(RifleFolder) / Name);
		if (!Asset)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Retargeted %s is missing."), Name);
		}
		return Asset;
	}

	/** A montage of Anim in Slot (made once). */
	UAnimMontage* MakeMontage(const FString& PackagePath, UAnimSequence* Anim, FName Slot, float BlendIn, float BlendOut, float Rate = 1.0f)
	{
		if (UAnimMontage* Existing = LoadAt<UAnimMontage>(PackagePath))
		{
			// Moved to another slot since it was made (the sword's attacks went full body).
			if (Existing->SlotAnimTracks.Num() > 0 && Existing->SlotAnimTracks[0].SlotName != Slot)
			{
				Existing->SlotAnimTracks[0].SlotName = Slot;
				Save(Existing);
			}
			return Existing;
		}
		if (!Anim)
		{
			return nullptr;
		}
		UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
		Factory->SourceAnimation = Anim;
		Factory->TargetSkeleton = Anim->GetSkeleton();
		UPackage* Package = CreatePackage(*PackagePath);
		UAnimMontage* Montage = Cast<UAnimMontage>(Factory->FactoryCreateNew(UAnimMontage::StaticClass(), Package,
			FName(FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone, nullptr, GWarn));
		if (!Montage)
		{
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Montage);
		if (Montage->SlotAnimTracks.Num() > 0)
		{
			Montage->SlotAnimTracks[0].SlotName = Slot;
		}
		Montage->BlendIn.SetBlendTime(BlendIn);
		Montage->BlendOut.SetBlendTime(BlendOut);
		Montage->RateScale = Rate;
		return Montage;
	}

	void AddNotify(UAnimMontage* Montage, UAnimNotify* Notify, float Time)
	{
		if (Montage->AnimNotifyTracks.Num() == 0)
		{
			Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White));
		}
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = FName(Notify->GetNotifyName());
		Event.Notify = Notify;
		Event.Link(Montage, Time);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Time));
		Event.TrackIndex = 0;
		Montage->RefreshCacheData();
	}

	/** A copy of Source (made once) with a HandIK_Sheath curve through Keys (time, weight). */
	UAnimSequence* CopyWithSheathCurve(const FString& PackagePath, UAnimSequence* Source, const TArray<FVector2f>& Keys)
	{
		if (UAnimSequence* Existing = LoadAt<UAnimSequence>(PackagePath))
		{
			return Existing;
		}
		if (!Source)
		{
			return nullptr;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		UAnimSequence* Copy = DuplicateObject<UAnimSequence>(Source, Package, FName(FPackageName::GetShortName(PackagePath)));
		Copy->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Copy);

		IAnimationDataController& Controller = Copy->GetController();
		Controller.OpenBracket(FText::FromString(TEXT("Hand IK curve")), false);
		const FAnimationCurveIdentifier Curve(UVaesenAnimInstance::HandIKSheathCurve, ERawCurveTrackTypes::RCT_Float);
		Controller.AddCurve(Curve, AACF_DefaultCurve, false);
		TArray<FRichCurveKey> CurveKeys;
		for (const FVector2f& Key : Keys)
		{
			FRichCurveKey& RichKey = CurveKeys.Emplace_GetRef(Key.X, Key.Y);
			RichKey.InterpMode = RCIM_Cubic;
			RichKey.TangentMode = RCTM_Auto;
		}
		Controller.SetCurveKeys(Curve, CurveKeys, false);
		Controller.CloseBracket(false);
		Save(Copy);
		return Copy;
	}

	/** Builds an anim graph from nodes, wiring pose pins and variable pins. */
	struct FGraphBuilder
	{
		UEdGraph* Graph = nullptr;

		explicit FGraphBuilder(UAnimBlueprint* Blueprint)
		{
			for (UEdGraph* Each : Blueprint->FunctionGraphs)
			{
				if (Each && Each->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
				{
					Graph = Each;
				}
			}
		}

		template <typename T>
		T* Add(int32 X, int32 Y, TFunctionRef<void(T*)> Setup)
		{
			FGraphNodeCreator<T> Creator(*Graph);
			T* Node = Creator.CreateNode();
			Node->NodePosX = X;
			Node->NodePosY = Y;
			Setup(Node);
			Creator.Finalize();
			return Node;
		}

		UAnimGraphNode_Root* Root() const
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				if (UAnimGraphNode_Root* Found = Cast<UAnimGraphNode_Root>(Node))
				{
					return Found;
				}
			}
			return nullptr;
		}

		static UEdGraphPin* PoseOut(UEdGraphNode* Node)
		{
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct)
				{
					return Pin;
				}
			}
			return nullptr;
		}

		static UEdGraphPin* In(UEdGraphNode* Node, const TCHAR* Name)
		{
			UEdGraphPin* Pin = Node->FindPin(FName(Name), EGPD_Input);
			if (!Pin)
			{
				UE_LOG(LogVaesenAnimSetup, Warning, TEXT("%s has no input pin %s."), *Node->GetName(), Name);
			}
			return Pin;
		}

		bool Link(UEdGraphPin* From, UEdGraphPin* To) const
		{
			if (!From || !To)
			{
				return false;
			}
			const bool bLinked = Graph->GetSchema()->TryCreateConnection(From, To);
			if (!bLinked)
			{
				UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Couldn't link %s.%s -> %s.%s"), *From->GetOwningNode()->GetName(), *From->PinName.ToString(),
					*To->GetOwningNode()->GetName(), *To->PinName.ToString());
			}
			return bLinked;
		}

		/** Pose of From into the pose pin PinName of To. */
		bool Pose(UEdGraphNode* From, UEdGraphNode* To, const TCHAR* PinName) const
		{
			return Link(PoseOut(From), In(To, PinName));
		}

		/** A variable of the anim instance (UVaesenAnimInstance) into a pin. */
		void Var(FName Variable, UEdGraphNode* To, const TCHAR* PinName)
		{
			UK2Node_VariableGet* Get = Add<UK2Node_VariableGet>(To->NodePosX - 260, To->NodePosY + 40 * (++VarCount % 4), [Variable](UK2Node_VariableGet* Node)
			{
				Node->VariableReference.SetSelfMember(Variable);
			});
			Link(Get->FindPin(Variable, EGPD_Output), In(To, PinName));
		}

		int32 VarCount = 0;
	};

	UAnimBlueprint* NewAnimBlueprint(const FString& PackagePath, USkeleton* Skeleton)
	{
		UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
		Factory->BlueprintType = BPTYPE_Normal;
		Factory->ParentClass = UVaesenAnimInstance::StaticClass();
		Factory->TargetSkeleton = Skeleton;
		UPackage* Package = CreatePackage(*PackagePath);
		UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), Package,
			FName(FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
		if (Blueprint)
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
		}
		return Blueprint;
	}

	/** The anim Blueprint at PackagePath: made new, or (rebuilding) the existing one with its anim graph cleared. */
	UAnimBlueprint* OpenAnimBlueprint(const FString& PackagePath, USkeleton* Skeleton)
	{
		UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(PackagePath);
		if (!Existing)
		{
			return NewAnimBlueprint(PackagePath, Skeleton);
		}
		for (UEdGraph* Graph : Existing->FunctionGraphs)
		{
			if (Graph && Graph->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
			{
				for (UEdGraphNode* Node : TArray<UEdGraphNode*>(Graph->Nodes))
				{
					if (!Cast<UAnimGraphNode_Root>(Node))
					{
						FBlueprintEditorUtils::RemoveNode(Existing, Node, true);
					}
				}
			}
		}
		return Existing;
	}

	bool CompileAndSave(UAnimBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			if (Message->GetSeverity() <= EMessageSeverity::Warning)
			{
				UE_LOG(LogVaesenAnimSetup, Warning, TEXT("%s: %s"), *Blueprint->GetName(), *Message->ToText().ToString());
			}
		}
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogVaesenAnimSetup, Error, TEXT("%s failed to compile."), *Blueprint->GetName());
		}
		return Save(Blueprint) && Blueprint->Status != BS_Error;
	}

	/** A blend space played at (Direction, Speed), with its aim offset (AimPitch) on top. */
	UEdGraphNode* LocomotionWithAim(FGraphBuilder& Builder, int32 X, int32 Y, UBlendSpace* Movement, UBlendSpace* AimOffset)
	{
		UAnimGraphNode_BlendSpacePlayer* Player = Builder.Add<UAnimGraphNode_BlendSpacePlayer>(X, Y, [Movement](UAnimGraphNode_BlendSpacePlayer* Node)
		{
			Node->Node.SetBlendSpace(Movement);
		});
		Builder.Var(TEXT("Direction"), Player, TEXT("X"));
		Builder.Var(TEXT("Speed"), Player, TEXT("Y"));
		UAnimGraphNode_RotationOffsetBlendSpace* Aim = Builder.Add<UAnimGraphNode_RotationOffsetBlendSpace>(X + 350, Y, [AimOffset](UAnimGraphNode_RotationOffsetBlendSpace* Node)
		{
			Node->Node.SetBlendSpace(AimOffset);
		});
		Builder.Pose(Player, Aim, TEXT("BasePose"));
		Builder.Var(TEXT("AimPitch"), Aim, TEXT("X"));
		return Aim;
	}

	/** Blend Poses by Bool: True / False. */
	UAnimGraphNode_BlendListByBool* ByBool(FGraphBuilder& Builder, int32 X, int32 Y, FName Variable, UEdGraphNode* WhenTrue, UEdGraphNode* WhenFalse, float BlendTime)
	{
		UAnimGraphNode_BlendListByBool* Blend = Builder.Add<UAnimGraphNode_BlendListByBool>(X, Y, [](UAnimGraphNode_BlendListByBool*) {});
		// The blend times are pins (shown by default); their defaults are what the compiled node gets.
		for (UEdGraphPin* Pin : Blend->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinName.ToString().StartsWith(TEXT("BlendTime")))
			{
				Builder.Graph->GetSchema()->TrySetDefaultValue(*Pin, FString::SanitizeFloat(BlendTime));
			}
		}
		Builder.Var(Variable, Blend, TEXT("bActiveValue"));
		Builder.Pose(WhenTrue, Blend, TEXT("BlendPose_0"));
		Builder.Pose(WhenFalse, Blend, TEXT("BlendPose_1"));
		return Blend;
	}

	/**
	 * ABP_RifleLocomotion: the rifle's locomotion, linked into the character's graph while a rifle is drawn. Standing
	 * or crouched, from the hip or aimed (each a direction/speed blend space with its pitch aim offset), and the jump
	 * loop in the air.
	 */
	UClass* BuildRifleLocomotion(USkeleton* Skeleton, bool bRebuild)
	{
		if (!bRebuild)
		{
			if (UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(RifleGraphPath))
			{
				return Existing->GeneratedClass;
			}
		}
		UAnimBlueprint* Blueprint = OpenAnimBlueprint(RifleGraphPath, Skeleton);
		FGraphBuilder Builder(Blueprint);
		if (!Blueprint || !Builder.Graph)
		{
			return nullptr;
		}

		UBlendSpace* AimIdle = RifleAsset<UBlendSpace>(TEXT("AO1D_Rifle_Idle"));
		UBlendSpace* AimAimed = RifleAsset<UBlendSpace>(TEXT("AO1D_Rifle_Aim"));
		UEdGraphNode* Hip = LocomotionWithAim(Builder, -1600, 0, RifleAsset<UBlendSpace>(TEXT("BS_Rifle")), AimIdle);
		UEdGraphNode* Aimed = LocomotionWithAim(Builder, -1600, 250, RifleAsset<UBlendSpace>(TEXT("BS_Rifle_Aim")), AimAimed);
		UEdGraphNode* CrouchHip = LocomotionWithAim(Builder, -1600, 500, RifleAsset<UBlendSpace>(TEXT("BS_Rifle_Crouch")), AimIdle);
		UEdGraphNode* CrouchAimed = LocomotionWithAim(Builder, -1600, 750, RifleAsset<UBlendSpace>(TEXT("BS_Rifle_Crouch_Aim")), AimAimed);

		UEdGraphNode* Standing = ByBool(Builder, -900, 100, TEXT("bAiming"), Aimed, Hip, 0.2f);
		UEdGraphNode* Crouched = ByBool(Builder, -900, 600, TEXT("bAiming"), CrouchAimed, CrouchHip, 0.2f);
		UEdGraphNode* Grounded = ByBool(Builder, -600, 300, TEXT("bCrouching"), Crouched, Standing, 0.25f);

		UAnimSequence* JumpLoop = RifleAnim(TEXT("AS_Rifle_JumpLoop"));
		UAnimGraphNode_SequencePlayer* Air = Builder.Add<UAnimGraphNode_SequencePlayer>(-600, 650, [JumpLoop](UAnimGraphNode_SequencePlayer* Node)
		{
			Node->Node.SetSequence(JumpLoop);
		});
		UEdGraphNode* Final = ByBool(Builder, -300, 300, TEXT("bFalling"), Air, Grounded, 0.15f);
		Builder.Pose(Final, Builder.Root(), TEXT("Result"));

		CompileAndSave(Blueprint);
		return Blueprint->GeneratedClass;
	}

	/**
	 * ABP_VaesenCharacter, the character's anim Blueprint:
	 *   weapon locomotion (linked graph tagged WeaponLocomotion: ABP_Manny unarmed, the weapon's when one is drawn)
	 *   -> UpperBody slot, layered from spine_01 up (fire, reload, draw: the legs keep walking)
	 *   -> DefaultSlot (full-body montages: stagger, hit reactions)
	 *   -> fingers closed round the grips (the category's GripPose, finger bones only)
	 *   -> the drawing hand onto the sheathed weapon's grip (HandIK_Sheath)
	 *   -> the Vaesen Shoulder Aim node: the weapon bone (ik_hand_gun, which the held weapon rides on), both hands on the
	 *      weapon, the shoulder aim (shoulder, head, recoil) and the bolt worked by hand
	 */
	UClass* BuildCharacterGraph(USkeleton* Skeleton, bool bRebuild)
	{
		if (!bRebuild)
		{
			if (UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(CharacterGraphPath))
			{
				return Existing->GeneratedClass;
			}
		}
		UClass* Unarmed = LoadObject<UClass>(nullptr, UnarmedGraphPath);
		UAnimBlueprint* Blueprint = OpenAnimBlueprint(CharacterGraphPath, Skeleton);
		FGraphBuilder Builder(Blueprint);
		if (!Blueprint || !Builder.Graph)
		{
			return nullptr;
		}

		UAnimGraphNode_LinkedAnimGraph* Locomotion = Builder.Add<UAnimGraphNode_LinkedAnimGraph>(-2000, 0, [Unarmed](UAnimGraphNode_LinkedAnimGraph* Node)
		{
			Node->Node.InstanceClass = Unarmed;
		});
		Locomotion->SetTag(UVaesenEquipmentComponent::WeaponLocomotionTag);
		UAnimGraphNode_SaveCachedPose* Cache = Builder.Add<UAnimGraphNode_SaveCachedPose>(-1700, 0, [](UAnimGraphNode_SaveCachedPose* Node)
		{
			Node->CacheName = TEXT("Locomotion");
		});
		Builder.Pose(Locomotion, Cache, TEXT("Pose"));
		const auto UseCache = [&Builder, Cache](int32 X, int32 Y)
		{
			return Builder.Add<UAnimGraphNode_UseCachedPose>(X, Y, [Cache](UAnimGraphNode_UseCachedPose* Node)
			{
				Node->SaveCachedPoseNode = Cache;
			});
		};

		UAnimGraphNode_Slot* UpperSlot = Builder.Add<UAnimGraphNode_Slot>(-1300, 200, [](UAnimGraphNode_Slot* Node)
		{
			Node->Node.SlotName = UpperBodySlot;
		});
		Builder.Pose(UseCache(-1600, 200), UpperSlot, TEXT("Source"));

		UAnimGraphNode_LayeredBoneBlend* Layered = Builder.Add<UAnimGraphNode_LayeredBoneBlend>(-1000, 0, [](UAnimGraphNode_LayeredBoneBlend* Node)
		{
			Node->Node.bMeshSpaceRotationBlend = true;
			if (Node->Node.LayerSetup.Num() == 0)
			{
				Node->Node.LayerSetup.AddDefaulted();
			}
			FBranchFilter Filter;
			Filter.BoneName = TEXT("spine_01");
			Filter.BlendDepth = 0;
			Node->Node.LayerSetup[0].BranchFilters = { Filter };
		});
		Builder.Pose(UseCache(-1300, -100), Layered, TEXT("BasePose"));
		Builder.Pose(UpperSlot, Layered, TEXT("BlendPoses_0"));

		UAnimGraphNode_Slot* FullSlot = Builder.Add<UAnimGraphNode_Slot>(-700, 0, [](UAnimGraphNode_Slot* Node)
		{
			Node->Node.SlotName = FAnimSlotGroup::DefaultSlotName;
		});
		Builder.Pose(Layered, FullSlot, TEXT("Source"));

		// The fingers closed round the grips: the category's grip pose, over the finger bones of each holding hand.
		UAnimGraphNode_SequenceEvaluator* Grip = Builder.Add<UAnimGraphNode_SequenceEvaluator>(-900, 400, [](UAnimGraphNode_SequenceEvaluator*) {});
		for (FOptionalPinFromProperty& Pin : Grip->ShowPinForProperties)
		{
			if (Pin.PropertyName == TEXT("Sequence"))
			{
				Pin.bShowPin = true;
			}
		}
		Grip->ReconstructNode();
		Builder.Var(TEXT("GripPose"), Grip, TEXT("Sequence"));
		UAnimGraphNode_SaveCachedPose* GripCache = Builder.Add<UAnimGraphNode_SaveCachedPose>(-650, 400, [](UAnimGraphNode_SaveCachedPose* Node)
		{
			Node->CacheName = TEXT("GripPose");
		});
		Builder.Pose(Grip, GripCache, TEXT("Pose"));
		const auto UseGrip = [&Builder, GripCache](int32 Y)
		{
			return Builder.Add<UAnimGraphNode_UseCachedPose>(-650, Y, [GripCache](UAnimGraphNode_UseCachedPose* Node)
			{
				Node->SaveCachedPoseNode = GripCache;
			});
		};
		UAnimGraphNode_LayeredBoneBlend* Fingers = Builder.Add<UAnimGraphNode_LayeredBoneBlend>(-450, 200, [](UAnimGraphNode_LayeredBoneBlend* Node)
		{
			Node->Node.AddPose();
			Node->Node.LayerSetup.SetNum(2);
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const TCHAR* Suffix = Side == 0 ? TEXT("_l") : TEXT("_r");
				TArray<FBranchFilter>& Filters = Node->Node.LayerSetup[Side].BranchFilters;
				Filters.Reset();
				for (const TCHAR* Finger : { TEXT("thumb_01"), TEXT("index_metacarpal"), TEXT("middle_metacarpal"), TEXT("ring_metacarpal"), TEXT("pinky_metacarpal") })
				{
					FBranchFilter& Filter = Filters.AddDefaulted_GetRef();
					Filter.BoneName = FName(FString(Finger) + Suffix);
					Filter.BlendDepth = 0;
				}
			}
		});
		Builder.Pose(FullSlot, Fingers, TEXT("BasePose"));
		Builder.Pose(UseGrip(500), Fingers, TEXT("BlendPoses_0"));
		Builder.Pose(UseGrip(600), Fingers, TEXT("BlendPoses_1"));
		Builder.Var(TEXT("LeftFingerGrip"), Fingers, TEXT("BlendWeights_0"));
		Builder.Var(TEXT("RightFingerGrip"), Fingers, TEXT("BlendWeights_1"));

		UAnimGraphNode_LocalToComponentSpace* ToComponent = Builder.Add<UAnimGraphNode_LocalToComponentSpace>(-200, 0, [](UAnimGraphNode_LocalToComponentSpace*) {});
		Builder.Pose(Fingers, ToComponent, TEXT("LocalPose"));

		const auto HandIK = [&Builder](int32 X, const TCHAR* Hand, const TCHAR* Elbow)
		{
			return Builder.Add<UAnimGraphNode_TwoBoneIK>(X, 0, [Hand, Elbow](UAnimGraphNode_TwoBoneIK* Node)
			{
				Node->Node.IKBone.BoneName = Hand;
				Node->Node.EffectorLocationSpace = BCS_WorldSpace;
				Node->Node.JointTargetLocationSpace = BCS_BoneSpace;
				Node->Node.JointTarget.BoneReference.BoneName = Elbow;
				Node->Node.JointTargetLocation = FVector::ZeroVector;
				Node->Node.bAllowStretching = false;
			});
		};
		// The drawing hand onto the sheathed weapon (before the weapon bone is set, so a drawn weapon follows the hand).
		UAnimGraphNode_TwoBoneIK* DrawHand = HandIK(-150, TEXT("hand_r"), TEXT("lowerarm_r"));
		Builder.Pose(ToComponent, DrawHand, TEXT("ComponentPose"));
		Builder.Var(TEXT("SheathHandIKAlpha"), DrawHand, TEXT("Alpha"));
		Builder.Var(TEXT("SheathHandIKLocation"), DrawHand, TEXT("EffectorLocation"));

		// The Vaesen Shoulder Aim node holds the weapon in the hands: the weapon bone, both hands on the weapon, the
		// shoulder aim (shoulder, head, recoil) and the bolt worked by hand. Always on (it blends itself).
		UAnimGraphNode_VaesenShoulderAim* ShoulderAim = Builder.Add<UAnimGraphNode_VaesenShoulderAim>(200, 0, [](UAnimGraphNode_VaesenShoulderAim*) {});
		Builder.Pose(DrawHand, ShoulderAim, TEXT("ComponentPose"));

		UAnimGraphNode_ComponentToLocalSpace* ToLocal = Builder.Add<UAnimGraphNode_ComponentToLocalSpace>(550, 0, [](UAnimGraphNode_ComponentToLocalSpace*) {});
		Builder.Pose(ShoulderAim, ToLocal, TEXT("ComponentPose"));
		Builder.Pose(ToLocal, Builder.Root(), TEXT("Result"));

		CompileAndSave(Blueprint);
		return Blueprint->GeneratedClass;
	}

	// --- Sockets ---

	FAnimPose PoseAt(const UAnimSequenceBase* Anim, double Time)
	{
		FAnimPose Pose;
		UAnimPoseExtensions::GetAnimPoseAtTime(Anim, Time, FAnimPoseEvaluationOptions(), Pose);
		return Pose;
	}

	/**
	 * The hand socket on Manny that holds a rifle the way the pack does: the pack holds its rifle at the UE4 skeleton's
	 * Gun socket on hand_r; the same rifle placement, seen from Manny's retargeted hand, is the socket.
	 */
	bool ComputeHandSocket(FTransform& OutSocket, FTransform& OutLeftPalmSocket, FVector& OutLeftGripInRifle)
	{
		const UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/RifleAnims/Animations/BlendSpaces/Standing_IdleWalk_Aim/AS_Rifle_Aim.AS_Rifle_Aim"));
		const UAnimSequence* Target = RifleAnim(TEXT("AS_Rifle_Aim"));
		const USkeletalMesh* UE4Mesh = LoadObject<USkeletalMesh>(nullptr, UE4MannequinMesh);
		const USkeletalMeshSocket* Gun = UE4Mesh ? UE4Mesh->GetSkeleton()->FindSocket(TEXT("Gun")) : nullptr;
		if (!Source || !Target || !Gun)
		{
			return false;
		}
		const FAnimPose SourcePose = PoseAt(Source, 0.0);
		const FAnimPose TargetPose = PoseAt(Target, 0.0);
		const FTransform Hand4 = UAnimPoseExtensions::GetBonePose(SourcePose, TEXT("hand_r"), EAnimPoseSpaces::World);
		const FTransform Hand5 = UAnimPoseExtensions::GetBonePose(TargetPose, TEXT("hand_r"), EAnimPoseSpaces::World);
		const FTransform GunLocal(Gun->RelativeRotation, Gun->RelativeLocation);
		const FTransform Rifle4 = GunLocal * Hand4;

		OutSocket.SetRotation(Hand5.GetRotation().Inverse() * Rifle4.GetRotation());
		OutSocket.SetLocation(Hand5.GetRotation().UnrotateVector(Hand4.GetRotation().RotateVector(GunLocal.GetLocation())));
		OutSocket.SetScale3D(FVector::OneVector);

		const FTransform Rifle5 = OutSocket * Hand5;
		// The left palm (between the wrist and the middle knuckle) as it holds the pack's rifle: hand_l_weapon is that
		// palm turned like the rifle (X to the muzzle, Z up), so a weapon's LeftGrip socket, turned the same way, puts
		// the hand back exactly as the pack holds it.
		const FTransform LeftHand5 = UAnimPoseExtensions::GetBonePose(TargetPose, TEXT("hand_l"), EAnimPoseSpaces::World);
		const FVector Knuckle5 = UAnimPoseExtensions::GetBonePose(TargetPose, TEXT("middle_01_l"), EAnimPoseSpaces::World).GetLocation();
		const FTransform Palm5(Rifle5.GetRotation(), (LeftHand5.GetLocation() + Knuckle5) * 0.5);
		OutLeftPalmSocket = Palm5.GetRelativeTransform(LeftHand5);
		OutLeftPalmSocket.SetScale3D(FVector::OneVector);
		OutLeftGripInRifle = Rifle5.InverseTransformPosition(Palm5.GetLocation());
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Hand socket %s; aimed rifle points %s (forward is +Y); left palm on the rifle at %s"),
			*Describe(OutSocket), *Rifle5.GetRotation().GetForwardVector().ToCompactString(), *OutLeftGripInRifle.ToCompactString());
		return true;
	}

	/**
	 * The back sling, worked out in Manny's mesh space (Manny faces +Y; his right is -X): the grip at the middle of the
	 * back, the barrel diagonally up over the right shoulder, the sights facing out.
	 */
	FTransform ComputeBackSling(const USkeleton* Skeleton, FName Bone)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const int32 BoneIndex = Reference.FindBoneIndex(Bone);
		const FTransform BoneInMesh = BoneIndex != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, BoneIndex) : FTransform::Identity;
		const FVector Barrel = FVector(-0.5, 0.0, 0.866).GetSafeNormal();
		const FVector Up(0.0, -1.0, 0.0);
		const FTransform Desired(FRotationMatrix::MakeFromXZ(Barrel, Up).ToQuat(), FVector(4.0, -17.0, 118.0));
		return Desired.GetRelativeTransform(BoneInMesh);
	}

	/** Sets a socket that's missing (with bOverwrite, also one that exists: it may have been tuned by hand). */
	/**
	 * The right eye, on the head bone (the sight line goes through it when aiming): about 11.5 cm below the crown (the
	 * top of Manny's mesh), in front of the head bone and a little to its right (Manny faces +Y; his right is -X).
	 */
	FTransform ComputeRightEye(const USkeleton* Skeleton)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const int32 Head = Reference.FindBoneIndex(TEXT("head"));
		const USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
		if (Head == INDEX_NONE || !Manny)
		{
			return FTransform::Identity;
		}
		const FTransform HeadInMesh = FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, Head);
		const FBox Bounds = Manny->GetImportedBounds().GetBox();
		const FVector Eye(HeadInMesh.GetLocation().X - 3.2, HeadInMesh.GetLocation().Y + 8.5, Bounds.Max.Z - 11.5);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Head bone at %s, crown at %.1f: right eye at %s"), *HeadInMesh.GetLocation().ToCompactString(), Bounds.Max.Z, *Eye.ToCompactString());
		return FTransform(Eye).GetRelativeTransform(HeadInMesh);
	}

	/**
	 * Where a rifle butt goes, on clavicle_r (so it follows the shoulder as it lifts to aim): in front of the right
	 * shoulder joint and a little inside it, worked out in Manny's mesh space (he faces +Y; his right is -X).
	 */
	FTransform ComputeShoulderPocket(const USkeleton* Skeleton)
	{
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const int32 Shoulder = Reference.FindBoneIndex(TEXT("upperarm_r"));
		const int32 Clavicle = Reference.FindBoneIndex(TEXT("clavicle_r"));
		if (Shoulder == INDEX_NONE || Clavicle == INDEX_NONE)
		{
			return FTransform::Identity;
		}
		const FVector Joint = FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, Shoulder).GetLocation();
		const FTransform Pocket(Joint + FVector(5.0, 6.0, 0.0));
		return Pocket.GetRelativeTransform(FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, Clavicle));
	}

	void SetSkeletonSocket(USkeleton* Skeleton, FName Name, FName Bone, const FTransform& Relative, bool bOverwrite)
	{
		USkeletalMeshSocket* Socket = Skeleton->FindSocket(Name);
		if (Socket && !bOverwrite)
		{
			return;
		}
		if (!Socket)
		{
			Socket = NewObject<USkeletalMeshSocket>(Skeleton);
			Socket->SocketName = Name;
			Skeleton->Sockets.Add(Socket);
		}
		Socket->BoneName = Bone;
		Socket->RelativeLocation = Relative.GetLocation();
		Socket->RelativeRotation = Relative.Rotator();
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Socket %s on %s: %s"), *Name.ToString(), *Bone.ToString(), *Describe(Relative));
	}

	void SetMeshSocket(UStaticMesh* Mesh, FName Name, const FTransform& Relative, bool bOverwrite)
	{
		if (!Mesh)
		{
			return;
		}
		UStaticMeshSocket* Socket = Mesh->FindSocket(Name);
		if (Socket && !bOverwrite)
		{
			return;
		}
		if (!Socket)
		{
			Socket = NewObject<UStaticMeshSocket>(Mesh);
			Socket->SocketName = Name;
			Mesh->AddSocket(Socket);
		}
		Socket->RelativeLocation = Relative.GetLocation();
		Socket->RelativeRotation = Relative.Rotator();
		Save(Mesh);
	}

	void BuildRifle()
	{
		const bool bRebuild = FParse::Param(FCommandLine::Get(), TEXT("rebuild"));
		// Sockets are only added where missing (they get tuned by hand); -resetsockets works them out again.
		const bool bResetSockets = FParse::Param(FCommandLine::Get(), TEXT("resetsockets"));
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		if (!Skeleton)
		{
			return;
		}

		// The upper-body slot (fire, reload, draw while the legs walk).
		if (!Skeleton->ContainsSlotName(UpperBodySlot))
		{
			Skeleton->SetSlotGroupName(UpperBodySlot, FAnimSlotGroup::DefaultGroupName);
		}

		// Sockets: the hand as the pack holds its rifle, the back sling, the left hand's place on the rifles.
		FTransform HandSocket, LeftPalmSocket;
		FVector LeftGripInRifle;
		const bool bHand = ComputeHandSocket(HandSocket, LeftPalmSocket, LeftGripInRifle);
		if (bHand)
		{
			SetSkeletonSocket(Skeleton, TEXT("hand_r_weapon"), TEXT("hand_r"), HandSocket, bResetSockets);
			SetSkeletonSocket(Skeleton, TEXT("hand_l_weapon"), TEXT("hand_l"), LeftPalmSocket, bResetSockets);
		}
		SetSkeletonSocket(Skeleton, TEXT("Sheath_BackSling"), TEXT("spine_04"), ComputeBackSling(Skeleton, TEXT("spine_04")), bResetSockets);
		SetSkeletonSocket(Skeleton, TEXT("ShoulderPocket"), TEXT("clavicle_r"), ComputeShoulderPocket(Skeleton), bResetSockets);
		SetSkeletonSocket(Skeleton, TEXT("Eye_R"), TEXT("head"), ComputeRightEye(Skeleton), bResetSockets);
		// The weapon bone the held weapon rides on (the anim sets ik_hand_gun to the right palm, or holds the weapon there).
		SetSkeletonSocket(Skeleton, TEXT("WeaponHeld"), TEXT("ik_hand_gun"), FTransform::Identity, bResetSockets);
		Save(Skeleton);

		// The grey-box long guns, from their shapes: LeftGrip under the fore-end (the palm's middle a couple of cm below
		// the wood, a little to the left), Sight (the line of the sights) and Stock (the butt plate) for the shoulder aim.
		// Each mesh has its own: the pack's rifle is shaped differently, so only the palm (hand_l_weapon) comes from it.
		struct FLongGun { const TCHAR* Name; FVector LeftGrip; FVector Sight; FVector Stock; };
		for (const FLongGun& Gun : { FLongGun{ TEXT("SM_GB_KragRifle"), FVector(28.0, -1.0, -5.0), FVector(14.0, 0.0, 4.5), FVector(-40.0, 0.0, -2.0) },
				 FLongGun{ TEXT("SM_GB_MuzzleLoader"), FVector(30.0, -1.0, -5.0), FVector(8.0, 0.0, 4.5), FVector(-40.0, 0.0, -2.0) },
				 FLongGun{ TEXT("SM_GB_Crossbow"), FVector(12.0, -1.0, -6.5), FVector(-4.0, 0.0, 1.5), FVector(-30.0, 0.0, -2.0) } })
		{
			UStaticMesh* Mesh = LoadAt<UStaticMesh>(FString(TEXT("/Game/Vaesen/Combat/GreyBox")) / Gun.Name);
			if (!Mesh)
			{
				continue;
			}
			// Where the left palm holds it (the older position-only LeftHandIK goes).
			SetMeshSocket(Mesh, TEXT("LeftGrip"), FTransform(Gun.LeftGrip), bResetSockets);
			if (UStaticMeshSocket* Old = Mesh->FindSocket(TEXT("LeftHandIK")))
			{
				Mesh->RemoveSocket(Old);
				Save(Mesh);
			}
			SetMeshSocket(Mesh, TEXT("Sight"), FTransform(Gun.Sight), false);
			SetMeshSocket(Mesh, TEXT("Stock"), FTransform(Gun.Stock), false);
		}
		// The Krag's bolt knob, at the end of its handle (the hand works the bolt there).
		SetMeshSocket(LoadAt<UStaticMesh>(TEXT("/Game/Vaesen/Combat/GreyBox/SM_GB_KragRifle")), TEXT("Bolt"), FTransform(FVector(10.0, 5.0, 4.0)), false);

		// Montages on the upper body.
		const FString Folder = RifleFolder;
		UAnimMontage* FireHip = MakeMontage(Folder / TEXT("AM_Rifle_FireHip"), RifleAnim(TEXT("AS_Rifle_Fire")), UpperBodySlot, 0.05f, 0.2f);
		UAnimMontage* FireAimed = MakeMontage(Folder / TEXT("AM_Rifle_FireAimed"), RifleAnim(TEXT("AS_Rifle_Fire_Aim")), UpperBodySlot, 0.05f, 0.2f);
		UAnimMontage* Reload = MakeMontage(Folder / TEXT("AM_Rifle_Reload"), RifleAnim(TEXT("AS_Rifle_ReloadLoaded")), UpperBodySlot, 0.2f, 0.25f);
		UAnimMontage* ReloadEmpty = MakeMontage(Folder / TEXT("AM_Rifle_ReloadEmpty"), RifleAnim(TEXT("AS_Rifle_ReloadEmpty")), UpperBodySlot, 0.2f, 0.25f);
		for (UAnimMontage* Montage : { FireHip, FireAimed, Reload, ReloadEmpty })
		{
			if (Montage)
			{
				Save(Montage);
			}
		}

		// Drawing from the back sling: the pack's equip starts at low ready, so the hand first reaches back for the
		// rifle's grip (HandIK_Sheath up), takes it (AN_WeaponGrab), and the equip brings it forward as the IK lets go.
		const bool bNewDraw = !Exists(Folder / TEXT("AM_Rifle_DrawFromBack"));
		UAnimSequence* DrawAnim = CopyWithSheathCurve(Folder / TEXT("AS_Rifle_DrawFromBack"), RifleAnim(TEXT("AS_Rifle_Equip")),
			{ { 0.0f, 0.0f }, { 0.24f, 1.0f }, { 0.32f, 1.0f }, { 0.62f, 0.0f } });
		UAnimMontage* Draw = MakeMontage(Folder / TEXT("AM_Rifle_DrawFromBack"), DrawAnim, UpperBodySlot, 0.15f, 0.2f, 0.8f);
		if (Draw && bNewDraw)
		{
			AddNotify(Draw, NewObject<UAnimNotify_VaesenWeaponGrab>(Draw), 0.28f);
			Save(Draw);
		}
		// Sheathing: the unequip lowers it, the hand takes it back over the shoulder (IK up) and lets go at the sling.
		const bool bNewSheath = !Exists(Folder / TEXT("AM_Rifle_SheathToBack"));
		UAnimSequence* SheathAnim = CopyWithSheathCurve(Folder / TEXT("AS_Rifle_SheathToBack"), RifleAnim(TEXT("AS_Rifle_UnEquip")),
			{ { 0.0f, 0.0f }, { 0.35f, 0.0f }, { 0.6f, 1.0f }, { 0.68f, 1.0f }, { 0.77f, 0.0f } });
		UAnimMontage* Sheath = MakeMontage(Folder / TEXT("AM_Rifle_SheathToBack"), SheathAnim, UpperBodySlot, 0.15f, 0.25f, 0.8f);
		if (Sheath && bNewSheath)
		{
			AddNotify(Sheath, NewObject<UAnimNotify_VaesenWeaponRelease>(Sheath), 0.64f);
			Save(Sheath);
		}

		// The anim Blueprints.
		UClass* RifleLocomotion = BuildRifleLocomotion(Skeleton, bRebuild);
		UClass* CharacterGraph = BuildCharacterGraph(Skeleton, bRebuild);

		// Category defaults: rifles (and crossbows, held the same way) use all of it; bare hands use ABP_Manny.
		UVaesenWeaponCategoryDefaults* Defaults = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"));
		if (Defaults)
		{
			// Who parries and how hard it wears them: the built-in rules (the asset keeps whole copies of each category,
			// so new rules don't reach it by themselves).
			const UVaesenWeaponCategoryDefaults* BuiltIn = GetDefault<UVaesenWeaponCategoryDefaults>();
			for (TPair<EVaesenWeaponCategory, FVaesenCategoryDefaults>& Pair : Defaults->Categories)
			{
				if (const FVaesenCategoryDefaults* Rules = BuiltIn->Categories.Find(Pair.Key))
				{
					Pair.Value.bCanParry = Rules->bCanParry;
					Pair.Value.bProceduralGuard = Rules->bProceduralGuard;
					Pair.Value.BlockProfile.WearScale = Rules->BlockProfile.WearScale;
					Pair.Value.BlockProfile.ParryWearShare = Rules->BlockProfile.ParryWearShare;
				}
			}
			for (const EVaesenWeaponCategory Category : { EVaesenWeaponCategory::Rifle, EVaesenWeaponCategory::Crossbow })
			{
				FVaesenCategoryDefaults& Entry = Defaults->Categories.FindOrAdd(Category);
				Entry.AnimLayer = RifleLocomotion;
				Entry.Montages.Fire = FireHip;
				Entry.Montages.FireAimed = FireAimed;
				Entry.Montages.Reload = Reload;
				Entry.Montages.ReloadEmpty = ReloadEmpty;
				// The fingers as the pack wraps them round its rifle (both hands, aimed).
				Entry.GripPose = RifleAnim(TEXT("AS_Rifle_Aim"));
			}
			Defaults->Categories.FindOrAdd(EVaesenWeaponCategory::Unarmed).AnimLayer = LoadObject<UClass>(nullptr, UnarmedGraphPath);
			if (FVaesenSheathLocation* Sling = Defaults->SheathLocations.Find(TEXT("Sheath_BackSling")))
			{
				Sling->DrawMontage = Draw;
				Sling->SheathMontage = Sheath;
				Sling->ReferenceGrabPoint = FVector::ZeroVector;
			}
			Save(Defaults);
		}

		// The player uses the character graph.
		if (UBlueprint* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Vaesen/Blueprints/Characters/BP_VaesenPlayerCharacter.BP_VaesenPlayerCharacter")))
		{
			AVaesenPlayerCharacter* PlayerDefaults = Cast<AVaesenPlayerCharacter>(Player->GeneratedClass->GetDefaultObject());
			if (PlayerDefaults && CharacterGraph && PlayerDefaults->GetMesh()->AnimClass != CharacterGraph)
			{
				PlayerDefaults->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
				PlayerDefaults->GetMesh()->AnimClass = CharacterGraph;
				FBlueprintEditorUtils::MarkBlueprintAsModified(Player);
				FKismetEditorUtilities::CompileBlueprint(Player);
				Save(Player);
			}
		}

		// The Krag's clip reload: the pack's reload is long; 4 s keeps it from looking hurried.
		if (UBlueprint* Krag = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Vaesen/Blueprints/Items/Weapons/BP_Weapon_KragRifle.BP_Weapon_KragRifle")))
		{
			UVaesenRangedWeaponItem* KragDefaults = Cast<UVaesenRangedWeaponItem>(Krag->GeneratedClass->GetDefaultObject());
			if (KragDefaults && KragDefaults->ReloadTime < 4.0f)
			{
				KragDefaults->ReloadTime = 4.0f;
				FBlueprintEditorUtils::MarkBlueprintAsModified(Krag);
				FKismetEditorUtilities::CompileBlueprint(Krag);
				Save(Krag);
			}
		}
	}

	// --- The one-handed sword ---

	const TCHAR* SwordGraphPath = TEXT("/Game/Vaesen/Animations/Sword/ABP_SwordLocomotion");

	UAnimSequence* SwordAnim(const TCHAR* Name)
	{
		UAnimSequence* Anim = LoadAt<UAnimSequence>(FString(SwordFolder) / Name);
		if (!Anim)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Retargeted %s is missing."), Name);
		}
		return Anim;
	}

	/**
	 * The pack's animations on Manny, in /Game/Vaesen/Animations/Sword (the right-handed ones; the mirrored
	 * left-handed copies stay in the pack until there are left-handed fighters). Locomotion, attacks, blocks and hit
	 * reactions stay in place (the capsule moves the character); the evades keep their root motion (they carry it).
	 */
	bool RetargetSwordPack()
	{
		if (Exists(FString(SwordFolder) / TEXT("1HM_Idle_Standing")))
		{
			return true;
		}
		USkeletalMesh* Source = LoadObject<USkeletalMesh>(nullptr, SwordPackMesh);
		USkeletalMesh* Target = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
		if (!Source || !Target)
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Sword pack (/Game/1HM_Sword_Attacks_Finishers) not in the project; skipping the sword."));
			return false;
		}
		// The same UE4 mannequin -> Manny retargeter as the rifle's (the pack's mannequin has the same bones).
		USkeletalMesh* RigSource = LoadObject<USkeletalMesh>(nullptr, UE4MannequinMesh);
		UIKRetargeter* Retargeter = MakeRetargeter(RigSource ? RigSource : Source, Target);

		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.SearchAllAssets(true);
		TArray<FAssetData> Found;
		Registry.GetAssetsByPath(FName(SwordPackFolder), Found, true);
		FIKRetargetBatchOperationInputs Inputs;
		for (const FAssetData& Asset : Found)
		{
			if (Asset.IsInstanceOf(UAnimSequence::StaticClass()) && !Asset.AssetName.ToString().Contains(TEXT("Mirrored")))
			{
				Inputs.AssetsToRetarget.Add(Asset);
			}
		}
		Inputs.SourceMesh = Source;
		Inputs.TargetMesh = Target;
		Inputs.IKRetargetAsset = Retargeter;
		Inputs.TargetPath = SwordFolder;
		Inputs.bUseSourcePath = false;
		Inputs.bIncludeReferencedAssets = false;
		Inputs.bOverwriteExistingFiles = true;
		const TArray<FAssetData> Results = UIKRetargetBatchOperation::RunBatchRetarget(Inputs);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Retargeted %d of %d sword animations to Manny."), Results.Num(), Inputs.AssetsToRetarget.Num());

		for (const FAssetData& Result : Results)
		{
			if (UAnimSequence* Anim = Cast<UAnimSequence>(Result.GetAsset()))
			{
				const bool bCarries = Anim->GetName().Contains(TEXT("Evade"));
				Anim->bEnableRootMotion = bCarries;
				Anim->RootMotionRootLock = ERootMotionRootLock::RefPose;
				Anim->bForceRootLock = !bCarries;
				Anim->PostEditChange();
				Save(Anim);
			}
		}
		return Exists(FString(SwordFolder) / TEXT("1HM_Idle_Standing"));
	}

	/**
	 * The pack's sword as its 1HMBladeMan Blueprint holds it (the mannequin with OneHandSword_Mesh attached to the
	 * hand, at an offset of its own), in hand_r's space: the blade's direction (the mesh's Z), the way its edge faces
	 * (its Y), and the point on it where the palm closes.
	 */
	bool BladeInHand(FVector& OutBlade, FVector& OutEdge, FVector& OutGrip)
	{
		const UBlueprint* BladeMan = LoadObject<UBlueprint>(nullptr, TEXT("/Game/1HM_Sword_Attacks_Finishers/Mannequin/Character/Mesh/1HMBladeMan.1HMBladeMan"));
		const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, SwordPackMesh);
		if (!BladeMan || !BladeMan->SimpleConstructionScript || !Mesh)
		{
			return false;
		}
		const USCS_Node* SwordNode = nullptr;
		for (const USCS_Node* Node : BladeMan->SimpleConstructionScript->GetAllNodes())
		{
			if (const UStaticMeshComponent* Component = Node ? Cast<UStaticMeshComponent>(Node->ComponentTemplate) : nullptr; Component && Component->GetStaticMesh())
			{
				SwordNode = Node;
			}
		}
		if (!SwordNode)
		{
			return false;
		}
		const USceneComponent* Sword = Cast<USceneComponent>(SwordNode->ComponentTemplate);
		// On a socket of the mannequin, or on the bone itself.
		FTransform OnHand = Sword->GetRelativeTransform();
		OnHand.SetScale3D(FVector::OneVector);
		FName Bone = SwordNode->AttachToName;
		if (const USkeletalMeshSocket* Socket = Mesh->FindSocket(SwordNode->AttachToName))
		{
			OnHand = OnHand * Socket->GetSocketLocalTransform();
			Bone = Socket->BoneName;
		}
		const FReferenceSkeleton& Reference = Mesh->GetRefSkeleton();
		const int32 HandIndex = Reference.FindBoneIndex(TEXT("hand_r"));
		const int32 KnuckleIndex = Reference.FindBoneIndex(TEXT("middle_01_r"));
		if (Bone != TEXT("hand_r") || HandIndex == INDEX_NONE || KnuckleIndex == INDEX_NONE)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("1HMBladeMan's sword is on %s, not the right hand."), *SwordNode->AttachToName.ToString());
			return false;
		}
		const FTransform Hand = FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, HandIndex);
		const FVector Knuckle = Hand.InverseTransformPosition(FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, KnuckleIndex).GetLocation());
		const FVector Palm = Knuckle * 0.5;

		OutBlade = OnHand.GetUnitAxis(EAxis::Z);
		OutEdge = OnHand.GetUnitAxis(EAxis::Y);
		OutGrip = OnHand.GetLocation() + OutBlade * FVector::DotProduct(Palm - OnHand.GetLocation(), OutBlade);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("1HMBladeMan: sword on %s at %s; in hand_r: blade %s edge %s grip %s"), *SwordNode->AttachToName.ToString(),
			*Describe(Sword->GetRelativeTransform()), *OutBlade.ToCompactString(), *OutEdge.ToCompactString(), *OutGrip.ToCompactString());
		return true;
	}

	/**
	 * How the pack holds its sword, as the category's InHand: the blade as 1HMBladeMan holds it (its WeaponSocket
	 * doesn't match: a sword there points out sideways in every pose); ours have the blade along X and the edges along
	 * Z, held by the Grip socket. Seen from Manny's retargeted hand (as the rifle's hand socket is).
	 */
	bool ComputeSwordInHand(const USkeleton* Skeleton, FTransform& OutInHand)
	{
		const UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Idle/1HM_Idle_Standing.1HM_Idle_Standing"));
		const UAnimSequence* Target = SwordAnim(TEXT("1HM_Idle_Standing"));
		const USkeletalMeshSocket* Palm = Skeleton->FindSocket(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket);
		FVector BladeLocal, EdgeLocal, GripLocal;
		if (!Source || !Target || !Palm || !BladeInHand(BladeLocal, EdgeLocal, GripLocal))
		{
			return false;
		}
		const FAnimPose SourcePose = PoseAt(Source, 0.0);
		const FAnimPose TargetPose = PoseAt(Target, 0.0);
		const FTransform Hand4 = UAnimPoseExtensions::GetBonePose(SourcePose, TEXT("hand_r"), EAnimPoseSpaces::World);
		const FTransform Hand5 = UAnimPoseExtensions::GetBonePose(TargetPose, TEXT("hand_r"), EAnimPoseSpaces::World);

		const FQuat Ours4 = FRotationMatrix::MakeFromXZ(Hand4.TransformVectorNoScale(BladeLocal), Hand4.TransformVectorNoScale(EdgeLocal)).ToQuat();
		const FVector Grip4 = Hand4.TransformPosition(GripLocal);

		FTransform Desired;
		Desired.SetRotation(Hand5.GetRotation().Inverse() * Ours4);
		Desired.SetLocation(Hand5.GetRotation().UnrotateVector(Grip4 - Hand4.GetLocation()));
		// In the node the weapon is InHand * hand socket * hand.
		OutInHand = Desired * Palm->GetSocketLocalTransform().Inverse();
		OutInHand.SetScale3D(FVector::OneVector);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Sword in the hand %s (category InHand %s); blade points %s"), *Describe(Desired), *Describe(OutInHand),
			*(Hand5.GetRotation() * Desired.GetRotation()).GetForwardVector().ToCompactString());
		return true;
	}

	/** Only part of the montage's animation plays: Start..End of it. */
	void TrimMontage(UAnimMontage* Montage, float Start, float End)
	{
		if (!Montage || Montage->SlotAnimTracks.Num() == 0 || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 0)
		{
			return;
		}
		FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
		Segment.AnimStartTime = Start;
		Segment.AnimEndTime = End;
		Segment.StartPos = 0.0f;
		Montage->SetCompositeLength(End - Start);
		if (Montage->CompositeSections.Num() > 0)
		{
			Montage->CompositeSections[0].SetTime(0.0f);
		}
		Montage->RefreshCacheData();
	}

	void AddNotifyState(UAnimMontage* Montage, UAnimNotifyState* State, float Start, float Duration)
	{
		if (Montage->AnimNotifyTracks.Num() == 0)
		{
			Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White));
		}
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyName = FName(State->GetNotifyName());
		Event.NotifyStateClass = State;
		Event.Link(Montage, Start);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Montage->CalculateOffsetForNotify(Start));
		Event.SetDuration(Duration);
		Event.EndLink.Link(Montage, Start + Duration);
		Event.TrackIndex = 0;
		Montage->RefreshCacheData();
	}

	/** How far ahead of its moment a sound starts (s): whooshes and blades sliding build up to their peak. */
	const float SoundLead = 0.15f;

	/** Sets a Weapon Sound notify for Sound at Time (moving one already there). True if it changed. */
	bool SetSoundNotify(UAnimMontage* Montage, EVaesenWeaponSound Sound, float Time)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const UAnimNotify_VaesenWeaponSound* Notify = Cast<UAnimNotify_VaesenWeaponSound>(Event.Notify);
			if (Notify && Notify->Sound == Sound && FMath::IsNearlyEqual(Event.GetTime(), Time, 0.005f))
			{
				return false;
			}
		}
		Montage->Notifies.RemoveAll([Sound](const FAnimNotifyEvent& Event)
		{
			const UAnimNotify_VaesenWeaponSound* Notify = Cast<UAnimNotify_VaesenWeaponSound>(Event.Notify);
			return Notify && Notify->Sound == Sound;
		});
		UAnimNotify_VaesenWeaponSound* Notify = NewObject<UAnimNotify_VaesenWeaponSound>(Montage);
		Notify->Sound = Sound;
		AddNotify(Montage, Notify, Time);
		return true;
	}

	/** Sets the Effort Vocal notify at Time (moving one already there). True if it changed. */
	bool SetEffortNotify(UAnimMontage* Montage, float Time)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (Cast<UAnimNotify_VaesenEffortVocal>(Event.Notify) && FMath::IsNearlyEqual(Event.GetTime(), Time, 0.005f))
			{
				return false;
			}
		}
		Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& Event) { return Cast<UAnimNotify_VaesenEffortVocal>(Event.Notify) != nullptr; });
		AddNotify(Montage, NewObject<UAnimNotify_VaesenEffortVocal>(Montage), Time);
		return true;
	}

	/** An attack's montage (made once): the hit window where the sword hand moves fastest, the whoosh as it opens. */
	UAnimMontage* MakeSwingMontage(const TCHAR* Anim, bool bHeavy, float* OutWindowEnd = nullptr, float* OutWindowStart = nullptr)
	{
		UAnimSequence* Sequence = SwordAnim(Anim);
		if (!Sequence)
		{
			return nullptr;
		}
		const FSwingTiming Timing = MeasureSwing(Sequence);
		// A little either side of the fastest stretch: the blade is dangerous a moment before and after its peak.
		const float Start = FMath::Max(Timing.WindowStart - 0.06f, 0.0f);
		const float End = FMath::Min(Timing.WindowEnd + 0.06f, Timing.Length);
		if (OutWindowEnd)
		{
			*OutWindowEnd = End;
		}
		if (OutWindowStart)
		{
			*OutWindowStart = Start;
		}
		const FString Path = FString(SwordFolder) / (FString(TEXT("AM_")) + Anim);
		const bool bNew = !Exists(Path);
		// Full body: the hips and legs step into the swing (it reaches as far as the pack means it to).
		UAnimMontage* Montage = MakeMontage(Path, Sequence, FAnimSlotGroup::DefaultSlotName, 0.1f, 0.3f);
		if (Montage && bNew)
		{
			AddNotifyState(Montage, NewObject<UAnimNotifyState_VaesenMeleeHitWindow>(Montage), Start, End - Start);
			Save(Montage);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s: hit window %.2f..%.2f of %.2f s"), *Montage->GetName(), Start, End, Timing.Length);
		}
		// The whoosh leads the blade (a whoosh builds before its peak), and the heavy swing's grunt a moment before.
		if (Montage)
		{
			const float WhooshAt = FMath::Max(Start - SoundLead, 0.0f);
			bool bChanged = SetSoundNotify(Montage, bHeavy ? EVaesenWeaponSound::SwingHeavy : EVaesenWeaponSound::SwingLight, WhooshAt);
			if (bHeavy)
			{
				bChanged |= SetEffortNotify(Montage, FMath::Max(WhooshAt - 0.07f, 0.0f));
			}
			if (bChanged)
			{
				Save(Montage);
			}
		}
		return Montage;
	}

	/** A montage of part of an animation (made once), for the guard and its impacts. */
	UAnimMontage* MakeTrimmedMontage(const TCHAR* Name, UAnimSequence* Sequence, FName Slot, float Start, float End, float BlendIn, float BlendOut)
	{
		const FString Path = FString(SwordFolder) / Name;
		const bool bNew = !Exists(Path);
		UAnimMontage* Montage = MakeMontage(Path, Sequence, Slot, BlendIn, BlendOut);
		if (Montage && bNew)
		{
			TrimMontage(Montage, Start, End);
			Save(Montage);
		}
		return Montage;
	}

	/** BS_1HM_Locomotion: idle, walk and run in eight directions, set like the rifle's (Direction, Speed), at each animation's own speed. */
	UBlendSpace* MakeSwordBlendSpace(bool bRebuild)
	{
		const FString Path = FString(SwordFolder) / TEXT("BS_1HM_Locomotion");
		UBlendSpace* Space = LoadAt<UBlendSpace>(Path);
		// The samples' speeds are measured in the pack's own animations: without the pack, keep what's there.
		const bool bPack = FPackageName::DoesPackageExist(FString(SwordPackFolder) / TEXT("1HM_Walk/1HM_WalkForward"));
		if (Space && (!bRebuild || !bPack))
		{
			return Space;
		}
		if (!Space)
		{
			UBlendSpace* Rifle = RifleAsset<UBlendSpace>(TEXT("BS_Rifle"));
			if (!Rifle)
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*Path);
			Space = DuplicateObject<UBlendSpace>(Rifle, Package, FName(TEXT("BS_1HM_Locomotion")));
			Space->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Space);
		}
		while (Space->GetNumberOfBlendSamples() > 0)
		{
			Space->DeleteSample(0);
		}

		struct FSample { const TCHAR* Name; float Direction; };
		const FSample Moving[] = {
			{ TEXT("Forward"), 0.0f }, { TEXT("ForwardRight"), 45.0f }, { TEXT("Right"), 90.0f }, { TEXT("BackRight"), 135.0f },
			{ TEXT("Back"), 180.0f }, { TEXT("Back"), -180.0f }, { TEXT("BackLeft"), -135.0f }, { TEXT("Left"), -90.0f }, { TEXT("ForwardLeft"), -45.0f } };
		const auto Add = [Space](UAnimSequence* Anim, float Direction, float Speed)
		{
			if (Anim)
			{
				const FVector Value(Direction, Speed, 0.0);
				Space->ExpandRangeForSample(Value);
				Space->AddSample(Anim, Value);
			}
		};
		UAnimSequence* Idle = SwordAnim(TEXT("1HM_Idle_Standing"));
		for (const float Direction : { -180.0f, -90.0f, 0.0f, 90.0f, 180.0f })
		{
			Add(Idle, Direction, 0.0f);
		}
		for (const TCHAR* Gait : { TEXT("Walk"), TEXT("Run") })
		{
			for (const FSample& Sample : Moving)
			{
				const FString Name = FString::Printf(TEXT("1HM_%s%s"), Gait, Sample.Name);
				UAnimSequence* Anim = SwordAnim(*Name);
				if (const UAnimSequence* Original = Anim ? LoadObject<UAnimSequence>(nullptr, *FString::Printf(TEXT("%s/1HM_%s/%s.%s"), SwordPackFolder, Gait, *Name, *Name)) : nullptr)
				{
					// The speed it walks at (measured in the pack's own animation; ours has the root locked in place).
					const FSwingTiming Timing = MeasureSwing(Original);
					Add(Anim, Sample.Direction, FMath::RoundToFloat(Timing.RootMove.Size2D() / Timing.Length));
				}
			}
		}
		Space->ValidateSampleData();
		Space->ResampleData();
		Space->PostEditChange();
		Save(Space);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("BS_1HM_Locomotion: %d samples, speed up to %.0f"), Space->GetNumberOfBlendSamples(), Space->GetBlendParameter(1).Max);
		return Space;
	}

	/**
	 * ABP_SwordLocomotion: the sword's stance, linked into the character's graph while a one-handed sword is drawn.
	 * Idle, walk and run (a direction/speed blend space), the jump loop in the air, and the guard over the upper body
	 * while blocking (the pack's block held where it settles).
	 */
	UClass* BuildSwordLocomotion(USkeleton* Skeleton, bool bRebuild, UBlendSpace* Movement, UAnimSequence* GuardAnim, float GuardTime)
	{
		if (!bRebuild)
		{
			if (UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(SwordGraphPath))
			{
				return Existing->GeneratedClass;
			}
		}
		UAnimBlueprint* Blueprint = OpenAnimBlueprint(SwordGraphPath, Skeleton);
		FGraphBuilder Builder(Blueprint);
		if (!Blueprint || !Builder.Graph)
		{
			return nullptr;
		}

		UAnimGraphNode_BlendSpacePlayer* Walking = Builder.Add<UAnimGraphNode_BlendSpacePlayer>(-1600, 0, [Movement](UAnimGraphNode_BlendSpacePlayer* Node)
		{
			Node->Node.SetBlendSpace(Movement);
		});
		Builder.Var(TEXT("Direction"), Walking, TEXT("X"));
		Builder.Var(TEXT("Speed"), Walking, TEXT("Y"));
		UAnimSequence* JumpLoop = SwordAnim(TEXT("1HM_JumpInPlace_Loop"));
		UAnimGraphNode_SequencePlayer* Air = Builder.Add<UAnimGraphNode_SequencePlayer>(-1600, 300, [JumpLoop](UAnimGraphNode_SequencePlayer* Node)
		{
			Node->Node.SetSequence(JumpLoop);
		});
		UEdGraphNode* Stance = ByBool(Builder, -1250, 100, TEXT("bFalling"), Air, Walking, 0.15f);

		// Not squared up to fight (not strafing): the plain locomotion (ABP_Manny), standing, running and jumping, arms
		// and all. The pack's stance (right foot forward, a fencer's footwork) is for squaring up: guarding, aiming and
		// a later target lock.
		UClass* Plain = LoadObject<UClass>(nullptr, UnarmedGraphPath);
		UAnimGraphNode_LinkedAnimGraph* Running = Builder.Add<UAnimGraphNode_LinkedAnimGraph>(-1600, -300, [Plain](UAnimGraphNode_LinkedAnimGraph* Node)
		{
			Node->Node.InstanceClass = Plain;
		});
		UEdGraphNode* Moving = ByBool(Builder, -1100, 0, TEXT("bRelaxed"), Running, Stance, 0.3f);
		UAnimGraphNode_SaveCachedPose* Cache = Builder.Add<UAnimGraphNode_SaveCachedPose>(-1000, 100, [](UAnimGraphNode_SaveCachedPose* Node)
		{
			Node->CacheName = TEXT("SwordLocomotion");
		});
		Builder.Pose(Moving, Cache, TEXT("Pose"));
		const auto UseCache = [&Builder, Cache](int32 Y)
		{
			return Builder.Add<UAnimGraphNode_UseCachedPose>(-800, Y, [Cache](UAnimGraphNode_UseCachedPose* Node)
			{
				Node->SaveCachedPoseNode = Cache;
			});
		};

		// The guard: the block's pose where it settles, from the spine up (the legs keep walking).
		UAnimGraphNode_SequenceEvaluator* Guard = Builder.Add<UAnimGraphNode_SequenceEvaluator>(-800, 400, [GuardAnim, GuardTime](UAnimGraphNode_SequenceEvaluator* Node)
		{
			Node->Node.SetSequence(GuardAnim);
			Node->Node.SetExplicitTime(GuardTime);
		});
		UAnimGraphNode_LayeredBoneBlend* Guarded = Builder.Add<UAnimGraphNode_LayeredBoneBlend>(-500, 250, [](UAnimGraphNode_LayeredBoneBlend* Node)
		{
			Node->Node.bMeshSpaceRotationBlend = true;
			if (Node->Node.LayerSetup.Num() == 0)
			{
				Node->Node.LayerSetup.AddDefaulted();
			}
			FBranchFilter Filter;
			Filter.BoneName = TEXT("spine_01");
			Filter.BlendDepth = 0;
			Node->Node.LayerSetup[0].BranchFilters = { Filter };
		});
		Builder.Pose(UseCache(200), Guarded, TEXT("BasePose"));
		Builder.Pose(Guard, Guarded, TEXT("BlendPoses_0"));

		UEdGraphNode* Final = ByBool(Builder, -250, 100, TEXT("bBlocking"), Guarded, UseCache(0), 0.15f);
		Builder.Pose(Final, Builder.Root(), TEXT("Result"));

		CompileAndSave(Blueprint);
		return Blueprint->GeneratedClass;
	}

	void BuildSword()
	{
		const bool bRebuild = FParse::Param(FCommandLine::Get(), TEXT("rebuild"));
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		UVaesenWeaponCategoryDefaults* Defaults = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"));
		if (!Skeleton || !Defaults || !RetargetSwordPack())
		{
			return;
		}
		const FString Folder = SwordFolder;

		// Swings play with root motion: the animation moves the body (they stand their ground), not the input, so
		// nobody glides across the floor mid-swing.
		for (const TCHAR* Name : { TEXT("1HM_Slash01"), TEXT("1HM_Slash02"), TEXT("1HM_Slash03"), TEXT("1HM_Slash04"), TEXT("1HM_Slash01_Blocked"),
				 TEXT("1HM_Slash02_Blocked"), TEXT("1HM_Slash03_Blocked"), TEXT("1HM_Slash04_Blocked"), TEXT("1HM_Stab01"), TEXT("1HM_Stab02"), TEXT("1HM_Stab03") })
		{
			UAnimSequence* Anim = SwordAnim(Name);
			if (Anim && (!Anim->bEnableRootMotion || Anim->bForceRootLock))
			{
				Anim->bEnableRootMotion = true;
				Anim->RootMotionRootLock = ERootMotionRootLock::RefPose;
				Anim->bForceRootLock = false;
				Anim->PostEditChange();
				Save(Anim);
			}
		}

		// Attacks: light = the four slashes in turn, heavy = the three stabs in turn.
		struct FSwing { const TCHAR* Anim; const TCHAR* Blocked; };
		const auto MakeCombo = [](TArrayView<const FSwing> Swings, bool bHeavy, FVaesenMeleeAttack& Attack)
		{
			float Recovery = 0.0f, FirstStart = 0.0f, FirstEnd = 0.0f;
			Attack.ComboSteps.Reset();
			for (int32 Index = 0; Index < Swings.Num(); ++Index)
			{
				float WindowStart = 0.0f, WindowEnd = 0.0f;
				UAnimMontage* Swing = MakeSwingMontage(Swings[Index].Anim, bHeavy, &WindowEnd, &WindowStart);
				UAnimMontage* Bounce = Swings[Index].Blocked
					? MakeMontage(FString(SwordFolder) / (FString(TEXT("AM_")) + Swings[Index].Blocked), SwordAnim(Swings[Index].Blocked), FAnimSlotGroup::DefaultSlotName, 0.08f, 0.3f) : nullptr;
				if (Bounce)
				{
					Save(Bounce);
				}
				// Over a moment after the blade has passed (the next swing may start; the follow-through plays out).
				Recovery = FMath::Max(Recovery, WindowEnd + 0.2f);
				if (Index == 0)
				{
					Attack.Montage = Swing;
					Attack.BlockedMontage = Bounce;
					FirstStart = WindowStart;
					FirstEnd = WindowEnd;
				}
				else
				{
					FVaesenComboStep& Step = Attack.ComboSteps.AddDefaulted_GetRef();
					Step.Montage = Swing;
					Step.BlockedMontage = Bounce;
				}
			}
			Attack.MontageSection = NAME_None;
			Attack.RecoveryTime = Recovery;
			Attack.ComboWindow = 0.6f;
			Attack.HitWindowStart = FirstStart;
			Attack.HitWindowEnd = FirstEnd;
		};

		FVaesenCategoryDefaults& Sword = Defaults->Categories.FindOrAdd(EVaesenWeaponCategory::OneHandedSword);
		const FSwing Slashes[] = { { TEXT("1HM_Slash01"), TEXT("1HM_Slash01_Blocked") }, { TEXT("1HM_Slash02"), TEXT("1HM_Slash02_Blocked") },
			{ TEXT("1HM_Slash03"), TEXT("1HM_Slash03_Blocked") }, { TEXT("1HM_Slash04"), TEXT("1HM_Slash04_Blocked") } };
		const FSwing Stabs[] = { { TEXT("1HM_Stab01"), nullptr }, { TEXT("1HM_Stab02"), nullptr }, { TEXT("1HM_Stab03"), nullptr } };
		MakeCombo(Slashes, false, Sword.LightAttack);
		MakeCombo(Stabs, true, Sword.HeavyAttack);
		Sword.bHasHeavyAttack = true;
		Sword.Montages.Attack = Sword.LightAttack.Montage;

		// The guard: the block raised to where it settles (it holds there in the stance while the block is held), and
		// the four blocks from the snap into them as the impacts of hits taken on it.
		UAnimSequence* GuardAnim = SwordAnim(TEXT("1HM_BlockRight"));
		float GuardTime = 0.4f;
		if (GuardAnim)
		{
			const FSwingTiming Raise = MeasureSwing(GuardAnim);
			GuardTime = FindSettle(GuardAnim, Raise.PeakTime, Raise.PeakTime + 0.5f);
			Sword.Montages.Block = MakeTrimmedMontage(TEXT("AM_1HM_Guard"), GuardAnim, UpperBodySlot, 0.0f, GuardTime, 0.1f, 0.12f);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Guard: 1HM_BlockRight held at %.2f s"), GuardTime);
		}
		Sword.Montages.BlockImpacts.Reset();
		for (const TCHAR* Block : { TEXT("1HM_BlockLeft"), TEXT("1HM_BlockRight"), TEXT("1HM_BlockLeftLow"), TEXT("1HM_BlockRightLow") })
		{
			if (UAnimSequence* Anim = SwordAnim(Block))
			{
				const FSwingTiming Timing = MeasureSwing(Anim);
				const float Settle = FindSettle(Anim, Timing.PeakTime, Timing.PeakTime + 0.5f);
				const FString Name = FString(TEXT("AM_")) + Block + TEXT("_Impact");
				if (UAnimMontage* Impact = MakeTrimmedMontage(*Name, Anim, UpperBodySlot, FMath::Max(Timing.PeakTime - 0.1f, 0.0f), FMath::Min(Settle + 0.3f, Timing.Length), 0.06f, 0.2f))
				{
					Sword.Montages.BlockImpacts.Add(Impact);
				}
			}
		}

		// Parried: the swing knocked aside and the swordsman reeling. Hit: a flinch (full body, in place).
		Sword.Montages.Stagger = MakeMontage(Folder / TEXT("AM_1HM_Stagger"), SwordAnim(TEXT("1HM_HitReact02")), FAnimSlotGroup::DefaultSlotName, 0.08f, 0.25f);
		Sword.Montages.HitReactions.Reset();
		for (const TCHAR* Hit : { TEXT("1HM_HitReact01"), TEXT("1HM_HitReact02") })
		{
			if (UAnimMontage* Reaction = MakeMontage(Folder / (FString(TEXT("AM_")) + Hit), SwordAnim(Hit), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.25f))
			{
				Sword.Montages.HitReactions.Add(Reaction);
			}
		}
		Sword.Montages.HitReactionBehind = MakeMontage(Folder / TEXT("AM_1HM_HitReact_Behind01"), SwordAnim(TEXT("1HM_HitReact_Behind01")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.25f);

		// Dodges (root motion).
		Sword.Montages.DodgeForward = MakeMontage(Folder / TEXT("AM_1HM_Evade_Forward"), SwordAnim(TEXT("1HM_Evade_Forward")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.2f);
		Sword.Montages.DodgeBack = MakeMontage(Folder / TEXT("AM_1HM_Evade_Back"), SwordAnim(TEXT("1HM_Evade_Back")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.2f);
		Sword.Montages.DodgeLeft = MakeMontage(Folder / TEXT("AM_1HM_Evade_Left"), SwordAnim(TEXT("1HM_Evade_Left")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.2f);
		Sword.Montages.DodgeRight = MakeMontage(Folder / TEXT("AM_1HM_Evade_Right"), SwordAnim(TEXT("1HM_Evade_Right")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.2f);

		for (UAnimMontage* Montage : TArray<UAnimMontage*>{ Sword.Montages.Stagger, Sword.Montages.HitReactionBehind, Sword.Montages.DodgeForward,
			Sword.Montages.DodgeBack, Sword.Montages.DodgeLeft, Sword.Montages.DodgeRight })
		{
			if (Montage)
			{
				Save(Montage);
			}
		}
		for (UAnimMontage* Montage : Sword.Montages.HitReactions)
		{
			Save(Montage);
		}

		// Drawing from the left hip and sheathing there: the right hand crosses to the hilt (HandIK_Sheath up),
		// takes it (AN_WeaponGrab) or lets go (AN_WeaponRelease) where it comes nearest the left hip.
		// A little quicker than the pack plays them: a draw shouldn't keep a fighter waiting.
		const float HipDrawRate = 1.4f;
		const FString DrawPath = Folder / TEXT("AM_1HM_DrawFromHip");
		const bool bNewDraw = !Exists(DrawPath);
		UAnimSequence* EquipAnim = SwordAnim(TEXT("1HM_Idle_Equip"));
		const float Grab = EquipAnim ? FindNearest(EquipAnim, TEXT("hand_r"), TEXT("thigh_l"), 0.0f, EquipAnim->GetPlayLength() * 0.7f) : 0.5f;
		UAnimSequence* DrawAnim = CopyWithSheathCurve(Folder / TEXT("AS_1HM_DrawFromHip"), EquipAnim,
			{ { 0.0f, 0.0f }, { FMath::Max(Grab - 0.25f, 0.0f), 0.0f }, { Grab - 0.04f, 1.0f }, { Grab + 0.04f, 1.0f }, { Grab + 0.3f, 0.0f } });
		UAnimMontage* Draw = MakeMontage(DrawPath, DrawAnim, UpperBodySlot, 0.15f, 0.25f, HipDrawRate);
		if (Draw && bNewDraw)
		{
			AddNotify(Draw, NewObject<UAnimNotify_VaesenWeaponGrab>(Draw), Grab);
			Save(Draw);
		}
		const FString SheathPath = Folder / TEXT("AM_1HM_SheathToHip");
		const bool bNewSheath = !Exists(SheathPath);
		UAnimSequence* UnequipAnim = SwordAnim(TEXT("1HM_Idle_Unequip"));
		const float Release = UnequipAnim ? FindNearest(UnequipAnim, TEXT("hand_r"), TEXT("thigh_l"), UnequipAnim->GetPlayLength() * 0.2f, UnequipAnim->GetPlayLength()) : 1.0f;
		UAnimSequence* SheathAnim = CopyWithSheathCurve(Folder / TEXT("AS_1HM_SheathToHip"), UnequipAnim,
			{ { 0.0f, 0.0f }, { FMath::Max(Release - 0.3f, 0.0f), 0.0f }, { Release - 0.04f, 1.0f }, { Release + 0.04f, 1.0f }, { Release + 0.25f, 0.0f } });
		UAnimMontage* Sheath = MakeMontage(SheathPath, SheathAnim, UpperBodySlot, 0.15f, 0.25f, HipDrawRate);
		if (Sheath && bNewSheath)
		{
			AddNotify(Sheath, NewObject<UAnimNotify_VaesenWeaponRelease>(Sheath), Release);
			Save(Sheath);
		}
		// Their sounds, timed here rather than at the hand's grab or release: the blade starts to slide out as the hand
		// closes on the hilt, and slides in a good while before the hand lets go at the end of the sheathing.
		const float SheathSoundAt = FMath::Max(Release - 0.45f, 0.0f);
		for (UAnimMontage* Montage : { Draw, Sheath })
		{
			if (!Montage)
			{
				continue;
			}
			bool bChanged = Montage->RateScale != HipDrawRate;
			Montage->RateScale = HipDrawRate;
			bChanged |= Montage == Draw ? SetSoundNotify(Montage, EVaesenWeaponSound::Draw, FMath::Max(Grab - SoundLead, 0.0f))
				: SetSoundNotify(Montage, EVaesenWeaponSound::Sheath, SheathSoundAt);
			if (bChanged)
			{
				Save(Montage);
			}
		}
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Hip draw: grabs at %.2f s; sheath lets go at %.2f s"), Grab, Release);
		if (FVaesenSheathLocation* Hip = Defaults->SheathLocations.Find(TEXT("Sheath_HipLeft")))
		{
			Hip->DrawMontage = Draw;
			Hip->SheathMontage = Sheath;
			Hip->ReferenceGrabPoint = FVector::ZeroVector;
			// Hung from the hilt, the blade down and back (between 45 degrees and straight down), swinging as he moves.
			Hip->HangAngle = 60.0f;

			// Resting against the left leg: how far the thigh slopes outward standing (the plain idle, which the sheath
			// was placed against), and in the run for the log (the legs come in under the body).
			const auto ThighOutward = [](const TCHAR* Path)
			{
				const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, Path);
				float Sum = 0.0f;
				int32 Count = 0;
				for (float Time = 0.0f; Anim && Time <= Anim->GetPlayLength(); Time += 1.0f / 30.0f, ++Count)
				{
					const FAnimPose Pose = PoseAt(Anim, Time);
					const FVector Thigh = (UAnimPoseExtensions::GetBonePose(Pose, TEXT("calf_l"), EAnimPoseSpaces::World).GetLocation()
						- UAnimPoseExtensions::GetBonePose(Pose, TEXT("thigh_l"), EAnimPoseSpaces::World).GetLocation()).GetSafeNormal();
					// Manny's left is +X.
					Sum += Thigh.X;
				}
				return Count > 0 ? Sum / Count : 0.0f;
			};
			Hip->LegBone = TEXT("thigh_l");
			Hip->KneeBone = TEXT("calf_l");
			Hip->LegRestOutward = ThighOutward(TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Left thigh slopes outward %.3f standing, %.3f running (average)"), Hip->LegRestOutward,
				ThighOutward(TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Run_Fwd.MM_Run_Fwd")));
		}

		// The stance, the fingers round the hilt, and the sword angled in the hand as the pack holds it.
		Sword.AnimLayer = BuildSwordLocomotion(Skeleton, bRebuild, MakeSwordBlendSpace(bRebuild), GuardAnim, GuardTime);
		Sword.GripPose = SwordAnim(TEXT("1HM_Idle_Standing"));
		FTransform InHand;
		if (ComputeSwordInHand(Skeleton, InHand))
		{
			Sword.InHand = InHand;
		}

		// The other one-handed weapons parry as the sword does until they have animations of their own: its stance
		// (which holds the guard, and squares up when locked on), its guard and the recoils of hits taken on it.
		{
			const TSubclassOf<UAnimInstance> Layer = Sword.AnimLayer;
			const TObjectPtr<UAnimMontage> Guard = Sword.Montages.Block;
			const TArray<TObjectPtr<UAnimMontage>> Recoils = Sword.Montages.BlockImpacts;
			for (const EVaesenWeaponCategory Borrower : { EVaesenWeaponCategory::Dagger, EVaesenWeaponCategory::OneHandedAxe, EVaesenWeaponCategory::Club,
				EVaesenWeaponCategory::Improvised })
			{
				// Found, not added: adding could move the map's entries under Sword, used below.
				FVaesenCategoryDefaults* Entry = Defaults->Categories.Find(Borrower);
				if (Entry && (!Entry->Montages.Block || Entry->Montages.Block->GetPathName().StartsWith(SwordFolder)))
				{
					Entry->AnimLayer = Layer;
					Entry->Montages.Block = Guard;
					Entry->Montages.BlockImpacts = Recoils;
				}
			}
		}

		// The left-hip sheath: the hilt at the left hip by the standing body's own layout (beside the left hip joint, a
		// little out and forward), on the pelvis so it rides the hips; turned as the draw holds the sword when it takes
		// it (so the hang lifts it to meet the hand), measured in the body's frame (the draw twists the hips: measured on
		// the pelvis, the hilt would end up between the legs). The draw's hand IK reaches it. Only over the grey-box
		// guess or our own earlier one (-resetsockets or -resetsheath work it out again; a socket tuned by hand stays).
		const USkeletalMeshSocket* Hip = Skeleton->FindSocket(TEXT("Sheath_HipLeft"));
		const bool bGreyBoxHip = !Hip || (Hip->BoneName == TEXT("pelvis") && Hip->RelativeLocation.Equals(FVector(0.0, 0.0, 18.0), 0.1));
		const USkeletalMeshSocket* Palm = Skeleton->FindSocket(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket);
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const int32 PelvisIndex = Reference.FindBoneIndex(TEXT("pelvis"));
		const int32 ThighIndex = Reference.FindBoneIndex(TEXT("thigh_l"));
		const bool bReset = FParse::Param(FCommandLine::Get(), TEXT("resetsockets")) || FParse::Param(FCommandLine::Get(), TEXT("resetsheath"));
		if (EquipAnim && Palm && PelvisIndex != INDEX_NONE && ThighIndex != INDEX_NONE && (bGreyBoxHip || bReset))
		{
			const FAnimPose AtGrab = PoseAt(EquipAnim, Grab);
			const FTransform Hand = UAnimPoseExtensions::GetBonePose(AtGrab, TEXT("hand_r"), EAnimPoseSpaces::World);
			const FTransform Root = UAnimPoseExtensions::GetBonePose(AtGrab, TEXT("root"), EAnimPoseSpaces::World);
			const FQuat HeldAtGrab = (Sword.InHand * Palm->GetSocketLocalTransform() * Hand).GetRelativeTransform(Root).GetRotation();
			// Manny faces +Y; his left is +X.
			const FVector HipJoint = FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, ThighIndex).GetLocation();
			const FTransform Hilt(HeldAtGrab, HipJoint + FVector(10.75, 4.0, 8.0));
			FTransform Sheathed = Hilt.GetRelativeTransform(FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, PelvisIndex));
			Sheathed.SetScale3D(FVector::OneVector);
			SetSkeletonSocket(Skeleton, TEXT("Sheath_HipLeft"), TEXT("pelvis"), Sheathed, true);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Left-hip sheath: hilt at %s (hip joint %s), blade points %s (mesh space; Manny faces +Y, his left is +X)"),
				*Hilt.GetLocation().ToCompactString(), *HipJoint.ToCompactString(), *Hilt.GetUnitAxis(EAxis::X).ToCompactString());
			Save(Skeleton);
		}
		Save(Defaults);
	}

	// --- Pistols (MandB Animation "Pistol Locomotion Pack") ---

	const TCHAR* PistolFolder = TEXT("/Game/Vaesen/Animations/Pistol");
	const TCHAR* PistolGraphPath = TEXT("/Game/Vaesen/Animations/Pistol/ABP_PistolLocomotion");
	const TCHAR* PistolPackSkeleton = TEXT("/Game/PistolLocomotionPack/Demo/Characters/Mannequins/Meshes/SK_Mannequin");

	/**
	 * The pack's animation (Animations/<Group>/<Name>) as ours, in /Game/Vaesen/Animations/Pistol: a copy put on
	 * Manny's skeleton (the pack's skeleton is a copy of it, the same bones), in place (the capsule moves). Made once.
	 */
	UAnimSequence* PistolAnim(const TCHAR* Group, const TCHAR* Name)
	{
		const FString Path = FString(PistolFolder) / Name;
		if (UAnimSequence* Existing = LoadAt<UAnimSequence>(Path))
		{
			return Existing;
		}
		UAnimSequence* Source = LoadAt<UAnimSequence>(FString(PistolPackFolder) / TEXT("Animations") / Group / Name);
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		if (!Source || !Skeleton)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Pistol pack animation %s/%s is missing."), Group, Name);
			return nullptr;
		}
		UPackage* Package = CreatePackage(*Path);
		UAnimSequence* Copy = DuplicateObject<UAnimSequence>(Source, Package, FName(Name));
		Copy->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Copy);
		Copy->ReplaceSkeleton(Skeleton);
		Copy->bEnableRootMotion = false;
		Copy->bForceRootLock = true;
		Copy->PostEditChange();
		Save(Copy);
		return Copy;
	}

	bool ComputePistolInHand(const USkeleton* Skeleton, FTransform& OutDesired, FTransform& OutInHand);

	/** The right arm in the pack's stance: the pack's own animation, our copy, and Manny's idle, side by side. */
	void AnalyzePistolArm()
	{
		const USkeleton* Pack = LoadAt<USkeleton>(PistolPackSkeleton);
		const USkeleton* Manny = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		const TCHAR* Bones[] = { TEXT("spine_05"), TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("clavicle_l"), TEXT("upperarm_l") };
		for (const TCHAR* Bone : Bones)
		{
			const int32 A = Pack ? Pack->GetReferenceSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
			const int32 B = Manny ? Manny->GetReferenceSkeleton().FindBoneIndex(Bone) : INDEX_NONE;
			if (A != INDEX_NONE && B != INDEX_NONE)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("ref %-11s pack %s | manny %s"), Bone, *Describe(Pack->GetReferenceSkeleton().GetRefBonePose()[A]),
					*Describe(Manny->GetReferenceSkeleton().GetRefBonePose()[B]));
			}
		}
		const auto Dump = [&Bones](const TCHAR* Label, const TCHAR* Path, float Time)
		{
			const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, Path);
			if (!Anim)
			{
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s: missing %s"), Label, Path);
				return;
			}
			const FAnimPose Pose = PoseAt(Anim, Time);
			FString Line;
			for (const TCHAR* Bone : Bones)
			{
				const FTransform World = UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::World);
				const FTransform Local = UAnimPoseExtensions::GetBonePose(Pose, Bone, EAnimPoseSpaces::Local);
				Line += FString::Printf(TEXT("\n    %-11s world %s local rot %s loc %s"), Bone, *World.GetLocation().ToCompactString(), *Local.Rotator().ToCompactString(), *Local.GetLocation().ToCompactString());
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s (%s, skeleton %s):%s"), Label, Path, *GetNameSafe(Anim->GetSkeleton()), *Line);
		};
		Dump(TEXT("PACK stand idle"), TEXT("/Game/PistolLocomotionPack/Animations/StandIdle/anim_pistol_stand_idle.anim_pistol_stand_idle"), 0.0f);
		Dump(TEXT("OURS stand idle"), TEXT("/Game/Vaesen/Animations/Pistol/anim_pistol_stand_idle.anim_pistol_stand_idle"), 0.0f);
		Dump(TEXT("PACK jog fwd"), TEXT("/Game/PistolLocomotionPack/Animations/Jog/anim_pistol_jog_fwd_loop.anim_pistol_jog_fwd_loop"), 0.0f);
		Dump(TEXT("OURS jog fwd"), TEXT("/Game/Vaesen/Animations/Pistol/anim_pistol_jog_fwd_loop.anim_pistol_jog_fwd_loop"), 0.0f);
		Dump(TEXT("MM_Idle"), TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"), 0.0f);

		// How the relaxed stance holds the gun, and where the right leg is.
		FTransform GunInHand, InHand;
		if (Manny && ComputePistolInHand(Manny, GunInHand, InHand))
		{
			for (const TCHAR* Path : { TEXT("/Game/Vaesen/Animations/Pistol/anim_pistol_stand_idle.anim_pistol_stand_idle"), TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle") })
			{
				const UAnimSequence* Anim = LoadObject<UAnimSequence>(nullptr, Path);
				if (!Anim)
				{
					continue;
				}
				const FAnimPose Pose = PoseAt(Anim, 0.0);
				const FTransform Gun = GunInHand * UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World);
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s (%.2f s): gun at %s barrel %s top %s; thigh_r %s calf_r %s pelvis %s"), *GetNameSafe(Anim), Anim->GetPlayLength(),
					*Gun.GetLocation().ToCompactString(), *Gun.GetUnitAxis(EAxis::X).ToCompactString(), *Gun.GetUnitAxis(EAxis::Z).ToCompactString(),
					*UAnimPoseExtensions::GetBonePose(Pose, TEXT("thigh_r"), EAnimPoseSpaces::World).GetLocation().ToCompactString(),
					*UAnimPoseExtensions::GetBonePose(Pose, TEXT("calf_r"), EAnimPoseSpaces::World).GetLocation().ToCompactString(),
					*UAnimPoseExtensions::GetBonePose(Pose, TEXT("pelvis"), EAnimPoseSpaces::World).GetLocation().ToCompactString());
			}
		}
	}

	/** How fast the pack's walk with root motion (Loop_root/<Name>) goes, in cm/s. */
	float PistolGaitSpeed(const FString& RootName)
	{
		const UAnimSequence* Root = LoadAt<UAnimSequence>(FString(PistolPackFolder) / TEXT("Animations/Loop_root") / RootName);
		if (!Root)
		{
			return 0.0f;
		}
		const FSwingTiming Timing = MeasureSwing(Root);
		return FMath::RoundToFloat(Timing.RootMove.Size2D() / FMath::Max(Timing.Length, 0.01f));
	}

	struct FPistolGait { const TCHAR* Group; const TCHAR* Loop; float Direction; };

	/**
	 * A direction/speed blend space set like the rifle's (BS_Rifle duplicated): Idle standing, and each loop at its own
	 * speed (measured in the pack's Loop_root copy, "<Loop>_root"). Made once; -rebuild remakes it while the pack's there.
	 */
	UBlendSpace* MakePistolBlendSpace(const TCHAR* Name, UAnimSequence* Idle, TArrayView<const FPistolGait> Gaits, bool bRebuild)
	{
		const FString Path = FString(PistolFolder) / Name;
		UBlendSpace* Space = LoadAt<UBlendSpace>(Path);
		const bool bPack = FPackageName::DoesPackageExist(FString(PistolPackFolder) / TEXT("Animations/Loop_root/anim_pistol_walk_fwd_loop_root"));
		if (Space && (!bRebuild || !bPack))
		{
			return Space;
		}
		if (!Space)
		{
			UBlendSpace* Rifle = RifleAsset<UBlendSpace>(TEXT("BS_Rifle"));
			if (!Rifle)
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*Path);
			Space = DuplicateObject<UBlendSpace>(Rifle, Package, FName(Name));
			Space->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Space);
		}
		while (Space->GetNumberOfBlendSamples() > 0)
		{
			Space->DeleteSample(0);
		}
		const auto Add = [Space](UAnimSequence* Anim, float Direction, float Speed)
		{
			if (Anim)
			{
				const FVector Value(Direction, Speed, 0.0);
				Space->ExpandRangeForSample(Value);
				Space->AddSample(Anim, Value);
			}
		};
		for (const float Direction : { -180.0f, -90.0f, 0.0f, 90.0f, 180.0f })
		{
			Add(Idle, Direction, 0.0f);
		}
		for (const FPistolGait& Gait : Gaits)
		{
			UAnimSequence* Loop = PistolAnim(Gait.Group, Gait.Loop);
			const float Speed = PistolGaitSpeed(FString(Gait.Loop) + TEXT("_root"));
			if (Speed <= 0.0f)
			{
				UE_LOG(LogVaesenAnimSetup, Warning, TEXT("No speed for %s"), Gait.Loop);
				continue;
			}
			Add(Loop, Gait.Direction, Speed);
			// Straight back also at -180, so the space wraps round.
			if (Gait.Direction == 180.0f)
			{
				Add(Loop, -180.0f, Speed);
			}
		}
		Space->ValidateSampleData();
		Space->ResampleData();
		Space->PostEditChange();
		Save(Space);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("%s: %d samples, speed up to %.0f"), Name, Space->GetNumberOfBlendSamples(), Space->GetBlendParameter(1).Max);
		return Space;
	}

	/**
	 * How the pack's pistol sits in the hand: its demo skeleton holds it at hand_rSocket (the pistol's barrel along its
	 * Y, up its Z). Ours point X along the barrel, Z up, held by their Grip where the palm closes. Manny's hand is the
	 * same bone, so this is the category's InHand directly (on top of the hand socket).
	 */
	bool ComputePistolInHand(const USkeleton* Skeleton, FTransform& OutDesired, FTransform& OutInHand)
	{
		const USkeleton* Pack = LoadAt<USkeleton>(PistolPackSkeleton);
		const USkeletalMeshSocket* Socket = Pack ? Pack->FindSocket(TEXT("hand_rSocket")) : nullptr;
		const USkeletalMeshSocket* Palm = Skeleton->FindSocket(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket);
		const FReferenceSkeleton& Reference = Skeleton->GetReferenceSkeleton();
		const int32 HandIndex = Reference.FindBoneIndex(TEXT("hand_r"));
		const int32 KnuckleIndex = Reference.FindBoneIndex(TEXT("middle_01_r"));
		if (!Socket || !Palm || HandIndex == INDEX_NONE || KnuckleIndex == INDEX_NONE)
		{
			return false;
		}
		const FTransform Gun = Socket->GetSocketLocalTransform();
		const FTransform Hand = FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, HandIndex);
		const FVector PalmInHand = Hand.InverseTransformPosition(FAnimationRuntime::GetComponentSpaceTransformRefPose(Reference, KnuckleIndex).GetLocation()) * 0.5;
		OutDesired = FTransform(FRotationMatrix::MakeFromXZ(Gun.GetUnitAxis(EAxis::Y), Gun.GetUnitAxis(EAxis::Z)).ToQuat(), PalmInHand);
		OutInHand = OutDesired * Palm->GetSocketLocalTransform().Inverse();
		OutInHand.SetScale3D(FVector::OneVector);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pistol in the hand %s (category InHand %s)"), *Describe(OutDesired), *Describe(OutInHand));
		return true;
	}

	/** The pistol's aim offset (pitch): the rifle's AO1D duplicated, with the pack's front poses at the pitch the gun points in each. */
	UBlendSpace* MakePistolAimOffset(const FTransform& GunInHand, bool bRebuild)
	{
		const FString Path = FString(PistolFolder) / TEXT("AO1D_Pistol");
		UBlendSpace* Offset = LoadAt<UBlendSpace>(Path);
		if (Offset && !bRebuild)
		{
			return Offset;
		}
		UAnimSequence* Center = PistolAnim(TEXT("AnimOffset"), TEXT("anim_pistol_front_center"));
		UAnimSequence* Up = PistolAnim(TEXT("AnimOffset"), TEXT("anim_pistol_front_up"));
		UAnimSequence* Down = PistolAnim(TEXT("AnimOffset"), TEXT("anim_pistol_front_down"));
		if (!Center || !Up || !Down)
		{
			return Offset;
		}
		// Where the gun points in each pose (in the pack's own poses: ours become additive below).
		const auto PitchOf = [&GunInHand](const TCHAR* Name)
		{
			const UAnimSequence* Pose = LoadAt<UAnimSequence>(FString(PistolPackFolder) / TEXT("Animations/AnimOffset") / Name);
			if (!Pose)
			{
				return 0.0f;
			}
			const FTransform Hand = UAnimPoseExtensions::GetBonePose(PoseAt(Pose, 0.0), TEXT("hand_r"), EAnimPoseSpaces::World);
			return static_cast<float>((GunInHand * Hand).Rotator().Pitch);
		};
		const float CenterPitch = PitchOf(TEXT("anim_pistol_front_center"));
		const float UpPitch = PitchOf(TEXT("anim_pistol_front_up")) - CenterPitch;
		const float DownPitch = PitchOf(TEXT("anim_pistol_front_down")) - CenterPitch;
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pistol aim poses: up %.0f, down %.0f degrees (from the centre's %.0f)"), UpPitch, DownPitch, CenterPitch);

		// Rotation offsets in mesh space, from the centre pose.
		for (UAnimSequence* Pose : { Center, Up, Down })
		{
			if (Pose->AdditiveAnimType != AAT_RotationOffsetMeshSpace)
			{
				Pose->AdditiveAnimType = AAT_RotationOffsetMeshSpace;
				Pose->RefPoseType = ABPT_AnimFrame;
				Pose->RefPoseSeq = Center;
				Pose->RefFrameIndex = 0;
				Pose->PostEditChange();
				Save(Pose);
			}
		}
		if (!Offset)
		{
			UBlendSpace* Rifle = RifleAsset<UBlendSpace>(TEXT("AO1D_Rifle_Aim"));
			if (!Rifle)
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*Path);
			Offset = DuplicateObject<UBlendSpace>(Rifle, Package, FName(TEXT("AO1D_Pistol")));
			Offset->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Offset);
		}
		if (UpPitch > 1.0f && DownPitch < -1.0f)
		{
			while (Offset->GetNumberOfBlendSamples() > 0)
			{
				Offset->DeleteSample(0);
			}
			const TPair<UAnimSequence*, float> Samples[] = { { Down, DownPitch }, { Center, 0.0f }, { Up, UpPitch } };
			for (const TPair<UAnimSequence*, float>& Sample : Samples)
			{
				Offset->ExpandRangeForSample(FVector(Sample.Value, 0.0, 0.0));
				Offset->AddSample(Sample.Key, FVector(Sample.Value, 0.0, 0.0));
			}
			Offset->ValidateSampleData();
			Offset->ResampleData();
			Offset->PostEditChange();
			Save(Offset);
		}
		return Offset;
	}

	/** A montage of part of a pistol animation (made once). */
	UAnimMontage* MakeTrimmedPistolMontage(const TCHAR* Name, UAnimSequence* Sequence, float Start, float End)
	{
		const FString Path = FString(PistolFolder) / Name;
		const bool bNew = !Exists(Path);
		UAnimMontage* Montage = MakeMontage(Path, Sequence, UpperBodySlot, 0.1f, 0.15f);
		if (Montage && bNew)
		{
			TrimMontage(Montage, Start, End);
			Save(Montage);
		}
		return Montage;
	}

	/**
	 * ABP_PistolLocomotion: the pistol's stance, linked into the character's graph while a pistol or revolver is drawn.
	 * Relaxed or aimed (each a direction/speed blend space), crouched, and the fall loop in the air; the aim offset
	 * (AimPitch) on each.
	 */
	UClass* BuildPistolLocomotion(USkeleton* Skeleton, bool bRebuild, UBlendSpace* Relaxed, UBlendSpace* Aimed, UBlendSpace* Crouched, UBlendSpace* AimOffset, UAnimSequence* Fall)
	{
		if (!bRebuild)
		{
			if (UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(PistolGraphPath))
			{
				return Existing->GeneratedClass;
			}
		}
		UAnimBlueprint* Blueprint = OpenAnimBlueprint(PistolGraphPath, Skeleton);
		FGraphBuilder Builder(Blueprint);
		if (!Blueprint || !Builder.Graph)
		{
			return nullptr;
		}
		UEdGraphNode* Hip = LocomotionWithAim(Builder, -1600, 0, Relaxed, AimOffset);
		UEdGraphNode* Aim = LocomotionWithAim(Builder, -1600, 250, Aimed, AimOffset);
		UEdGraphNode* Crouch = LocomotionWithAim(Builder, -1600, 500, Crouched, AimOffset);

		// Not squared up (not aiming): the plain locomotion (ABP_Manny), standing, running and jumping, arms and all;
		// the hand hanging at his side holds the gun muzzle down. The pack's relaxed set leaves the right collarbone at
		// the bind pose, the shoulder rolled forward into the body.
		UClass* Plain = LoadObject<UClass>(nullptr, UnarmedGraphPath);
		UAnimGraphNode_LinkedAnimGraph* PlainGraph = Builder.Add<UAnimGraphNode_LinkedAnimGraph>(-1600, -300, [Plain](UAnimGraphNode_LinkedAnimGraph* Node)
		{
			Node->Node.InstanceClass = Plain;
		});
		UAnimGraphNode_SaveCachedPose* PlainCache = Builder.Add<UAnimGraphNode_SaveCachedPose>(-1300, -300, [](UAnimGraphNode_SaveCachedPose* Node)
		{
			Node->CacheName = TEXT("PistolPlain");
		});
		Builder.Pose(PlainGraph, PlainCache, TEXT("Pose"));
		const auto UsePlain = [&Builder, PlainCache](int32 Y)
		{
			return Builder.Add<UAnimGraphNode_UseCachedPose>(-1150, Y, [PlainCache](UAnimGraphNode_UseCachedPose* Node)
			{
				Node->SaveCachedPoseNode = PlainCache;
			});
		};

		UEdGraphNode* Lowered = ByBool(Builder, -1100, 0, TEXT("bRelaxed"), UsePlain(-150), Hip, 0.3f);
		UEdGraphNode* Standing = ByBool(Builder, -900, 100, TEXT("bAiming"), Aim, Lowered, 0.2f);
		UEdGraphNode* Grounded = ByBool(Builder, -600, 300, TEXT("bCrouching"), Crouch, Standing, 0.25f);
		UAnimGraphNode_SequencePlayer* Air = Builder.Add<UAnimGraphNode_SequencePlayer>(-900, 650, [Fall](UAnimGraphNode_SequencePlayer* Node)
		{
			Node->Node.SetSequence(Fall);
		});
		UEdGraphNode* Falling = ByBool(Builder, -600, 650, TEXT("bRelaxed"), UsePlain(800), Air, 0.2f);
		UEdGraphNode* Final = ByBool(Builder, -300, 300, TEXT("bFalling"), Falling, Grounded, 0.15f);
		Builder.Pose(Final, Builder.Root(), TEXT("Result"));
		CompileAndSave(Blueprint);
		return Blueprint->GeneratedClass;
	}

	void BuildPistol()
	{
		const bool bRebuild = FParse::Param(FCommandLine::Get(), TEXT("rebuild"));
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		UVaesenWeaponCategoryDefaults* Defaults = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"));
		const bool bPack = FPackageName::DoesPackageExist(FString(PistolPackFolder) / TEXT("Animations/StandIdle/anim_pistol_stand_idle"));
		if (!Skeleton || !Defaults || (!bPack && !Exists(PistolGraphPath)))
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pistol pack (/Game/PistolLocomotionPack) not in the project; skipping the pistols."));
			return;
		}
		const FString Folder = PistolFolder;

		// How the pack holds its pistol (the grip in the hand); the aim offset's poses are measured with it.
		FTransform GunInHand, InHand;
		const bool bHold = bPack && ComputePistolInHand(Skeleton, GunInHand, InHand);

		// The stance: relaxed (walk, jog), aimed (the aimed walk), crouched; the fall loop.
		UAnimSequence* StandIdle = PistolAnim(TEXT("StandIdle"), TEXT("anim_pistol_stand_idle"));
		UAnimSequence* AimIdle = PistolAnim(TEXT("Aim"), TEXT("anim_pistol_aim_idle"));
		UAnimSequence* CrouchIdle = PistolAnim(TEXT("Crouch"), TEXT("anim_pistol_crouch_idle"));
		const FPistolGait Relaxed[] = {
			{ TEXT("Walk"), TEXT("anim_pistol_walk_fwd_loop"), 0.0f }, { TEXT("Walk"), TEXT("anim_pistol_walk_45_fwd_right_loop"), 45.0f },
			{ TEXT("Walk"), TEXT("anim_pistol_walk_right_loop"), 90.0f }, { TEXT("Walk"), TEXT("anim_pistol_walk_45_bwd_right_loop"), 135.0f },
			{ TEXT("Walk"), TEXT("anim_pistol_walk_bwd_loop"), 180.0f }, { TEXT("Walk"), TEXT("anim_pistol_walk_45_bwd_left_loop"), -135.0f },
			{ TEXT("Walk"), TEXT("anim_pistol_walk_left_loop"), -90.0f }, { TEXT("Walk"), TEXT("anim_pistol_walk_45_fwd_left_loop"), -45.0f },
			{ TEXT("Jog"), TEXT("anim_pistol_jog_fwd_loop"), 0.0f }, { TEXT("Jog"), TEXT("anim_pistol_jog_45_fwd_right_loop"), 45.0f },
			{ TEXT("Jog"), TEXT("anim_pistol_jog_right_loop"), 90.0f }, { TEXT("Jog"), TEXT("anim_pistol_jog_45_bwd_right_loop"), 135.0f },
			{ TEXT("Jog"), TEXT("anim_pistol_jog_bwd_loop"), 180.0f }, { TEXT("Jog"), TEXT("anim_pistol_jog_45_bwd_left_loop"), -135.0f },
			{ TEXT("Jog"), TEXT("anim_pistol_jog_left_loop"), -90.0f }, { TEXT("Jog"), TEXT("anim_pistol_jog_45_fwd_left_loop"), -45.0f } };
		const FPistolGait Aimed[] = {
			{ TEXT("Aim"), TEXT("anim_pistol_aim_walk_fwd_loop"), 0.0f }, { TEXT("Aim"), TEXT("anim_pistol_aim_walk_45_fwd_right_loop"), 45.0f },
			{ TEXT("Aim"), TEXT("anim_pistol_aim_walk_right_loop"), 90.0f }, { TEXT("Aim"), TEXT("anim_pistol_aim_walk_45_bwd_right_loop"), 135.0f },
			{ TEXT("Aim"), TEXT("anim_pistol_aim_walk_bwd_loop"), 180.0f }, { TEXT("Aim"), TEXT("anim_pistol_aim_walk_45_bwd_left_loop"), -135.0f },
			{ TEXT("Aim"), TEXT("anim_pistol_aim_walk_left_loop"), -90.0f }, { TEXT("Aim"), TEXT("anim_pistol_aim_walk_45_fwd_left_loop"), -45.0f } };
		const FPistolGait Crouched[] = {
			{ TEXT("Crouch"), TEXT("anim_pistol_crouch_fwd_loop"), 0.0f }, { TEXT("Crouch"), TEXT("anim_pistol_crouch_walk_right_loop"), 90.0f },
			{ TEXT("Crouch"), TEXT("anim_pistol_crouch_bwd_loop"), 180.0f }, { TEXT("Crouch"), TEXT("anim_pistol_crouch_walk_left_loop"), -90.0f } };
		UBlendSpace* RelaxedSpace = MakePistolBlendSpace(TEXT("BS_Pistol"), StandIdle, Relaxed, bRebuild);
		UBlendSpace* AimedSpace = MakePistolBlendSpace(TEXT("BS_Pistol_Aim"), AimIdle, Aimed, bRebuild);
		UBlendSpace* CrouchedSpace = MakePistolBlendSpace(TEXT("BS_Pistol_Crouch"), CrouchIdle, Crouched, bRebuild);
		UBlendSpace* AimOffset = bHold ? MakePistolAimOffset(GunInHand, bRebuild) : LoadAt<UBlendSpace>(Folder / TEXT("AO1D_Pistol"));
		UAnimSequence* Fall = PistolAnim(TEXT("Jump"), TEXT("anim_pistol_jump_fall_loop"));
		UClass* Graph = BuildPistolLocomotion(Skeleton, bRebuild, RelaxedSpace, AimedSpace, CrouchedSpace, AimOffset, Fall);

		// Firing (the upper body; the procedural kick is on top).
		UAnimMontage* Fire = MakeMontage(Folder / TEXT("AM_Pistol_Fire"), PistolAnim(TEXT("Aim"), TEXT("anim_pistol_aim_fire")), UpperBodySlot, 0.05f, 0.2f);
		if (Fire)
		{
			Save(Fire);
		}

		// Reloading. The pack's one reload, with its sounds where the hands are: the gun opened as it starts, the round
		// (or rounds) in as the left hand meets the gun, closed as it ends. A revolver loads round by round: the moment
		// the left hand meets the gun, again for each round.
		UAnimSequence* ReloadAnim = PistolAnim(TEXT("Reload"), TEXT("anim_pistol_aim_reload"));
		UAnimMontage* Reload = nullptr;
		UAnimMontage* ReloadRound = nullptr;
		if (ReloadAnim)
		{
			const float Length = ReloadAnim->GetPlayLength();
			const float Insert = FindNearest(ReloadAnim, TEXT("hand_l"), TEXT("hand_r"), Length * 0.2f, Length * 0.9f);
			Reload = MakeMontage(Folder / TEXT("AM_Pistol_Reload"), ReloadAnim, UpperBodySlot, 0.15f, 0.2f);
			if (Reload)
			{
				bool bChanged = SetSoundNotify(Reload, EVaesenWeaponSound::ReloadOpen, Length * 0.1f);
				bChanged |= SetSoundNotify(Reload, EVaesenWeaponSound::ReloadInsert, FMath::Max(Insert - 0.05f, 0.0f));
				bChanged |= SetSoundNotify(Reload, EVaesenWeaponSound::ReloadClose, Length * 0.85f);
				if (bChanged)
				{
					Save(Reload);
				}
			}
			const float RoundStart = FMath::Max(Insert - 0.3f, 0.0f);
			ReloadRound = MakeTrimmedPistolMontage(TEXT("AM_Pistol_ReloadRound"), ReloadAnim, RoundStart, FMath::Min(Insert + 0.25f, Length));
			if (ReloadRound && SetSoundNotify(ReloadRound, EVaesenWeaponSound::ReloadInsert, FMath::Max(Insert - RoundStart - 0.05f, 0.0f)))
			{
				Save(ReloadRound);
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pistol reload %.2f s: the left hand meets the gun at %.2f s"), Length, Insert);
		}

		// Drawing from the right-hip holster and holstering there (the pack has neither): the relaxed stance from the
		// spine up, the hand reaching the holstered gun's grip (HandIK_Sheath up), taking it or letting go there.
		const float HolsterGrab = 0.32f;
		UAnimSequence* DrawAnim = CopyWithSheathCurve(Folder / TEXT("AS_Pistol_DrawFromHolster"), StandIdle,
			{ { 0.0f, 0.0f }, { 0.05f, 0.0f }, { HolsterGrab - 0.03f, 1.0f }, { HolsterGrab + 0.04f, 1.0f }, { HolsterGrab + 0.33f, 0.0f } });
		const bool bNewDraw = !Exists(Folder / TEXT("AM_Pistol_DrawFromHolster"));
		UAnimMontage* Draw = DrawAnim ? MakeTrimmedPistolMontage(TEXT("AM_Pistol_DrawFromHolster"), DrawAnim, 0.0f, 0.85f) : nullptr;
		if (Draw && bNewDraw)
		{
			AddNotify(Draw, NewObject<UAnimNotify_VaesenWeaponGrab>(Draw), HolsterGrab);
		}
		const float HolsterRelease = 0.33f;
		UAnimSequence* HolsterAnim = CopyWithSheathCurve(Folder / TEXT("AS_Pistol_Holster"), StandIdle,
			{ { 0.0f, 0.0f }, { 0.05f, 0.0f }, { HolsterRelease - 0.03f, 1.0f }, { HolsterRelease + 0.04f, 1.0f }, { HolsterRelease + 0.27f, 0.0f } });
		const bool bNewHolster = !Exists(Folder / TEXT("AM_Pistol_Holster"));
		UAnimMontage* Holster = HolsterAnim ? MakeTrimmedPistolMontage(TEXT("AM_Pistol_Holster"), HolsterAnim, 0.0f, 0.75f) : nullptr;
		if (Holster && bNewHolster)
		{
			AddNotify(Holster, NewObject<UAnimNotify_VaesenWeaponRelease>(Holster), HolsterRelease);
		}
		if (Draw)
		{
			SetSoundNotify(Draw, EVaesenWeaponSound::Draw, FMath::Max(HolsterGrab - SoundLead, 0.0f));
			Save(Draw);
		}
		if (Holster)
		{
			SetSoundNotify(Holster, EVaesenWeaponSound::Sheath, FMath::Max(HolsterRelease - 0.1f, 0.0f));
			Save(Holster);
		}
		if (FVaesenSheathLocation* Hip = Defaults->SheathLocations.Find(TEXT("Sheath_HipRight")))
		{
			Hip->DrawMontage = Draw;
			Hip->SheathMontage = Holster;
			Hip->ReferenceGrabPoint = FVector::ZeroVector;
			// A holster holds the gun still; it rides the thigh (below).
			Hip->HangAngle = 0.0f;
		}

		// The holster: on the right thigh (a tied-down holster, so it goes with the leg as he runs), the grip a little
		// out from the hip joint and up at the belt, the gun turned as the relaxed hand holds it (muzzle down, the top
		// to the front), so the hand takes it without a turn. Placed against the plain idle, which he stands in with the
		// gun holstered. Only over the grey-box guess (on the pelvis) or with -resetsockets / -resetsheath.
		const USkeletalMeshSocket* HolsterSocket = Skeleton->FindSocket(TEXT("Sheath_HipRight"));
		const bool bGreyBoxHolster = !HolsterSocket || HolsterSocket->BoneName == TEXT("pelvis");
		const bool bResetHolster = FParse::Param(FCommandLine::Get(), TEXT("resetsockets")) || FParse::Param(FCommandLine::Get(), TEXT("resetsheath"));
		const UAnimSequence* PlainIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
		if (bHold && StandIdle && PlainIdle && (bGreyBoxHolster || bResetHolster))
		{
			FQuat Held = (GunInHand * UAnimPoseExtensions::GetBonePose(PoseAt(StandIdle, HolsterGrab), TEXT("hand_r"), EAnimPoseSpaces::World)).GetRotation();
			const FTransform Thigh = UAnimPoseExtensions::GetBonePose(PoseAt(PlainIdle, 0.0), TEXT("thigh_r"), EAnimPoseSpaces::World);
			// Manny faces +Y; his right is -X. The gun lies flat against the outside of the hip and thigh: skin the mesh
			// into the idle (where he stands with it holstered), find the skin there (the outermost point to his right
			// of the hips and the right thigh; not the arm, which hangs alongside), and put the grip a centimetre out from it
			// (where a holster's leather would be), the barrel along the thigh. A little behind the hip joint, as a holster
			// rides, so the hanging hand is beside it rather than over it.
			FVector GripAt = Thigh.GetLocation() + FVector(-13.0, -5.0, 7.0);
			const USkeletalMesh* Manny = LoadObject<USkeletalMesh>(nullptr, MannyMesh);
			const FMeshDescription* Mesh = Manny ? Manny->GetMeshDescription(0) : nullptr;
			if (Mesh)
			{
				const FReferenceSkeleton& Ref = Manny->GetRefSkeleton();
				const FAnimPose IdlePose = PoseAt(PlainIdle, 0.0);
				TArray<FTransform> Skinning;
				TArray<bool> OnHipOrThigh;
				TArray<FTransform> RefWorld;
				for (int32 Bone = 0; Bone < Ref.GetNum(); ++Bone)
				{
					const int32 Parent = Ref.GetParentIndex(Bone);
					RefWorld.Add(Parent == INDEX_NONE ? Ref.GetRefBonePose()[Bone] : Ref.GetRefBonePose()[Bone] * RefWorld[Parent]);
					const FName Name = Ref.GetBoneName(Bone);
					Skinning.Add(RefWorld[Bone].Inverse() * UAnimPoseExtensions::GetBonePose(IdlePose, Name, EAnimPoseSpaces::World));
					const FString Text = Name.ToString();
					OnHipOrThigh.Add(Text == TEXT("pelvis") || (Text.StartsWith(TEXT("thigh")) && Text.EndsWith(TEXT("_r"))) || Text.StartsWith(TEXT("spine_01")));
				}
				// The skin in the idle: every vertex of the hips and the right thigh, skinned.
				TArray<FVector> Skin;
				const FSkeletalMeshConstAttributes Attributes(*Mesh);
				const FSkinWeightsVertexAttributesConstRef Weights = Attributes.GetVertexSkinWeights();
				const TVertexAttributesConstRef<FVector3f> Positions = Mesh->GetVertexPositions();
				for (const FVertexID Vertex : Mesh->Vertices().GetElementIDs())
				{
					FVector At = FVector::ZeroVector;
					float Strongest = 0.0f;
					bool bHipOrThigh = false;
					for (const UE::AnimationCore::FBoneWeight Weight : Weights.Get(Vertex))
					{
						const int32 Bone = Weight.GetBoneIndex();
						if (!Skinning.IsValidIndex(Bone))
						{
							continue;
						}
						At += Skinning[Bone].TransformPosition(FVector(Positions[Vertex])) * Weight.GetWeight();
						if (Weight.GetWeight() > Strongest)
						{
							Strongest = Weight.GetWeight();
							bHipOrThigh = OnHipOrThigh[Bone];
						}
					}
					if (bHipOrThigh)
					{
						Skin.Add(At);
					}
				}
				// The outermost point (to his right) over a band of heights below the grip.
				auto SkinAt = [&](double Below, double Above)
				{
					double Outermost = 0.0;
					for (const FVector& P : Skin)
					{
						if (P.Z >= GripAt.Z - Below && P.Z <= GripAt.Z - Above && FMath::Abs(P.Y - GripAt.Y) <= 5.0)
						{
							Outermost = FMath::Min(Outermost, P.X);
						}
					}
					return Outermost;
				};
				// The gun is some 20 cm from the grip to the muzzle; it follows the thigh rather than standing out from it.
				const double Top = SkinAt(6.0, 0.0);
				const double Bottom = SkinAt(24.0, 16.0);
				if (Top < -5.0 && Bottom < -5.0)
				{
					GripAt.X = Top - 1.0;
					const FVector Lean = FVector(Bottom - Top, 0.0, -18.0).GetSafeNormal();
					const FVector Barrel = Held.GetAxisX();
					const FVector BarrelSide = (Barrel - FVector::DotProduct(Barrel, FVector::YAxisVector) * FVector::YAxisVector).GetSafeNormal();
					Held = FQuat::FindBetweenNormals(BarrelSide, Lean) * Held;
				}
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Right-hip holster: in the idle the skin is %.1f cm to his right at the grip, %.1f cm at the muzzle (%d hip and thigh vertices)"),
					-Top, -Bottom, Skin.Num());
			}
			const FTransform Grip(Held, GripAt);
			FTransform OnThigh = Grip.GetRelativeTransform(Thigh);
			OnThigh.SetScale3D(FVector::OneVector);
			SetSkeletonSocket(Skeleton, TEXT("Sheath_HipRight"), TEXT("thigh_r"), OnThigh, true);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Right-hip holster: grip at %s (hip joint %s), barrel %s, top %s (mesh space; Manny faces +Y, his right is -X)"),
				*Grip.GetLocation().ToCompactString(), *Thigh.GetLocation().ToCompactString(), *Grip.GetUnitAxis(EAxis::X).ToCompactString(), *Grip.GetUnitAxis(EAxis::Z).ToCompactString());
			Save(Skeleton);
		}

		// The categories: the pistol (the rolling-block, a whole reload) and the revolver (round by round).
		for (const EVaesenWeaponCategory Category : { EVaesenWeaponCategory::Pistol, EVaesenWeaponCategory::Revolver })
		{
			FVaesenCategoryDefaults& Entry = Defaults->Categories.FindOrAdd(Category);
			Entry.AnimLayer = Graph;
			Entry.GripPose = AimIdle;
			Entry.Montages.Fire = Fire;
			Entry.Montages.FireAimed = Fire;
			Entry.Montages.Reload = Category == EVaesenWeaponCategory::Revolver ? ReloadRound : Reload;
			Entry.Montages.ReloadEmpty = Entry.Montages.Reload;
			if (bHold)
			{
				Entry.InHand = InHand;
			}
		}
		Save(Defaults);

		// The left hand on the gun when aiming (the pack aims with both hands), and how far out the sight is: measured
		// in the aimed idle, on our grey-box pistols (where not set already, or still the one-handed guess).
		if (bHold && AimIdle)
		{
			const FAnimPose Pose = PoseAt(AimIdle, 0.0);
			const FTransform Gun = GunInHand * UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World);
			const USkeletalMeshSocket* LeftPalm = Skeleton->FindSocket(TEXT("hand_l_weapon"));
			const USkeletalMeshSocket* Eye = Skeleton->FindSocket(TEXT("Eye_R"));
			if (LeftPalm && Eye)
			{
				FTransform LeftGrip = (LeftPalm->GetSocketLocalTransform() * UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_l"), EAnimPoseSpaces::World)).GetRelativeTransform(Gun);
				LeftGrip.SetScale3D(FVector::OneVector);
				const FVector EyeAt = (Eye->GetSocketLocalTransform() * UAnimPoseExtensions::GetBonePose(Pose, TEXT("head"), EAnimPoseSpaces::World)).GetLocation();
				const float SightOut = FMath::RoundToFloat(FVector::DotProduct(Gun.GetLocation() - EyeAt, Gun.GetUnitAxis(EAxis::X)));
				UE_LOG(LogVaesenAnimSetup, Display, TEXT("Pistol aimed: left palm on the gun at %s; the grip %.0f cm out from the eye"), *Describe(LeftGrip), SightOut);
				for (const TCHAR* MeshName : { TEXT("SM_GB_Revolver"), TEXT("SM_GB_Pistol") })
				{
					SetMeshSocket(LoadAt<UStaticMesh>(FString(TEXT("/Game/Vaesen/Combat/GreyBox")) / MeshName), TEXT("LeftGrip"), LeftGrip, false);
				}
				for (const TCHAR* WeaponName : { TEXT("BP_Weapon_Revolver"), TEXT("BP_Weapon_RollingBlockPistol") })
				{
					UBlueprint* Weapon = LoadAt<UBlueprint>(FString(TEXT("/Game/Vaesen/Blueprints/Items/Weapons")) / WeaponName);
					UVaesenWeaponItem* Item = Weapon ? Cast<UVaesenWeaponItem>(Weapon->GeneratedClass->GetDefaultObject()) : nullptr;
					if (Item && Item->Hold.AimSightOffset.Equals(FVector(52.0, 0.0, 0.0)) && SightOut > 20.0f)
					{
						Item->Hold.AimSightOffset = FVector(SightOut, 0.0, 0.0);
						FBlueprintEditorUtils::MarkBlueprintAsModified(Weapon);
						FKismetEditorUtilities::CompileBlueprint(Weapon);
						Save(Weapon);
					}
				}
			}
		}
	}

	// --- The bow (the GhostSamurai pack) ---

	const TCHAR* BowFolder = TEXT("/Game/Vaesen/Animations/Bow");
	const TCHAR* BowPackFolder = TEXT("/Game/GhostSamurai_Bundle/GhostSamurai_Bundle_UE5");
	const TCHAR* BowGraphPath = TEXT("/Game/Vaesen/Animations/Bow/ABP_BowLocomotion");

	/** A pack animation (GhostSamurai_Bow_<Name>, in Bow/<Group>), on the pack's own skeleton (its extra bow, string and arrow bones). */
	UAnimSequence* BowSource(const TCHAR* Group, const TCHAR* Name)
	{
		const FString Short = FString(TEXT("GhostSamurai_Bow_")) + Name;
		const FString Folder = FString(BowPackFolder) / TEXT("Bow");
		return LoadAt<UAnimSequence>(*Group ? Folder / Group / Short : Folder / Short);
	}

	/**
	 * Our copy of a pack animation, on Manny's skeleton (made once), root locked (in place). Manny has no bow, string or
	 * arrow bones, so the arrow (on the pack's arrow bone, under hand_r; hidden by scaling it to nothing) is baked first
	 * into curves: BowArrow (shown) and where it is on hand_r (the six BowArrow transform curves).
	 */
	UAnimSequence* BowAnim(const TCHAR* Group, const TCHAR* Name)
	{
		const FString ShortName = FString(Name).Replace(TEXT("_Root"), TEXT(""));
		const FString Path = FString(BowFolder) / (TEXT("AS_Bow_") + ShortName);
		if (UAnimSequence* Existing = LoadAt<UAnimSequence>(Path))
		{
			return Existing;
		}
		UAnimSequence* Source = BowSource(Group, Name);
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		if (!Source || !Skeleton)
		{
			UE_LOG(LogVaesenAnimSetup, Warning, TEXT("Bow pack animation %s/%s is missing."), Group, Name);
			return nullptr;
		}

		// The arrow through the animation, 30 times a second.
		const double Length = Source->GetPlayLength();
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length * 30.0));
		TArray<FRichCurveKey> Shown;
		TArray<FRichCurveKey> Parts[6];
		// And the string: its pull from where it rests on the bow (the pack bow's String from its bow bone, at rest).
		TArray<FRichCurveKey> Pull[3];
		FVector StringRest(0.0, 0.0, 17.0);
		if (const USkeletalMesh* PackBow = LoadAt<USkeletalMesh>(FString(BowPackFolder) / TEXT("Weapon/Mesh/Bow/SK_Bow_01")))
		{
			const FReferenceSkeleton& Ref = PackBow->GetRefSkeleton();
			const int32 BowBone = Ref.FindBoneIndex(TEXT("bow"));
			const int32 StringBone = Ref.FindBoneIndex(TEXT("String"));
			if (BowBone != INDEX_NONE && StringBone != INDEX_NONE)
			{
				StringRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, StringBone)
					.GetRelativeTransform(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, BowBone)).GetLocation();
			}
		}
		FRotator Previous = FRotator::ZeroRotator;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Time = Length * Step / Steps;
			const FAnimPose Pose = PoseAt(Source, Time);
			const FTransform ArrowWorld = UAnimPoseExtensions::GetBonePose(Pose, TEXT("arrow"), EAnimPoseSpaces::World);
			const FTransform Arrow = ArrowWorld.GetRelativeTransform(UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World));
			FRichCurveKey& ShownKey = Shown.Emplace_GetRef(static_cast<float>(Time), ArrowWorld.GetScale3D().X > 0.5 ? 1.0f : 0.0f);
			ShownKey.InterpMode = RCIM_Constant;
			// Unwound against the frame before (no 359 -> 0 jumps between keys).
			FRotator Rotation = Arrow.Rotator();
			if (Step > 0)
			{
				Rotation.Pitch = Previous.Pitch + FMath::UnwindDegrees(Rotation.Pitch - Previous.Pitch);
				Rotation.Yaw = Previous.Yaw + FMath::UnwindDegrees(Rotation.Yaw - Previous.Yaw);
				Rotation.Roll = Previous.Roll + FMath::UnwindDegrees(Rotation.Roll - Previous.Roll);
			}
			Previous = Rotation;
			const float Values[6] = { float(Arrow.GetLocation().X), float(Arrow.GetLocation().Y), float(Arrow.GetLocation().Z),
				float(Rotation.Pitch), float(Rotation.Yaw), float(Rotation.Roll) };
			for (int32 Index = 0; Index < 6; ++Index)
			{
				Parts[Index].Emplace_GetRef(static_cast<float>(Time), Values[Index]).InterpMode = RCIM_Linear;
			}
			const FVector String = UAnimPoseExtensions::GetBonePose(Pose, TEXT("String"), EAnimPoseSpaces::World)
				.GetRelativeTransform(UAnimPoseExtensions::GetBonePose(Pose, TEXT("bow"), EAnimPoseSpaces::World)).GetLocation() - StringRest;
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Pull[Index].Emplace_GetRef(static_cast<float>(Time), static_cast<float>(String[Index])).InterpMode = RCIM_Linear;
			}
		}

		UPackage* Package = CreatePackage(*Path);
		UAnimSequence* Copy = DuplicateObject<UAnimSequence>(Source, Package, FName(FPackageName::GetShortName(Path)));
		Copy->SetFlags(RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Copy);
		Copy->ReplaceSkeleton(Skeleton);
		Copy->bEnableRootMotion = false;
		Copy->bForceRootLock = true;

		IAnimationDataController& Controller = Copy->GetController();
		Controller.OpenBracket(FText::FromString(TEXT("Bow arrow curves")), false);
		const auto SetCurve = [&Controller](FName Name, const TArray<FRichCurveKey>& Keys)
		{
			const FAnimationCurveIdentifier Curve(Name, ERawCurveTrackTypes::RCT_Float);
			Controller.AddCurve(Curve, AACF_DefaultCurve, false);
			Controller.SetCurveKeys(Curve, Keys, false);
		};
		SetCurve(UVaesenAnimInstance::BowArrowCurve, Shown);
		for (int32 Index = 0; Index < 3; ++Index)
		{
			SetCurve(UVaesenAnimInstance::BowStringCurves[Index], Pull[Index]);
		}
		for (int32 Index = 0; Index < 6; ++Index)
		{
			SetCurve(UVaesenAnimInstance::BowArrowTransformCurves[Index], Parts[Index]);
		}
		Controller.CloseBracket(false);
		Copy->PostEditChange();
		Save(Copy);
		return Copy;
	}

	/** Where the pack's bow bone is from hand_l (the left fist) in a pack animation at Time. */
	FTransform BowInFist(const UAnimSequence* Source, double Time)
	{
		const FAnimPose Pose = PoseAt(Source, Time);
		FTransform Bow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("bow"), EAnimPoseSpaces::World)
			.GetRelativeTransform(UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_l"), EAnimPoseSpaces::World));
		Bow.SetScale3D(FVector::OneVector);
		return Bow;
	}

	/** When the bow comes into the fist (the first moment it's where the fist holds it, Held) or last leaves it. */
	float BowHandoffTime(const UAnimSequence* Source, const FTransform& Held, bool bFirst)
	{
		const double Length = Source->GetPlayLength();
		const int32 Steps = 60;
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const double Time = Length * (bFirst ? Step : Steps - Step) / Steps;
			if (FVector::Dist(BowInFist(Source, Time).GetLocation(), Held.GetLocation()) < 1.5)
			{
				return static_cast<float>(Time);
			}
		}
		return bFirst ? static_cast<float>(Length) : 0.0f;
	}

	/** The bow's aim offset (pitch): the pack's front aim poses (down, centre, up) at the pitch the arrow points in each. */
	UBlendSpace* MakeBowAimOffset(bool bRebuild)
	{
		const FString Path = FString(BowFolder) / TEXT("AO1D_Bow");
		UBlendSpace* Offset = LoadAt<UBlendSpace>(Path);
		if (Offset && !bRebuild)
		{
			return Offset;
		}
		const TCHAR* Group = TEXT("Attack/AimPose");
		UAnimSequence* Center = BowAnim(Group, TEXT("Shoot_AimPose_F"));
		UAnimSequence* Up = BowAnim(Group, TEXT("Shoot_AimPose_Up"));
		UAnimSequence* Down = BowAnim(Group, TEXT("Shoot_AimPose_Down"));
		if (!Center || !Up || !Down)
		{
			return Offset;
		}
		// Where the arrow points in each pose (its bone's +Y, in the pack's own poses).
		const auto PitchOf = [Group](const TCHAR* Name)
		{
			const UAnimSequence* Pose = BowSource(Group, Name);
			if (!Pose)
			{
				return 0.0f;
			}
			const FVector Along = UAnimPoseExtensions::GetBonePose(PoseAt(Pose, 0.0), TEXT("arrow"), EAnimPoseSpaces::World).GetUnitAxis(EAxis::Y);
			return static_cast<float>(FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Along.Z, -1.0, 1.0))));
		};
		const float CenterPitch = PitchOf(TEXT("Shoot_AimPose_F"));
		const float UpPitch = PitchOf(TEXT("Shoot_AimPose_Up")) - CenterPitch;
		const float DownPitch = PitchOf(TEXT("Shoot_AimPose_Down")) - CenterPitch;
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("Bow aim poses: up %.0f, down %.0f degrees (from the centre's %.0f)"), UpPitch, DownPitch, CenterPitch);

		for (UAnimSequence* Pose : { Center, Up, Down })
		{
			if (Pose->AdditiveAnimType != AAT_RotationOffsetMeshSpace)
			{
				Pose->AdditiveAnimType = AAT_RotationOffsetMeshSpace;
				Pose->RefPoseType = ABPT_AnimFrame;
				Pose->RefPoseSeq = Center;
				Pose->RefFrameIndex = 0;
				Pose->PostEditChange();
				Save(Pose);
			}
		}
		if (!Offset)
		{
			UBlendSpace* Rifle = RifleAsset<UBlendSpace>(TEXT("AO1D_Rifle_Aim"));
			if (!Rifle)
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*Path);
			Offset = DuplicateObject<UBlendSpace>(Rifle, Package, FName(TEXT("AO1D_Bow")));
			Offset->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Offset);
		}
		if (UpPitch > 1.0f && DownPitch < -1.0f)
		{
			while (Offset->GetNumberOfBlendSamples() > 0)
			{
				Offset->DeleteSample(0);
			}
			const TPair<UAnimSequence*, float> Samples[] = { { Down, DownPitch }, { Center, 0.0f }, { Up, UpPitch } };
			for (const TPair<UAnimSequence*, float>& Sample : Samples)
			{
				Offset->ExpandRangeForSample(FVector(Sample.Value, 0.0, 0.0));
				Offset->AddSample(Sample.Key, FVector(Sample.Value, 0.0, 0.0));
			}
			Offset->ValidateSampleData();
			Offset->ResampleData();
			Offset->PostEditChange();
			Save(Offset);
		}
		return Offset;
	}

	/**
	 * BS_Bow: the archer with the bow in hand, a direction/speed blend space set like the rifle's (BS_Rifle duplicated):
	 * the pack's idle standing, its walk and run in eight directions, each at its own speed (measured in the pack's root
	 * motion; our copies are in place). Made once; -rebuild remakes it while the pack's there.
	 */
	UBlendSpace* MakeBowBlendSpace(bool bRebuild)
	{
		const FString Path = FString(BowFolder) / TEXT("BS_Bow");
		UBlendSpace* Space = LoadAt<UBlendSpace>(Path);
		if (Space && (!bRebuild || !BowSource(TEXT("Movement/Walk"), TEXT("Walk_F_Root"))))
		{
			return Space;
		}
		if (!Space)
		{
			UBlendSpace* Rifle = RifleAsset<UBlendSpace>(TEXT("BS_Rifle"));
			if (!Rifle)
			{
				return nullptr;
			}
			UPackage* Package = CreatePackage(*Path);
			Space = DuplicateObject<UBlendSpace>(Rifle, Package, FName(TEXT("BS_Bow")));
			Space->SetFlags(RF_Public | RF_Standalone);
			FAssetRegistryModule::AssetCreated(Space);
		}
		while (Space->GetNumberOfBlendSamples() > 0)
		{
			Space->DeleteSample(0);
		}
		const auto Add = [Space](UAnimSequence* Anim, float Direction, float Speed)
		{
			if (Anim)
			{
				const FVector Value(Direction, Speed, 0.0);
				Space->ExpandRangeForSample(Value);
				Space->AddSample(Anim, Value);
			}
		};
		UAnimSequence* Idle = BowAnim(TEXT(""), TEXT("Idle_Root"));
		for (const float Direction : { -180.0f, -90.0f, 0.0f, 90.0f, 180.0f })
		{
			Add(Idle, Direction, 0.0f);
		}
		const TPair<const TCHAR*, float> Ways[] = { { TEXT("F"), 0.0f }, { TEXT("FR"), 45.0f }, { TEXT("R"), 90.0f }, { TEXT("BR"), 135.0f },
			{ TEXT("B"), 180.0f }, { TEXT("BL"), -135.0f }, { TEXT("L"), -90.0f }, { TEXT("FL"), -45.0f } };
		for (const TCHAR* Gait : { TEXT("Walk"), TEXT("Run") })
		{
			for (const TPair<const TCHAR*, float>& Way : Ways)
			{
				const FString Name = FString::Printf(TEXT("%s_%s_Root"), Gait, Way.Key);
				const FString Group = FString(TEXT("Movement/")) + Gait;
				const UAnimSequence* Source = BowSource(*Group, *Name);
				if (!Source)
				{
					continue;
				}
				const FSwingTiming Timing = MeasureSwing(Source);
				const float Speed = FMath::RoundToFloat(Timing.RootMove.Size2D() / FMath::Max(Timing.Length, 0.01f));
				Add(BowAnim(*Group, *Name), Way.Value, Speed);
				// Straight back also at -180, so the space wraps round.
				if (Way.Value == 180.0f)
				{
					Add(BowAnim(*Group, *Name), -180.0f, Speed);
				}
			}
		}
		Space->ValidateSampleData();
		Space->ResampleData();
		Space->PostEditChange();
		Save(Space);
		UE_LOG(LogVaesenAnimSetup, Display, TEXT("BS_Bow: %d samples, speed up to %.0f"), Space->GetNumberOfBlendSamples(), Space->GetBlendParameter(1).Max);
		return Space;
	}

	/**
	 * ABP_BowLocomotion, linked into the character's graph while a bow is drawn: the plain locomotion (ABP_Manny), the bow
	 * hanging at his side; nocked (bAiming), the pack's full-draw hold from the spine up, aimed up and down (AimPitch),
	 * over the pack's archer's legs (BS_Bow: he strafes then, facing the aim). The nock and draw, and the release, are
	 * montages over it.
	 */
	UClass* BuildBowLocomotion(USkeleton* Skeleton, bool bRebuild, UBlendSpace* Movement, UAnimSequence* Hold, UBlendSpace* AimOffset, UAnimSequence* Low, float LowTime)
	{
		if (!bRebuild)
		{
			// Made before the low stance was, or with it at another moment: remade.
			UAnimBlueprint* Existing = LoadAt<UAnimBlueprint>(BowGraphPath);
			bool bHasLow = false;
			for (UEdGraph* Graph : Existing ? Existing->FunctionGraphs : TArray<TObjectPtr<UEdGraph>>())
			{
				for (UEdGraphNode* Node : Graph ? Graph->Nodes : TArray<TObjectPtr<UEdGraphNode>>())
				{
					const UAnimGraphNode_SequenceEvaluator* Evaluator = Cast<UAnimGraphNode_SequenceEvaluator>(Node);
					bHasLow |= Evaluator && FMath::IsNearlyEqual(Evaluator->Node.GetExplicitTime(), LowTime, 0.001f);
				}
			}
			if (Existing && (bHasLow || !Low))
			{
				return Existing->GeneratedClass;
			}
		}
		UAnimBlueprint* Blueprint = OpenAnimBlueprint(BowGraphPath, Skeleton);
		FGraphBuilder Builder(Blueprint);
		if (!Blueprint || !Builder.Graph)
		{
			return nullptr;
		}
		UAnimGraphNode_BlendSpacePlayer* Moving = Builder.Add<UAnimGraphNode_BlendSpacePlayer>(-1700, 0, [Movement](UAnimGraphNode_BlendSpacePlayer* Node)
		{
			Node->Node.SetBlendSpace(Movement);
		});
		Builder.Var(TEXT("Direction"), Moving, TEXT("X"));
		Builder.Var(TEXT("Speed"), Moving, TEXT("Y"));
		UAnimGraphNode_SaveCachedPose* MovingCache = Builder.Add<UAnimGraphNode_SaveCachedPose>(-1400, 0, [](UAnimGraphNode_SaveCachedPose* Node)
		{
			Node->CacheName = TEXT("BowMoving");
		});
		Builder.Pose(Moving, MovingCache, TEXT("Pose"));
		const auto UseMoving = [&Builder, MovingCache](int32 Y)
		{
			return Builder.Add<UAnimGraphNode_UseCachedPose>(-1150, Y, [MovingCache](UAnimGraphNode_UseCachedPose* Node)
			{
				Node->SaveCachedPoseNode = MovingCache;
			});
		};

		// Nocked: the full-draw hold, aimed up and down, from the spine up.
		UAnimGraphNode_SequencePlayer* Held = Builder.Add<UAnimGraphNode_SequencePlayer>(-1600, 250, [Hold](UAnimGraphNode_SequencePlayer* Node)
		{
			Node->Node.SetSequence(Hold);
		});
		UAnimGraphNode_RotationOffsetBlendSpace* Aim = Builder.Add<UAnimGraphNode_RotationOffsetBlendSpace>(-1250, 250, [AimOffset](UAnimGraphNode_RotationOffsetBlendSpace* Node)
		{
			Node->Node.SetBlendSpace(AimOffset);
		});
		Builder.Pose(Held, Aim, TEXT("BasePose"));
		Builder.Var(TEXT("AimPitch"), Aim, TEXT("X"));
		UAnimGraphNode_LayeredBoneBlend* Upper = Builder.Add<UAnimGraphNode_LayeredBoneBlend>(-900, 150, [](UAnimGraphNode_LayeredBoneBlend* Node)
		{
			Node->Node.bMeshSpaceRotationBlend = true;
			if (Node->Node.LayerSetup.Num() == 0)
			{
				Node->Node.LayerSetup.AddDefaulted();
			}
			FBranchFilter Filter;
			Filter.BoneName = TEXT("spine_01");
			Filter.BlendDepth = 0;
			Node->Node.LayerSetup[0].BranchFilters = { Filter };
		});
		Builder.Pose(UseMoving(100), Upper, TEXT("BasePose"));
		// Nocked but not aimed (bBowLowReady): the bow held low in front, the arrow on the string pointing down ahead
		// (the pack's lowering of a drawn bow, where the arrow's still on the string).
		UEdGraphNode* Nocked = Aim;
		if (Low)
		{
			UAnimGraphNode_SequenceEvaluator* LowPose = Builder.Add<UAnimGraphNode_SequenceEvaluator>(-1250, 500, [Low, LowTime](UAnimGraphNode_SequenceEvaluator* Node)
			{
				Node->Node.SetSequence(Low);
				Node->Node.SetExplicitTime(LowTime);
			});
			Nocked = ByBool(Builder, -1050, 300, TEXT("bBowLowReady"), LowPose, Aim, 0.2f);
		}
		Builder.Pose(Nocked, Upper, TEXT("BlendPoses_0"));
		// Not nocked: the plain locomotion (ABP_Manny), standing, walking, running and jumping, the bow hanging at his side.
		UClass* Plain = LoadObject<UClass>(nullptr, UnarmedGraphPath);
		UAnimGraphNode_LinkedAnimGraph* PlainGraph = Builder.Add<UAnimGraphNode_LinkedAnimGraph>(-900, 450, [Plain](UAnimGraphNode_LinkedAnimGraph* Node)
		{
			Node->Node.InstanceClass = Plain;
		});
		UEdGraphNode* Final = ByBool(Builder, -300, 100, TEXT("bAiming"), Upper, PlainGraph, 0.2f);
		Builder.Pose(Final, Builder.Root(), TEXT("Result"));
		CompileAndSave(Blueprint);
		return Blueprint->GeneratedClass;
	}

	/**
	 * M_ArrowStreak: the faint streak behind an arrow in flight. Unlit and see-through, a warm off-white; along the rod
	 * (the engine cylinder's local Z, -50 to 50) it fades from the arrow's end (Opacity) to nothing at the tail.
	 */
	UMaterialInterface* MakeStreakMaterial()
	{
		const FString Path = TEXT("/Game/Vaesen/Materials/M_ArrowStreak");
		if (UMaterial* Existing = LoadAt<UMaterial>(Path))
		{
			return Existing;
		}
		UMaterial* Material = NewAsset<UMaterial>(Path);
		Material->BlendMode = BLEND_Translucent;
		Material->SetShadingModel(MSM_Unlit);
		Material->TwoSided = true;
		const auto Add = [Material](TSubclassOf<UMaterialExpression> Class, int32 X, int32 Y)
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
		};
		UMaterialExpressionVectorParameter* Color = CastChecked<UMaterialExpressionVectorParameter>(Add(UMaterialExpressionVectorParameter::StaticClass(), -500, -100));
		Color->ParameterName = TEXT("Color");
		Color->DefaultValue = FLinearColor(1.0f, 0.92f, 0.78f);
		UMaterialExpressionScalarParameter* Opacity = CastChecked<UMaterialExpressionScalarParameter>(Add(UMaterialExpressionScalarParameter::StaticClass(), -500, 150));
		Opacity->ParameterName = TEXT("Opacity");
		Opacity->DefaultValue = 0.35f;
		UMaterialExpressionLocalPosition* Local = CastChecked<UMaterialExpressionLocalPosition>(Add(UMaterialExpressionLocalPosition::StaticClass(), -500, 300));
		UMaterialExpressionCustom* Fade = CastChecked<UMaterialExpressionCustom>(Add(UMaterialExpressionCustom::StaticClass(), -250, 200));
		Fade->Description = TEXT("Fade");
		Fade->OutputType = CMOT_Float1;
		Fade->Inputs.Reset();
		for (const TCHAR* Name : { TEXT("Local"), TEXT("Opacity") })
		{
			Fade->Inputs.AddDefaulted_GetRef().InputName = Name;
		}
		Fade->Code = TEXT("float Along = saturate((Local.z + 50.0) / 100.0);\nreturn Along * Along * Opacity;");
		UMaterialEditingLibrary::ConnectMaterialExpressions(Local, TEXT(""), Fade, TEXT("Local"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Opacity, TEXT(""), Fade, TEXT("Opacity"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Color, TEXT(""), MP_EmissiveColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Fade, TEXT(""), MP_Opacity);
		UMaterialEditingLibrary::RecompileMaterial(Material);
		Save(Material);
		return Material;
	}

	void BuildBow()
	{
		const bool bRebuild = FParse::Param(FCommandLine::Get(), TEXT("rebuild"));
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, MannySkeletonPath);
		UVaesenWeaponCategoryDefaults* Defaults = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"));
		const UAnimSequence* HoldSource = BowSource(TEXT("Attack"), TEXT("Shoot_Aim_Idle_Root"));
		if (!Skeleton || !Defaults || (!HoldSource && !Exists(BowGraphPath)))
		{
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Bow pack (/Game/GhostSamurai_Bundle) not in the project; skipping the bow."));
			return;
		}
		const FString Folder = BowFolder;

		// The hold: the pack's full draw (the draw ends in it, the release starts from it), the aim offset over it.
		UAnimSequence* Hold = BowAnim(TEXT("Attack"), TEXT("Shoot_Aim_Idle_Root"));
		UBlendSpace* AimOffset = MakeBowAimOffset(bRebuild);
		// Held low, nocked: the pack's lowering of a drawn bow (Shoot_Cancel) where the bow's lowest with the arrow still on
		// the string and the drawing hand in front (later it goes back to the quiver).
		UAnimSequence* Lowering = BowAnim(TEXT("Attack"), TEXT("Shoot_Cancel_Root"));
		float LowTime = 0.0f;
		if (const UAnimSequence* Source = BowSource(TEXT("Attack"), TEXT("Shoot_Cancel_Root")))
		{
			double Lowest = TNumericLimits<double>::Max();
			const double Length = Source->GetPlayLength();
			for (int32 Step = 0; Step <= 60; ++Step)
			{
				const double Time = Length * Step / 60.0;
				const FAnimPose Pose = PoseAt(Source, Time);
				const FTransform Pelvis = UAnimPoseExtensions::GetBonePose(Pose, TEXT("pelvis"), EAnimPoseSpaces::World);
				// The pack's character faces +Y.
				const FVector Right = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_r"), EAnimPoseSpaces::World).GetLocation() - Pelvis.GetLocation();
				const FVector Left = UAnimPoseExtensions::GetBonePose(Pose, TEXT("hand_l"), EAnimPoseSpaces::World).GetLocation() - Pelvis.GetLocation();
				// On the string, pointing ahead (down a little at most), not on its way back to the quiver.
				const FTransform Arrow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("arrow"), EAnimPoseSpaces::World);
				const bool bOnString = Arrow.GetScale3D().X > 0.5 && Arrow.GetUnitAxis(EAxis::Y).Y > 0.75;
				// The drawing hand still on the string, near the bow (it soon leaves for the quiver, the arrow with it).
				if (bOnString && Right.Y > 15.0 && FVector::Dist(Right, Left) < 55.0 && Left.Z < Lowest)
				{
					Lowest = Left.Z;
					LowTime = static_cast<float>(Time);
				}
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Bow held low, nocked: Shoot_Cancel at %.2f s of %.2f (the bow hand %.0f cm above the pelvis)"), LowTime, Length, Lowest);
		}
		UClass* Graph = BuildBowLocomotion(Skeleton, bRebuild, MakeBowBlendSpace(bRebuild), Hold, AimOffset, LowTime > 0.0f ? Lowering : nullptr, LowTime);

		// Taking an arrow from the quiver, nocking and drawing; the release, the bow lowered.
		UAnimMontage* Draw = MakeMontage(Folder / TEXT("AM_Bow_NockAndDraw"), BowAnim(TEXT("Attack"), TEXT("Shoot_Start_Root")), UpperBodySlot, 0.15f, 0.15f);
		UAnimMontage* Release = MakeMontage(Folder / TEXT("AM_Bow_Release"), BowAnim(TEXT("Attack"), TEXT("Shoot_End_Root")), UpperBodySlot, 0.05f, 0.25f);
		// Held low, the lowering run backwards: from the bow held low without an arrow, the hand to the quiver and the arrow
		// onto the string (NockLow); and from there up to full draw, level (RaiseToShoot, then the release).
		UAnimMontage* NockLow = nullptr;
		UAnimMontage* Raise = nullptr;
		if (Lowering && LowTime > 0.0f)
		{
			NockLow = MakeMontage(Folder / TEXT("AM_Bow_NockLow"), Lowering, UpperBodySlot, 0.15f, 0.15f);
			Raise = MakeMontage(Folder / TEXT("AM_Bow_RaiseToShoot"), Lowering, UpperBodySlot, 0.05f, 0.05f);
			const TPair<UAnimMontage*, FVector2D> Parts[] = { { NockLow, FVector2D(LowTime, Lowering->GetPlayLength()) }, { Raise, FVector2D(0.0, LowTime) } };
			for (const TPair<UAnimMontage*, FVector2D>& Part : Parts)
			{
				if (Part.Key)
				{
					TrimMontage(Part.Key, static_cast<float>(Part.Value.X), static_cast<float>(Part.Value.Y));
					Part.Key->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimPlayRate = -1.0f;
					Part.Key->RefreshCacheData();
				}
			}
		}
		for (UAnimMontage* Montage : { Draw, Release, NockLow, Raise })
		{
			if (Montage)
			{
				Save(Montage);
			}
		}

		// Hit: the archer's own (in place).
		TArray<TObjectPtr<UAnimMontage>> Hits;
		for (const TCHAR* Hit : { TEXT("Hit_F_Root"), TEXT("Hit_FL_Root"), TEXT("Hit_FR_Root") })
		{
			const FString Name = FString(TEXT("AM_Bow_")) + FString(Hit).Replace(TEXT("_Root"), TEXT(""));
			if (UAnimMontage* Reaction = MakeMontage(Folder / Name, BowAnim(TEXT("Hit"), Hit), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.25f))
			{
				Save(Reaction);
				Hits.Add(Reaction);
			}
		}
		UAnimMontage* HitBehind = MakeMontage(Folder / TEXT("AM_Bow_Hit_B"), BowAnim(TEXT("Hit"), TEXT("Hit_B_Root")), FAnimSlotGroup::DefaultSlotName, 0.05f, 0.25f);
		if (HitBehind)
		{
			Save(HitBehind);
		}

		// The bow in the fist as the pack holds it at full draw (where the aim matters); on the back as the pack carries it.
		const UAnimSequence* CarrySource = BowSource(TEXT("Common"), TEXT("Common_Idle_Root"));
		const FTransform InFist = HoldSource ? BowInFist(HoldSource, 0.0) : FTransform::Identity;
		// Where the pack carries it, but lying flat against the back (its thin side, X, into the back; the pack's stands
		// out from it), the limbs as the pack has them.
		if (CarrySource)
		{
			const FAnimPose Pose = PoseAt(CarrySource, 0.0);
			const FTransform Bow = UAnimPoseExtensions::GetBonePose(Pose, TEXT("bow"), EAnimPoseSpaces::World);
			const FTransform Chest = UAnimPoseExtensions::GetBonePose(Pose, TEXT("spine_05"), EAnimPoseSpaces::World);
			const FVector Limbs = Bow.GetUnitAxis(EAxis::Y);
			const FVector ToBack = FVector::VectorPlaneProject(Chest.GetLocation() - Bow.GetLocation(), Limbs).GetSafeNormal();
			FTransform OnBack = FTransform(FRotationMatrix::MakeFromYX(Limbs, ToBack).ToQuat(), Bow.GetLocation()).GetRelativeTransform(Chest);
			OnBack.SetScale3D(FVector::OneVector);
			const USkeletalMeshSocket* Existing = Skeleton->FindSocket(TEXT("Sheath_BowBack"));
			if (Existing && Existing->GetSocketLocalTransform().Equals(OnBack, 0.01))
			{
				OnBack = Existing->GetSocketLocalTransform();
			}
			else
			{
				SetSkeletonSocket(Skeleton, TEXT("Sheath_BowBack"), TEXT("spine_05"), OnBack, true);
				Save(Skeleton);
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Bow on the back (spine_05): %s; in the fist: %s"), *Describe(OnBack), *Describe(InFist));
		}

		// Taking it from the back and putting it there (the pack's): the bow into the fist, and out of it, where it's held.
		UAnimMontage* FromBack = nullptr;
		UAnimMontage* ToBack = nullptr;
		const UAnimSequence* EquipSource = BowSource(TEXT("Common/EquipAndUnarm"), TEXT("Common_Equip_Root"));
		const UAnimSequence* UnarmSource = BowSource(TEXT("Common/EquipAndUnarm"), TEXT("Common_Unarm_Root"));
		if (EquipSource && UnarmSource)
		{
			const FTransform Fist = BowInFist(EquipSource, EquipSource->GetPlayLength());
			const float Grab = BowHandoffTime(EquipSource, Fist, true);
			const float Letgo = BowHandoffTime(UnarmSource, Fist, false);
			const bool bNewFrom = !Exists(Folder / TEXT("AM_Bow_FromBack"));
			const bool bNewTo = !Exists(Folder / TEXT("AM_Bow_ToBack"));
			FromBack = MakeMontage(Folder / TEXT("AM_Bow_FromBack"), BowAnim(TEXT("Common/EquipAndUnarm"), TEXT("Common_Equip_Root")), UpperBodySlot, 0.1f, 0.2f);
			ToBack = MakeMontage(Folder / TEXT("AM_Bow_ToBack"), BowAnim(TEXT("Common/EquipAndUnarm"), TEXT("Common_Unarm_Root")), UpperBodySlot, 0.1f, 0.2f);
			if (FromBack && bNewFrom)
			{
				AddNotify(FromBack, NewObject<UAnimNotify_VaesenWeaponGrab>(FromBack), Grab);
			}
			if (ToBack && bNewTo)
			{
				AddNotify(ToBack, NewObject<UAnimNotify_VaesenWeaponRelease>(ToBack), Letgo);
			}
			if (FromBack)
			{
				SetSoundNotify(FromBack, EVaesenWeaponSound::Draw, FMath::Max(Grab - SoundLead, 0.0f));
				Save(FromBack);
			}
			if (ToBack)
			{
				SetSoundNotify(ToBack, EVaesenWeaponSound::Sheath, FMath::Max(Letgo - 0.1f, 0.0f));
				Save(ToBack);
			}
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("Bow: taken from the back at %.2f s, let go there at %.2f s"), Grab, Letgo);
		}

		// The category: in the left fist, the locomotion and hold above, the nock and draw (the bow ability plays the
		// reload montage for it), the release, the hits, the fist's grip.
		FVaesenCategoryDefaults& Bow = Defaults->Categories.FindOrAdd(EVaesenWeaponCategory::Bow);
		Bow.AnimLayer = Graph;
		Bow.bHeldInLeftHand = true;
		if (HoldSource)
		{
			Bow.InHand = InFist;
		}
		// The fist round the bow: a pose with none of the bow's curves (the grip's layer would put its own over the
		// animation's, hiding the arrow and slackening the string).
		const FString GripPath = Folder / TEXT("AS_Bow_GripPose");
		UAnimSequence* Grip = LoadAt<UAnimSequence>(GripPath);
		if (!Grip)
		{
			if (UAnimSequence* Source = BowSource(TEXT("Hit"), TEXT("Hit_F_Root")))
			{
				Grip = DuplicateObject<UAnimSequence>(Source, CreatePackage(*GripPath), FName(TEXT("AS_Bow_GripPose")));
				Grip->SetFlags(RF_Public | RF_Standalone);
				FAssetRegistryModule::AssetCreated(Grip);
				Grip->ReplaceSkeleton(Skeleton);
				Grip->bEnableRootMotion = false;
				Grip->bForceRootLock = true;
				Grip->PostEditChange();
				Save(Grip);
			}
		}
		Bow.GripPose = Grip;
		Bow.Montages.Reload = Draw;
		Bow.Montages.ReloadEmpty = Draw;
		Bow.Montages.Fire = Release;
		Bow.Montages.FireAimed = Release;
		Bow.Montages.NockLow = NockLow;
		Bow.Montages.RaiseToShoot = Raise;
		if (Hits.Num() > 0)
		{
			Bow.Montages.HitReactions = Hits;
			Bow.Montages.HitReactionBehind = HitBehind;
		}
		Save(Defaults);

		// The hunting bow: the pack's bow and arrow, carried on the back where the pack carries it, taken and put back with
		// the pack's motions; the arrow leaves from the rest above the grip, toward the belly (-Z).
		const FString PackMeshes = FString(BowPackFolder) / TEXT("Weapon/Mesh/Bow");
		const FString OurMeshes = TEXT("/Game/Vaesen/ThirdParty/GhostSamurai_Bundle/GhostSamurai_Bundle_UE5/Weapon/Mesh/Bow");
		UStaticMesh* BowMesh = LoadAt<UStaticMesh>(OurMeshes / TEXT("SM_Bow_01"));
		BowMesh = BowMesh ? BowMesh : LoadAt<UStaticMesh>(PackMeshes / TEXT("SM_Bow_01"));
		UStaticMesh* ArrowMesh = LoadAt<UStaticMesh>(OurMeshes / TEXT("SM_Arrow_01"));
		ArrowMesh = ArrowMesh ? ArrowMesh : LoadAt<UStaticMesh>(PackMeshes / TEXT("SM_Arrow_01"));
		USkeletalMesh* StringBow = LoadAt<USkeletalMesh>(OurMeshes / TEXT("SK_Bow_01"));
		StringBow = StringBow ? StringBow : LoadAt<USkeletalMesh>(PackMeshes / TEXT("SK_Bow_01"));
		// The string bow is skinned to the pack's demo mannequin (its skeleton, physics and post-process): a skeleton of
		// its own instead (its bones, from the mesh), so it doesn't bring the whole demo character with it.
		if (StringBow && StringBow->GetPathName().StartsWith(TEXT("/Game/Vaesen/")) && StringBow->GetSkeleton()
			&& !StringBow->GetSkeleton()->GetPathName().StartsWith(OurMeshes))
		{
			USkeleton* Own = NewAsset<USkeleton>(OurMeshes / TEXT("SKEL_Bow_01"));
			Own->MergeAllBonesToBoneTree(StringBow);
			StringBow->SetSkeleton(Own);
			StringBow->SetPhysicsAsset(nullptr);
			StringBow->SetPostProcessAnimBlueprint(nullptr);
			StringBow->SetShadowPhysicsAsset(nullptr);
			StringBow->PostEditChange();
			Save(Own);
			Save(StringBow);
			UE_LOG(LogVaesenAnimSetup, Display, TEXT("The string bow on its own skeleton (%d bones)"), Own->GetReferenceSkeleton().GetNum());
		}
		if (BowMesh)
		{
			SetMeshSocket(BowMesh, TEXT("Grip"), FTransform::Identity, false);
			SetMeshSocket(BowMesh, TEXT("Muzzle"), FTransform(FRotator(-90.0, 0.0, 0.0), FVector(0.0, 4.0, -2.0)), false);
		}
		// Carried on the back, held still (it doesn't hang and swing); without the socket, on the upper back.
		FVaesenSheathLocation& Back = Defaults->SheathLocations.FindOrAdd(TEXT("Sheath_BowBack"));
		Back.FallbackBone = TEXT("spine_05");
		Back.HangAngle = 0.0f;
		Save(Defaults);
		// The arrow in flight: the pack's (along its Y from the nock), turned along the projectile's X, its point at the front.
		UBlueprint* ArrowItem = LoadAt<UBlueprint>(TEXT("/Game/Vaesen/Blueprints/Items/Ammo/BP_Ammo_Arrow"));
		UVaesenAmmoItem* Arrows = ArrowItem ? Cast<UVaesenAmmoItem>(ArrowItem->GeneratedClass->GetDefaultObject()) : nullptr;
		if (Arrows && ArrowMesh)
		{
			const double Length = ArrowMesh->GetBoundingBox().Max.Y;
			Arrows->ProjectileMesh = ArrowMesh;
			Arrows->ProjectileScale = FVector::OneVector;
			Arrows->ProjectileMeshOffset = FTransform(FRotator(0.0, -90.0, 0.0), FVector(-Length, 0.0, 0.0));
			// Stuck (it becomes a pickup) and on the ground, the same arrow.
			Arrows->PickupMesh = ArrowMesh;
			Arrows->PickupScale = FVector::OneVector;
			// A faint streak behind it in flight, from its nock back (the arrow's length behind the point).
			Arrows->StreakMaterial = MakeStreakMaterial();
			Arrows->StreakLength = 250.0f;
			Arrows->StreakWidth = 1.0f;
			Arrows->StreakBehind = Length;
			FBlueprintEditorUtils::MarkBlueprintAsModified(ArrowItem);
			FKismetEditorUtilities::CompileBlueprint(ArrowItem);
			Save(ArrowItem);
		}

		UBlueprint* Weapon = LoadAt<UBlueprint>(TEXT("/Game/Vaesen/Blueprints/Items/Weapons/BP_Weapon_HuntingBow"));
		UVaesenRangedWeaponItem* Item = Weapon ? Cast<UVaesenRangedWeaponItem>(Weapon->GeneratedClass->GetDefaultObject()) : nullptr;
		if (Item && BowMesh)
		{
			Item->WeaponMesh = BowMesh;
			Item->PickupMesh = BowMesh;
			Item->Hold.InHand = FTransform::Identity;
			Item->SheathSocket = TEXT("Sheath_BowBack");
			Item->SheathOffset = FTransform::Identity;
			Item->DrawMontage = FromBack;
			Item->SheathMontage = ToBack;
			Item->NockedArrowMesh = ArrowMesh;
			// A missile, not a bullet: slow enough to follow (with its streak), falling in an arc (the arrow turns along
			// it). 45 m/s at full draw: some 0.45 s and a 1 m drop over 20 m; a click shot (half draw) leaves at 34 m/s.
			Item->LaunchSpeed = 4500.0f;
			Item->StringBowMesh = StringBow;
			FBlueprintEditorUtils::MarkBlueprintAsModified(Weapon);
			FKismetEditorUtilities::CompileBlueprint(Weapon);
			Save(Weapon);
		}
	}
}

UVaesenAnimSetupCommandlet::UVaesenAnimSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVaesenAnimSetupCommandlet::Main(const FString& Params)
{
	if (!VaesenAnimSetup::RetargetRiflePack())
	{
		return 0;
	}
	if (Params.Contains(TEXT("-analyzebow")))
	{
		VaesenAnimSetup::AnalyzeBow();
		return 0;
	}
	FString AnalyzeFolderPath;
	if (FParse::Value(*Params, TEXT("-analyzefolder="), AnalyzeFolderPath))
	{
		VaesenAnimSetup::AnalyzeFolder(AnalyzeFolderPath);
		return 0;
	}
	if (Params.Contains(TEXT("-analyzepistolarm")))
	{
		VaesenAnimSetup::AnalyzePistolArm();
		return 0;
	}
	if (Params.Contains(TEXT("-analyzepistol")))
	{
		VaesenAnimSetup::AnalyzePistol();
		return 0;
	}
	if (Params.Contains(TEXT("-analyzesword")))
	{
		VaesenAnimSetup::AnalyzeSword();
		return 0;
	}
	if (Params.Contains(TEXT("-analyze")))
	{
		VaesenAnimSetup::AnalyzeRifle();
		return 0;
	}
	VaesenAnimSetup::BuildRifle();
	VaesenAnimSetup::BuildSword();
	VaesenAnimSetup::BuildPistol();
	VaesenAnimSetup::BuildBow();
	return 0;
}
