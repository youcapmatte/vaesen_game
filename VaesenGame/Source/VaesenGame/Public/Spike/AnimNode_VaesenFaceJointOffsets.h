#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "AnimNode_VaesenFaceJointOffsets.generated.h"

class USkeletalMeshComponent;

/** One face joint moved away from where the base face's rig puts it. */
USTRUCT(BlueprintType)
struct VAESENGAME_API FVaesenFaceJointOffset
{
	GENERATED_BODY()

	/** The face joint's name, as in the DNA (e.g. "FACIAL_C_Jaw"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	FName Joint;

	/** In the joint's parent space, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face")
	FVector Offset = FVector::ZeroVector;
};

/**
 * Phase 0 spike (P0-4): moves face joints per character, after the face rig (RigLogic) has posed them.
 *
 * The face rig reads its DNA from the head's skeletal mesh asset, which every character with the same base face
 * shares, so a character's sculpt sliders can't change the DNA. Instead the sliders' joint changes are added here,
 * per animation instance: put this node after the RigLogic node in the face's animation Blueprint and feed it the
 * character's offsets. The slider morph targets are made with these joint moves already taken out, so the skin isn't
 * moved twice.
 */
USTRUCT(BlueprintInternalUseOnly)
struct VAESENGAME_API FAnimNode_VaesenFaceJointOffsets : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Links")
	FPoseLink Source;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face", meta = (PinShownByDefault))
	TArray<FVaesenFaceJointOffset> Offsets;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

	/**
	 * Offsets for one face mesh component, used instead of the Offsets pin while set (empty clears). Lets game code
	 * (e.g. the appearance component, or the spike's face driver) feed a face without wiring its animation Blueprint.
	 * Thread-safe; animation reads it on worker threads.
	 */
	static void SetOffsetsFor(const USkeletalMeshComponent* Face, TArray<FVaesenFaceJointOffset> InOffsets);

	/**
	 * Minimum values for curves on one face (P0-6, age): after the face rig, each curve is raised to at least its floor.
	 * The face's wrinkle maps are driven by such curves (e.g. "head_wm1_normal_head_wm1_browsRaiseInner_L"), so a floor
	 * gives an older face permanent light wrinkles that still deepen with expressions. Empty clears.
	 */
	static void SetCurveFloorsFor(const USkeletalMeshComponent* Face, TMap<FName, float> InFloors);

private:
	static bool GetOffsetsFor(const USkeletalMeshComponent* Face, TArray<FVaesenFaceJointOffset>& Out);
	static bool GetCurveFloorsFor(const USkeletalMeshComponent* Face, TMap<FName, float>& Out);

	/** What this evaluation uses: the registered offsets for this face, or the pin's. */
	TArray<FVaesenFaceJointOffset> ActiveOffsets;
	TMap<FName, float> ActiveCurveFloors;

	/** Offsets' joints resolved to bones, rebuilt when the joints or the required bones change. */
	TArray<FBoneReference> ResolvedJoints;
	uint16 ResolvedBonesSerial = 0;

	void ResolveJoints(const FBoneContainer& Bones);
};
