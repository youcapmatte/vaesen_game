#include "VaesenAnimSetup.h"

#include "AnimPose.h"
#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
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
#include "Characters/VaesenPlayerCharacter.h"
#include "EdGraphSchema_K2.h"
#include "Factories/AnimBlueprintFactory.h"
#include "Factories/AnimMontageFactory.h"
#include "Inventory/VaesenEquipmentComponent.h"
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
	const TCHAR* UE4MannequinMesh = TEXT("/Game/RifleAnims/ShowcaseAssets/Meshes/Mannequin/SK_Mannequin.SK_Mannequin");
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
		if (const UStaticMesh* Rifle = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/RifleAnims/ShowcaseAssets/Meshes/Rifle/SM_Rifle.SM_Rifle")))
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
	if (Params.Contains(TEXT("-analyze")))
	{
		VaesenAnimSetup::AnalyzeRifle();
		return 0;
	}
	VaesenAnimSetup::BuildRifle();
	return 0;
}
