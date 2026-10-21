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
	/** The weapon mesh socket this handle stands for (None: the weapon itself). */
	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	FName Socket;

#if WITH_EDITOR
	virtual void PostEditComponentMove(bool bFinished) override;
#endif
};

/**
 * Weapon Fit rig: checks and sets a weapon's sockets with the hands on it, in the editor, without playing
 * (L_WeaponFit has one).
 *
 * A weapon is held by its mesh's sockets, the same rule for every weapon (X toward the muzzle, Z up):
 *   - Grip: where the right palm closes (the skeleton's hand_r_weapon);
 *   - LeftGrip: where the left palm holds, and how it's turned (the skeleton's hand_l_weapon);
 *   - Sight: the line of the sights (its X is the line); Stock: the butt plate.
 * The hands go there by IK and the fingers close by the category's grip pose, so a weapon with its sockets right
 * needs nothing else.
 *
 * Pick a Weapon and a Pose (or a Preview Animation and a time); the character shows it through the real anim
 * Blueprint. The small shapes are the sockets: cube Grip, sphere LeftGrip, cone Sight, cylinder Stock. Move or turn
 * one with the gizmo and the hands follow. Or move the weapon itself: from the hip that moves Grip (the weapon slides
 * in the hand), aimed it moves Sight and Stock (where it lies against the shoulder). Changes go into the weapon's
 * mesh; Save Weapon keeps them.
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

	/** The weapon item's defaults, or null. */
	UVaesenWeaponItem* GetWeaponDefaults() const;

	FVaesenHeldWeapon GetHeldWeapon() const;

	/** Where the rig aims: straight ahead of the actor, pitched by AimPitch. */
	FRotator GetAim() const { return FRotator(AimPitch, GetActorRotation().Yaw, 0.0); }

	bool IsAimPose() const { return Pose == EVaesenFitPose::Aim || Pose == EVaesenFitPose::CrouchAim; }
	bool IsCrouchPose() const { return Pose == EVaesenFitPose::Crouch || Pose == EVaesenFitPose::CrouchAim; }

	USkeletalMeshComponent* GetBody() const { return Body; }
	USceneComponent* GetGrip() const { return Grip; }
	UVaesenFitHandleComponent* GetWeaponMeshHandle() const { return WeaponMesh; }

	/** The handle of a socket (Grip, LeftGrip, Sight, Stock), or null. */
	UVaesenFitHandleComponent* GetSocketHandle(FName Socket) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;

	/** The gizmo moved the weapon or a socket handle: sets the socket(s) on the weapon's mesh. */
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

	/** Show the socket handles (hide them to look at the hands). */
	UPROPERTY(EditAnywhere, Category = "Weapon Fit")
	bool bShowSockets = true;

	/** Saves the weapon's mesh (its sockets) and its Blueprint. */
	UFUNCTION(CallInEditor, Category = "Weapon Fit")
	void SaveWeapon();

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

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> GripHandle;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> LeftGripHandle;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> SightHandle;

	UPROPERTY(VisibleAnywhere, Category = "Weapon Fit")
	TObjectPtr<UVaesenFitHandleComponent> StockHandle;

private:
	UVaesenFitHandleComponent* MakeHandle(FName Name, FName Socket, const TCHAR* Shape, float Size);

	/** Puts the weapon's mesh on, the body's anim class. */
	void ApplySetup();

	/** Holds the mesh by its Grip socket (again, after Grip moved). */
	void AlignMesh();

	/** Keeps the grip, the mesh and the handles where they belong, the layer linked, the preview animation set. */
	void ApplyPose();

	/** Sets a socket of the weapon's mesh, from a transform relative to the grip. */
	void SetSocketFromGrip(FName Socket, const FTransform& InGrip);

	UStaticMesh* GetStaticMesh() const;

	/** The weapon mesh relative to the grip (its Grip socket at the grip). */
	FTransform MeshInGrip;

	UPROPERTY(Transient)
	TObjectPtr<UClass> LinkedLayer;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayingAnimation;
};
