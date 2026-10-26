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

	/** How the right palm holds a grip: the skeleton's hand_r_weapon socket, on hand_r. */
	FTransform PalmOnHand;

	/** The weapon's grip on the weapon bone (ik_hand_gun): its Hold.InHand. */
	FTransform GripOnWeaponBone;

	/** A weapon is in the right hand (the weapon bone follows it, or holds it while shouldered). */
	bool bHeld = false;

	/** Shouldered, 0..1 (blends in and out). */
	float ShoulderBlend = 0.0f;
	bool bShoulder = false;

	/** Working the bolt: how far through it (0..1), and the bolt knob on the weapon (grip space). */
	bool bCycling = false;
	float CyclePhase = 0.0f;
	FVector BoltLocation = FVector::ZeroVector;

	FVector SightLocation = FVector(0.0, 0.0, 6.0);
	FQuat SightRotation = FQuat::Identity;
	FVector StockLocation = FVector::ZeroVector;
	bool bHasStock = false;

	/** The left hand bone in the weapon's space (its palm on LeftGrip); valid if bLeftHand. Without bLeftHandTurns
	 * (an older LeftHandIK socket: a place only) the hand keeps the animation's rotation. */
	FTransform LeftHandInWeapon;
	bool bLeftHand = false;
	bool bLeftHandTurns = false;

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
	float RecoilBodyPitch = 0.0f;

	/**
	 * Carrying a one-handed weapon at a run, 0..1: the wrist turns so the weapon's X (the blade) points along
	 * CarryDirection (component space), clear of the body and the other arm, whatever the arm is doing.
	 */
	float CarryBlend = 0.0f;
	FVector CarryDirection = FVector(0.0, 1.0, 0.0);

	/** A shot's kick, 1 easing to 0. */
	float Kick = 0.0f;

	/**
	 * Guarding with a long gun (no guard animation: held so here), 0..1: the weapon across the chest in both hands,
	 * GuardRotation (component space), its middle (GuardMiddleInWeapon, between the hands) GuardFromChest from the chest
	 * (spine_05). A blow on it (GuardKnock, 1 easing to 0) drives it back into the chest and rocks the upper body.
	 */
	float GuardBlend = 0.0f;
	FQuat GuardRotation = FQuat::Identity;
	FVector GuardFromChest = FVector(0.0, 30.0, -12.0);
	FVector GuardMiddleInWeapon = FVector::ZeroVector;
	float GuardKnock = 0.0f;

	/**
	 * The weapon is in the left fist (a bow: on hand_l, GripOnLeftHand from it), not on the weapon bone. Guarding, the
	 * left arm takes it across the chest and the right hand (its bone, RightHandInWeapon) holds the upper limb.
	 */
	bool bLeftHeld = false;
	FTransform GripOnLeftHand;
	FTransform RightHandInWeapon;

	/** That weapon carried in the plain stance, 0..1: the left arm holds it hanging upright at his side (the fist as its pack grips it). */
	float LeftCarryBlend = 0.0f;
};

/**
 * Vaesen Shoulder Aim: holds the weapon in the hands on this frame's pose. The weapon rides on the weapon bone
 * (ik_hand_gun), which follows the right hand's grip; while shouldered or working the bolt it holds the weapon itself.
 *
 * Shouldered, a long gun is held the way a shooter does:
 *   - The weapon lies along the aim (its Sight socket's line).
 *   - The gap between the eye line and the shoulder pocket is shared: the right shoulder lifts (clavicle_r) by
 *     ShoulderShare of it and the head bows and leans onto the stock (neck_01) by the rest.
 *   - The butt (Stock socket) goes on the lifted shoulder pocket; the sight line is then at the eye.
 *   - Both arms are solved onto the weapon: the right hand holds its grip, the left hand its LeftGrip, turned to it.
 *   - A shot rocks the upper body back (spine_05) and climbs the muzzle.
 * A weapon without a Stock puts its sight in front of the eye and leaves the body alone.
 *
 * Working a bolt (aimed or not): the weapon stays where it is, held by the left hand (and the shoulder), and the right
 * hand goes to the bolt knob, lifts, pulls back, pushes, turns down and comes back to the grip (VaesenBolt moments).
 *
 * The left hand always holds LeftGrip of a two-handed weapon. The inputs come from UVaesenAnimInstance (PreUpdate).
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

	FBoneReference Spine = FBoneReference(TEXT("spine_05"));
	FBoneReference Neck = FBoneReference(TEXT("neck_01"));
	FBoneReference Head = FBoneReference(TEXT("head"));
	FBoneReference ClavicleR = FBoneReference(TEXT("clavicle_r"));
	FBoneReference UpperArmR = FBoneReference(TEXT("upperarm_r"));
	FBoneReference LowerArmR = FBoneReference(TEXT("lowerarm_r"));
	FBoneReference HandR = FBoneReference(TEXT("hand_r"));
	FBoneReference UpperArmL = FBoneReference(TEXT("upperarm_l"));
	FBoneReference LowerArmL = FBoneReference(TEXT("lowerarm_l"));
	FBoneReference HandL = FBoneReference(TEXT("hand_l"));
	FBoneReference WeaponBone = FBoneReference(TEXT("ik_hand_gun"));
};
