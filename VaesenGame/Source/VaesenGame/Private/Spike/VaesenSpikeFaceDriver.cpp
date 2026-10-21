#include "Spike/VaesenSpikeFaceDriver.h"

#include "Components/LODSyncComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AVaesenSpikeFaceDriver::AVaesenSpikeFaceDriver()
{
	PrimaryActorTick.bCanEverTick = true;
	// Before the faces animate, so this frame's morphs and offsets are used.
	PrimaryActorTick.TickGroup = TG_PrePhysics;
}

USkeletalMeshComponent* AVaesenSpikeFaceDriver::FindFace(AActor* Target)
{
	return FindPart(Target, TEXT("Face"));
}

USkeletalMeshComponent* AVaesenSpikeFaceDriver::FindPart(AActor* Target, const TCHAR* Name)
{
	if (!Target)
	{
		return nullptr;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Target->GetComponents(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetName() == Name)
		{
			return Mesh;
		}
	}
	return nullptr;
}

void AVaesenSpikeFaceDriver::BeginPlay()
{
	Super::BeginPlay();
	LoadedOffsets.SetNum(Faces.Num());
	for (int32 I = 0; I < Faces.Num(); ++I)
	{
		// Keep the test faces animating even when not rendered (an editor in the background, or off-screen), so
		// scripted checks see the same poses as a person watching.
		USkeletalMeshComponent* Face = bForceFullDetail ? FindFace(Faces[I].Target) : nullptr;
		if (Face)
		{
			Face->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			// Full detail: the slider joints exist only at the top face LODs (lower LODs drop most face joints). The
			// MetaHuman's LOD Sync component decides every part's LOD, so force it there.
			if (ULODSyncComponent* LODSync = Faces[I].Target->FindComponentByClass<ULODSyncComponent>())
			{
				LODSync->ForcedLOD = 0;
			}
			Face->SetForcedLOD(1);
		}

		FString Text;
		if (Faces[I].JointOffsetsFile.IsEmpty() || !FFileHelper::LoadFileToString(Text, *Faces[I].JointOffsetsFile))
		{
			continue;
		}
		TSharedPtr<FJsonObject> Json;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) || !Json)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Spike] Couldn't read joint offsets %s"), *Faces[I].JointOffsetsFile);
			continue;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Json->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Pair.Value->AsArray();
			if (V.Num() == 3)
			{
				LoadedOffsets[I].Add({ FName(*Pair.Key), FVector(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber()) });
			}
		}
		UE_LOG(LogTemp, Display, TEXT("[Spike] %s: %d joint offsets from %s"), *GetNameSafe(Faces[I].Target), LoadedOffsets[I].Num(), *Faces[I].JointOffsetsFile);
	}
}

void AVaesenSpikeFaceDriver::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const FVaesenSpikeFaceSlider& Face : Faces)
	{
		FAnimNode_VaesenFaceJointOffsets::SetOffsetsFor(FindFace(Face.Target), {});
		FAnimNode_VaesenFaceJointOffsets::SetCurveFloorsFor(FindFace(Face.Target), {});
	}
	Super::EndPlay(Reason);
}

void AVaesenSpikeFaceDriver::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The MetaHuman component loads its animation Blueprints asynchronously and, when they arrive (seconds after
	// BeginPlay), sets the face's and body's animation class again, which drops a single animation. So check every
	// frame and (re)start ours wherever it isn't running; after that one reset it keeps playing.
	auto KeepPlaying = [](USkeletalMeshComponent* Mesh, UAnimationAsset* Animation)
	{
		const UAnimSingleNodeInstance* Single = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
		if (Mesh && Animation && (Mesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode || !Single || Single->GetAnimationAsset() != Animation))
		{
			Mesh->PlayAnimation(Animation, true);
			// The MetaHuman Blueprint holds its body still with a play rate of 0.
			Mesh->SetPlayRate(1.f);
		}
	};
	for (const FVaesenSpikeFaceSlider& Entry : Faces)
	{
		KeepPlaying(FaceAnimation ? FindFace(Entry.Target) : nullptr, FaceAnimation);
		KeepPlaying(BodyAnimation ? FindPart(Entry.Target, TEXT("Body")) : nullptr, BodyAnimation);
	}
	bAnimationStarted = true;
	// Once, 3 s in: where each face's most-offset joint is, relative to its actor, so a run without the editor (a
	// packaged build) shows in its log that the offsets reach the pose. Faces with the same base and animation differ
	// there only by their offsets.
	if (!bJointsLogged && GetWorld()->GetTimeSeconds() > 3.0)
	{
		bJointsLogged = true;
		FName Probe;
		double Largest = -1.0;
		for (const TArray<FVaesenFaceJointOffset>& List : LoadedOffsets)
		{
			for (const FVaesenFaceJointOffset& Offset : List)
			{
				if (Offset.Offset.Size() > Largest)
				{
					Largest = Offset.Offset.Size();
					Probe = Offset.Joint;
				}
			}
		}
		for (int32 I = 0; I < Faces.Num() && !Probe.IsNone(); ++I)
		{
			if (USkeletalMeshComponent* Face = FindFace(Faces[I].Target))
			{
				const FVector Local = Faces[I].Target->GetActorTransform().InverseTransformPosition(Face->GetSocketLocation(Probe));
				UE_LOG(LogTemp, Display, TEXT("[P0-4] %s (%s, weight %.2f): %s at (%.3f, %.3f, %.3f), LOD %d"), *GetNameSafe(Faces[I].Target),
					Faces[I].Morph.IsNone() ? TEXT("no slider") : *Faces[I].Morph.ToString(), Faces[I].Weight, *Probe.ToString(), Local.X, Local.Y, Local.Z, Face->GetPredictedLODLevel());
			}
		}
	}
	const float Sweep = SweepSeconds > 0.f ? FMath::Sin(GetWorld()->GetTimeSeconds() * UE_TWO_PI / SweepSeconds) : 1.f;
	for (int32 I = 0; I < Faces.Num(); ++I)
	{
		USkeletalMeshComponent* Face = FindFace(Faces[I].Target);
		if (!Face)
		{
			continue;
		}
		const float Weight = SweepSeconds > 0.f ? Sweep : Faces[I].Weight;
		if (!Faces[I].Morph.IsNone())
		{
			Face->SetMorphTarget(Faces[I].Morph, Weight);
		}
		if (Faces[I].WrinkleAge > 0.f)
		{
			// The wrinkle maps that come with age: forehead lines, frown lines, crow's feet, nasolabial folds, lip and
			// neck lines. At age 1 they sit at 60% of their full (expression) strength.
			static const TCHAR* const AgeCurves[] = {
				TEXT("head_wm1_normal_head_wm1_browsRaiseInner_L"), TEXT("head_wm1_normal_head_wm1_browsRaiseInner_R"),
				TEXT("head_wm1_normal_head_wm1_browsRaiseOuter_L"), TEXT("head_wm1_normal_head_wm1_browsRaiseOuter_R"),
				TEXT("head_wm2_normal_head_wm2_browsDown_L"), TEXT("head_wm2_normal_head_wm2_browsDown_R"),
				TEXT("head_wm1_normal_head_wm1_squintInner_L"), TEXT("head_wm1_normal_head_wm1_squintInner_R"),
				TEXT("head_wm3_normal_head_wm3_smile_L"), TEXT("head_wm3_normal_head_wm3_smile_R"),
				TEXT("head_wm1_normal_head_wm1_purse_UL"), TEXT("head_wm1_normal_head_wm1_purse_UR"),
				TEXT("head_wm1_normal_head_wm1_purse_DL"), TEXT("head_wm1_normal_head_wm1_purse_DR"),
				TEXT("head_wm2_normal_head_wm2_neckStretch_L"), TEXT("head_wm2_normal_head_wm2_neckStretch_R"),
			};
			TMap<FName, float> Floors;
			for (const TCHAR* Curve : AgeCurves)
			{
				Floors.Add(FName(Curve), 0.6f * Faces[I].WrinkleAge);
			}
			FAnimNode_VaesenFaceJointOffsets::SetCurveFloorsFor(Face, MoveTemp(Floors));
		}
		if (LoadedOffsets.IsValidIndex(I) && !LoadedOffsets[I].IsEmpty())
		{
			TArray<FVaesenFaceJointOffset> Scaled = LoadedOffsets[I];
			for (FVaesenFaceJointOffset& Offset : Scaled)
			{
				Offset.Offset *= Weight;
			}
			FAnimNode_VaesenFaceJointOffsets::SetOffsetsFor(Face, MoveTemp(Scaled));
		}
	}
}
