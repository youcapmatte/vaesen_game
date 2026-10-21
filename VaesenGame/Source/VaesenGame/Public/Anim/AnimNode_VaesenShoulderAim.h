#pragma once

#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "AnimNode_VaesenShoulderAim.generated.h"

/**
 * What the shoulder aim needs from the anim instance each frame (UVaesenAnimInstance fills it on the game thread).
 * Weapon sockets are relative to its grip (the weapon's origin); everything else in component space.
 */
struct FVaesenShoulderAimInput
{
	/** Where it aims, in component space (no roll). */
	FQuat Aim = FQuat::Identity;

	/** The weapon's grip relative to hand_r (rigid: the weapon is in that hand). */
	FTransform WeaponInHand;

	FVector SightLocation = FVector(0.0, 0.0, 6.0);
	FQuat SightRotation = FQuat::Identity;
	FVector StockLocation = FVector::ZeroVector;
	bool bHasStock = false;

	/** The left hand bone in the weapon's space (its palm on LeftGrip); valid if bLeftHand. */
	FTransform LeftHandInWeapon;
	bool bLeftHand = false;

	/** The shoulder pocket on clavicle_r (the skeleton's ShoulderPocket socket); without it, next to upperarm_r. */
	FTransform PocketOnClavicle;
	bool bHasPocketSocket = false;

	/** The right eye on the head bone (the skeleton's Eye_R socket); without it, roughly there. */
	FTransform EyeOnHead;
	bool bHasEyeSocket = false;

	/** From the weapon's Hold. */
	FVector EyeOffset = FVector::ZeroVector;
	FVector ShoulderOffset = FVector::ZeroVector;
	FQuat AimRotation = FQuat::Identity;
	float ShoulderShare = 0.4f;
	float RecoilBack = 0.0f;
	float RecoilPitch = 0.0f;

	/** A shot's kick, 1 easing to 0. */
	float Kick = 0.0f;
};

/**
 * Vaesen Shoulder Aim: shoulders a long gun the way a shooter does, on this frame's pose.
 *   - The weapon lies along the aim (its Sight socket's line).
 *   - The gap between the eye line and the shoulder pocket is shared: the right shoulder lifts (clavicle_r) by
 *     ShoulderShare of it and the head bows and leans onto the stock (neck_01) by the rest.
 *   - The butt (Stock socket) goes on the lifted shoulder pocket; the sight line is then at the eye.
 *   - Both arms are solved onto the weapon: the right hand holds its grip, the left hand its LeftGrip, turned to it.
 * A weapon without a Stock puts its sight in front of the eye and leaves the body alone. The inputs come from
 * UVaesenAnimInstance (PreUpdate); Alpha blends it in and out.
 */
USTRUCT(BlueprintInternalUseOnly)
struct VAESENGAME_API FAnimNode_VaesenShoulderAim : public FAnimNode_SkeletalControlBase
{
	GENERATED_BODY()

	/** At most this much shoulder lift and head bow (degrees). */
	UPROPERTY(EditAnywhere, Category = "Shoulder Aim")
	float MaxShoulderLift = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Shoulder Aim")
	float MaxHeadBow = 30.0f;

	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms) override;
	virtual bool IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) override;

private:
	virtual void InitializeBoneReferences(const FBoneContainer& RequiredBones) override;

	FVaesenShoulderAimInput Input;
	bool bActive = false;

	FBoneReference Neck = FBoneReference(TEXT("neck_01"));
	FBoneReference Head = FBoneReference(TEXT("head"));
	FBoneReference ClavicleR = FBoneReference(TEXT("clavicle_r"));
	FBoneReference UpperArmR = FBoneReference(TEXT("upperarm_r"));
	FBoneReference LowerArmR = FBoneReference(TEXT("lowerarm_r"));
	FBoneReference HandR = FBoneReference(TEXT("hand_r"));
	FBoneReference UpperArmL = FBoneReference(TEXT("upperarm_l"));
	FBoneReference LowerArmL = FBoneReference(TEXT("lowerarm_l"));
	FBoneReference HandL = FBoneReference(TEXT("hand_l"));
};
