#include "Spike/AnimNode_VaesenFaceJointOffsets.h"

#include "Animation/AnimInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/ScopeRWLock.h"

namespace VaesenFaceJointOffsets
{
	FRWLock Lock;
	TMap<TObjectKey<USkeletalMeshComponent>, TArray<FVaesenFaceJointOffset>> Registered;
	TMap<TObjectKey<USkeletalMeshComponent>, TMap<FName, float>> RegisteredFloors;
}

void FAnimNode_VaesenFaceJointOffsets::SetCurveFloorsFor(const USkeletalMeshComponent* Face, TMap<FName, float> InFloors)
{
	if (!Face)
	{
		return;
	}
	FWriteScopeLock Scope(VaesenFaceJointOffsets::Lock);
	if (InFloors.IsEmpty())
	{
		VaesenFaceJointOffsets::RegisteredFloors.Remove(Face);
	}
	else
	{
		VaesenFaceJointOffsets::RegisteredFloors.Add(Face, MoveTemp(InFloors));
	}
}

bool FAnimNode_VaesenFaceJointOffsets::GetCurveFloorsFor(const USkeletalMeshComponent* Face, TMap<FName, float>& Out)
{
	FReadScopeLock Scope(VaesenFaceJointOffsets::Lock);
	if (const TMap<FName, float>* Found = Face ? VaesenFaceJointOffsets::RegisteredFloors.Find(Face) : nullptr)
	{
		Out = *Found;
		return true;
	}
	Out.Reset();
	return false;
}

void FAnimNode_VaesenFaceJointOffsets::SetOffsetsFor(const USkeletalMeshComponent* Face, TArray<FVaesenFaceJointOffset> InOffsets)
{
	if (!Face)
	{
		return;
	}
	FWriteScopeLock Scope(VaesenFaceJointOffsets::Lock);
	if (InOffsets.IsEmpty())
	{
		VaesenFaceJointOffsets::Registered.Remove(Face);
	}
	else
	{
		VaesenFaceJointOffsets::Registered.Add(Face, MoveTemp(InOffsets));
	}
}

bool FAnimNode_VaesenFaceJointOffsets::GetOffsetsFor(const USkeletalMeshComponent* Face, TArray<FVaesenFaceJointOffset>& Out)
{
	FReadScopeLock Scope(VaesenFaceJointOffsets::Lock);
	if (const TArray<FVaesenFaceJointOffset>* Found = Face ? VaesenFaceJointOffsets::Registered.Find(Face) : nullptr)
	{
		Out = *Found;
		return true;
	}
	return false;
}

void FAnimNode_VaesenFaceJointOffsets::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_VaesenFaceJointOffsets::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	ResolvedJoints.Reset(); // Re-resolved against the new required bones on the next evaluation.
}

void FAnimNode_VaesenFaceJointOffsets::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);
	Source.Update(Context);
	if (!GetOffsetsFor(Context.AnimInstanceProxy->GetSkelMeshComponent(), ActiveOffsets))
	{
		ActiveOffsets = Offsets;
	}
	GetCurveFloorsFor(Context.AnimInstanceProxy->GetSkelMeshComponent(), ActiveCurveFloors);
}

void FAnimNode_VaesenFaceJointOffsets::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	// Offsets can change at runtime (another character, the creator's sliders), and the required bones with the LOD.
	if (ResolvedJoints.Num() != ActiveOffsets.Num() || ResolvedBonesSerial != Bones.GetSerialNumber())
	{
		ResolveJoints(Bones);
	}
	for (int32 I = 0; I < ActiveOffsets.Num(); ++I)
	{
		if (ResolvedJoints[I].BoneName != ActiveOffsets[I].Joint)
		{
			ResolvedJoints[I] = FBoneReference(ActiveOffsets[I].Joint);
			ResolvedJoints[I].Initialize(Bones);
		}
		// Joints missing at this level of detail (the face rig drops joints at lower LODs) are skipped.
		const FCompactPoseBoneIndex Index = ResolvedJoints[I].GetCompactPoseIndex(Bones);
		if (Index != INDEX_NONE)
		{
			Output.Pose[Index].AddToTranslation(ActiveOffsets[I].Offset);
		}
	}

	for (const TPair<FName, float>& Floor : ActiveCurveFloors)
	{
		const float Current = Output.Curve.Get(Floor.Key);
		if (Current < Floor.Value)
		{
			Output.Curve.Set(Floor.Key, Floor.Value);
		}
	}
}

void FAnimNode_VaesenFaceJointOffsets::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("Vaesen Face Joint Offsets (%d joints)"), ActiveOffsets.Num()));
	Source.GatherDebugData(DebugData);
}

void FAnimNode_VaesenFaceJointOffsets::ResolveJoints(const FBoneContainer& Bones)
{
	ResolvedJoints.Reset(ActiveOffsets.Num());
	for (const FVaesenFaceJointOffset& Entry : ActiveOffsets)
	{
		FBoneReference& Joint = ResolvedJoints.Emplace_GetRef(Entry.Joint);
		Joint.Initialize(Bones);
	}
	ResolvedBonesSerial = Bones.GetSerialNumber();
}
