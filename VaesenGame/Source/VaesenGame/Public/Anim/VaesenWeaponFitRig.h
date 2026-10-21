#pragma once

#include "CoreMinimal.h"
#include "Anim/VaesenAnimInstance.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "VaesenWeaponFitRig.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMesh;
class UVaesenWeaponItem;

UENUM()
enum class EVaesenFitPose : uint8
{
	Hip,
	Aim,
	Crouch,
	CrouchAim
};

/** A part of the Weapon Fit rig the gizmo can move: tells the rig when it's moved. */
UCLASS(ClassGroup = "Vaesen")
class VAESENGAME_API UVaesenFitHandleComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif
};

/**
 * Weapon Fit rig: tunes how a weapon sits in the hands, in the editor, without playing (L_WeaponFit has one).
 *
 * Pick a Weapon and a Pose (or a Preview Animation and a time). The character shows it through the real anim
 * Blueprint, hands on the weapon as in the game. Then click the weapon and move or rotate it with the gizmo:
 *   - in the hip and crouch poses that changes how it sits in the hand (Hold > In Hand);
 *   - aimed, where it sits against the shoulder (Hold > Aim Sight Offset, Stock To Shoulder, Aim Rotation);
 *   - the small sphere is the off hand: move it to move the left hand along the weapon (Hold > Left Hand Offset).
 * Every change goes straight into the weapon's Blueprint (its Hold); Save Weapon (or Save All) keeps it.
 */
UCLASS(HideCategories = (Rendering, Physics, Collision, Input, HLOD, Replication, Networking, LOD, Cooking, DataLayers, WorldPartition, LevelInstance))
class VAESENGAME_API AVaesenWeaponFitRig : public AActor
{
	GENERATED_BODY()

public:
	AVaesenWeaponFitRig();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** The weapon item's defaults (where the Hold is kept), or null. */
	UVaesenWeaponItem* GetWeaponDefaults() const;

	FVaesenHeldWeapon GetHeldWeapon() const;

	/** Where the rig aims: straight ahead of the actor, pitched by AimPitch. */
	FRotator GetAim() const { return FRotator(AimPitch, GetActorRotation().Yaw, 0.0); }

	bool IsAimPose() const { return Pose == EVaesenFitPose::Aim || Pose == EVaesenFitPose::CrouchAim; }
	bool IsCrouchPose() const { return Pose == EVaesenFitPose::Crouch || Pose == EVaesenFitPose::CrouchAim; }

	USkeletalMeshComponent* GetBody() const { return Body; }
	USceneComponent* GetGrip() const { return Grip; }
	UVaesenFitHandleComponent* GetWeaponMeshHandle() const { return WeaponMesh; }
	UVaesenFitHandleComponent* GetLeftHandHandle() const { return LeftHandHandle; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;

	/** The gizmo moved the weapon or the off-hand handle: work out the Hold that puts it there. */
	void OnHandleMoved(UVaesenFitHandleComponent* Handle, bool bFinished);
#endif

	/** The weapon item Blueprint to fit. */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit")
	TSubclassOf<UVaesenWeaponItem> Weapon;

	UPROPERTY(EditAnywhere, Category = "Weapon Fit")
	EVaesenFitPose Pose = EVaesenFitPose::Aim;

	/** Looking up (+) or down (-). */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit", meta = (UIMin = "-60", UIMax = "60", Units = "Degrees"))
	float AimPitch = 0.0f;

	/** Walking: 0 stands, 120 walks, 240 jogs, 480 runs (the pose then moves). */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit", meta = (UIMin = "0", UIMax = "480"))
	float Speed = 0.0f;

	/** Which way it walks, relative to where it faces. */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit", meta = (UIMin = "-180", UIMax = "180", Units = "Degrees"))
	float Direction = 0.0f;

	/** Any animation of the skeleton on the upper body (a reload, the fire, the draw), held at Preview Time. */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit|Animation")
	TObjectPtr<UAnimSequenceBase> PreviewAnimation;

	UPROPERTY(EditAnywhere, Category = "Weapon Fit|Animation", meta = (UIMin = "0", Units = "Seconds"))
	float PreviewTime = 0.0f;

	/** Play it instead of holding it at Preview Time. */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit|Animation")
	bool bPlayPreview = false;

	/** Saves the weapon's Blueprint (with its Hold). */
	UFUNCTION(CallInEditor, Category = "Weapon Fit")
	void SaveWeapon();

	/** Puts the weapon's Hold back to the defaults. */
	UFUNCTION(CallInEditor, Category = "Weapon Fit")
	void ResetHold();

	/** The body (Manny) and its anim Blueprint. */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit|Setup")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditAnywhere, Category = "Weapon Fit|Setup")
	TSoftClassPtr<UAnimInstance> AnimClass;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** The weapon's origin, in the hand socket like the weapon actor's. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<USceneComponent> Grip;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> WeaponMesh;

	/** Where the off hand holds the weapon. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> LeftHandHandle;

private:
	/** Puts the weapon's mesh on, the body's anim class, the weapon category's locomotion layer. */
	void ApplySetup();

	/** Keeps the grip and the off-hand handle where the Hold says, and the preview animation where it should be. */
	void ApplyPose();

	/** The weapon mesh relative to the grip (its Grip socket at the grip). */
	FTransform MeshInGrip;

	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedLayer;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayingAnimation;
};
