#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Weapons/VaesenWeaponTypes.h"
#include "VaesenAnimInstance.generated.h"

class UMeshComponent;
class USceneComponent;

/** A weapon in the right hand, for the shoulder aim: its grip (the weapon's origin), its mesh (sockets), its hold. */
struct FVaesenHeldWeapon
{
	const USceneComponent* Grip = nullptr;
	const UMeshComponent* Mesh = nullptr;
	const FVaesenWeaponHold* Hold = nullptr;

	bool IsValid() const { return Grip && Mesh && Hold; }

	/** A socket of the mesh, relative to the grip; false (and Fallback) without it. */
	bool GetLocalSocket(FName Socket, FVector& OutLocation, const FVector& Fallback = FVector::ZeroVector) const;
};

/**
 * Parent class for the character's anim Blueprint (reparent ABP_Manny to it). It works out every frame what the
 * graph needs for weapons, so the Blueprint only reads variables:
 *   - weapon state: drawn, category, aiming, blocking, reloading, drawing a bow (draw alpha);
 *   - the aim offset (AimPitch, AimYaw) while aiming;
 *   - hand IK: the off hand onto a two-handed weapon's LeftHandIK socket, and the drawing hand onto the sheathed
 *     weapon's Grip while the montage's HandIK_Sheath curve is up (so one draw montage serves every weapon there);
 *   - TriggerCurl, pulsed on each shot (or the montage's TriggerFinger curve) for the index finger;
 *   - for motion matching later: LocomotionDatabase, the drawn category's pose search database
 *     (DA_WeaponCategoryDefaults), plus speed and direction.
 * It also poses the Weapon Fit rig (AVaesenWeaponFitRig) in the editor, from the rig's settings instead of a character.
 * Upper-body actions are montages in an UpperBody slot, layered per bone from the spine, so the legs keep walking.
 */
UCLASS()
class VAESENGAME_API UVaesenAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** A shot: the trigger finger curls and lets go (and a shouldered weapon kicks). */
	void PulseTriggerFinger() { TriggerPulse = 1.0f; RecoilKick = 1.0f; }

	/**
	 * Where a shouldered weapon goes (its grip, world): laid along Aim, its Sight socket in front of the head by the
	 * hold's AimSightOffset, slid so its Stock meets the shoulder; Kick (0..1) is a shot's recoil.
	 */
	static FTransform PlaceShouldered(const USkeletalMeshComponent* Body, const FVaesenHeldWeapon& Weapon, const FRotator& Aim, float Kick = 0.0f);

	/** The reverse: the hold's aim values (sight offset, stock, rotation) that put the weapon at Placed. */
	static void ShoulderHoldFromPlacement(const USkeletalMeshComponent* Body, const FVaesenHeldWeapon& Weapon, const FRotator& Aim,
		const FTransform& Placed, FVaesenWeaponHold& InOutHold);

	/** Curve names read from montages. */
	static const FName HandIKSheathCurve;
	static const FName TriggerFingerCurve;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bWeaponDrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	EVaesenWeaponCategory WeaponCategory = EVaesenWeaponCategory::Unarmed;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bTwoHanded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bBlocking = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bStaggered = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bCrouching = false;

	/** In the air (jumping, falling). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bFalling = false;

	/** Bows: 0 = slack, 1 = full draw. */
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	float BowDrawAlpha = 0.0f;

	/** Aim offset, degrees: up/down and left/right of where the body faces. */
	UPROPERTY(BlueprintReadOnly, Category = "Aim")
	float AimPitch = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim")
	float AimYaw = 0.0f;

	/** Off hand onto the weapon (world space), weight 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform LeftHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float LeftHandIKAlpha = 0.0f;

	/** LeftHandIKTransform's location (what a Two Bone IK node takes). */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector LeftHandIKLocation = FVector::ZeroVector;

	/** The drawing hand onto the sheathed weapon's grip (world space), weight from the HandIK_Sheath curve. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FTransform SheathHandIKTransform;

	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float SheathHandIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector SheathHandIKLocation = FVector::ZeroVector;

	/** The off hand's target relative to hand_r (exact every frame: the weapon is fixed to that hand). */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector LeftHandIKInHand = FVector::ZeroVector;

	/** Shoulder aim: where the right hand goes (world space) to hold the weapon at the shoulder, weight 0..1. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float AimIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FVector AimHandLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "IK")
	FRotator AimHandRotation = FRotator::ZeroRotator;

	/** Sheath IK drives the left hand (an off-hand shield) instead of the right. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	bool bSheathIKLeftHand = false;

	/** 0..1 curl of the index finger. */
	UPROPERTY(BlueprintReadOnly, Category = "IK")
	float TriggerCurl = 0.0f;

	// --- Locomotion (also the motion matching query inputs) ---

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.0f;

	/** Movement direction relative to where the body faces, -180..180. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bStrafing = false;

	/** The drawn category's locomotion database for motion matching (null = the default locomotion). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	TObjectPtr<UObject> LocomotionDatabase;

private:
	float RecoilKick = 0.0f;

	FTransform AimHandInComponent;

	float TriggerPulse = 0.0f;

	/** Weapon whose sheath grip the drawing hand reaches for (the draw / sheath in progress). */
	TWeakObjectPtr<class AVaesenWeaponActor> SheathTarget;

	/** Where its grip was when the reach began, relative to the character (the weapon moves to the hand at the grab). */
	FTransform SheathTargetInComponent;
};
