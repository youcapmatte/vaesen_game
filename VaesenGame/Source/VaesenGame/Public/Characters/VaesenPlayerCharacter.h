#pragma once

#include "CoreMinimal.h"
#include "Characters/VaesenBaseCharacter.h"
#include "VaesenPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
struct FInputActionValue;

/**
 * Third-person character controlled by a player. Input actions are assigned in the Blueprint subclass (e.g. BP_VaesenPlayerCharacter).
 *
 * The mouse wheel (ZoomAction) moves the camera in and out smoothly between MinThirdPersonDistance and
 * MaxCameraDistance. Zooming in past the closest distance glides the camera into the head and switches to first
 * person: the body turns with the view, and the Head part is not drawn for this player's camera (the torso's neck is
 * closed) but still casts its shadow. Everyone else sees the whole character. One step out returns to third
 * person. The view mode replicates, so the server and other players see the character turn the right way.
 *
 * First-person camera motion: the eyes sit at a steady point in the body (not on the animated head, which jitters)
 * with gentle procedural motion on top: a step bob and sway that grow with speed, slow breathing when standing still
 * and a short dip on landing. The view never rotates by itself. Players scale it all with the camera motion setting
 * (0 = completely still), see SetCameraMotionScale.
 */
UCLASS()
class VAESENGAME_API AVaesenPlayerCharacter : public AVaesenBaseCharacter
{
	GENERATED_BODY()

public:
	AVaesenPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Camera")
	bool IsFirstPerson() const { return bFirstPerson; }

	/** Player setting: how much the first-person camera moves (bob, sway, breathing, landing), 0..1. Saved per user. */
	UFUNCTION(BlueprintCallable, Category = "Camera")
	static void SetCameraMotionScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "Camera")
	static float GetCameraMotionScale();

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Zoom(const FInputActionValue& Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	/** Mouse wheel (Axis1D): positive zooms in. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ZoomAction;

	/** Farthest the camera can be pulled back. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0", Units = "cm"))
	float MaxCameraDistance = 800.0f;

	/** Closest third-person distance; zooming in further switches to first person. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0", Units = "cm"))
	float MinThirdPersonDistance = 150.0f;

	/** How far one wheel step moves the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "1", Units = "cm"))
	float ZoomStep = 60.0f;

	/** How quickly the camera glides to the wanted distance (higher = snappier). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Zoom", meta = (ClampMin = "0.1"))
	float ZoomInterpSpeed = 8.0f;

	/** While gliding into first person, switch once the camera is this close. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (ClampMin = "0", Units = "cm"))
	float FirstPersonSwitchDistance = 30.0f;

	/** How far down you can look in first person, so you can't look straight down into the neck. Third person keeps the full range. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (ClampMin = "-89.9", ClampMax = "0", Units = "Degrees"))
	float FirstPersonMinPitch = -65.0f;

	/** Eye height above the capsule's centre (the capsule is 192 cm tall, so 68 puts the eyes at about 164 cm). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonEyeHeight = 68.0f;

	/** Eyes this far in front of the body's centre, so the view doesn't start inside the head. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonForwardOffset = 10.0f;

	/** Up-and-down per step at walking speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0", Units = "cm"))
	float BobHeight = 1.2f;

	/** Side-to-side per stride at walking speed. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0", Units = "cm"))
	float BobSway = 0.8f;

	/** Distance covered by one full stride (two steps); sets the bob's rhythm. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "10", Units = "cm"))
	float StrideLength = 150.0f;

	/** Breathing when standing still. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0", Units = "cm"))
	float BreathingHeight = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0", Units = "Hz"))
	float BreathingRate = 0.25f;

	/** Deepest dip on landing from a big fall. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0", Units = "cm"))
	float LandingDip = 6.0f;

	/** How quickly the camera eases toward its motion (higher = tighter, lower = floatier). */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person|Motion", meta = (ClampMin = "0.1"))
	float MotionSmoothing = 10.0f;

private:
	void SetFirstPerson(bool bInFirstPerson);
	void ApplyViewMode();

	UFUNCTION(Server, Reliable)
	void ServerSetFirstPerson(bool bInFirstPerson);

	UFUNCTION()
	void OnRep_FirstPerson();

	UPROPERTY(ReplicatedUsing = OnRep_FirstPerson)
	bool bFirstPerson = false;

	/** Where the camera is gliding to; 0 = into the head (first person). Local player only. */
	float DesiredCameraDistance = 400.0f;

	/** First-person eye position plus motion for this frame. */
	FVector GetFirstPersonEyeLocation(float DeltaSeconds);

	// First-person motion state (local player only).
	float BobPhase = 0.0f;
	float BobWeight = 0.0f;
	float LandingOffset = 0.0f;
	float LastFallSpeed = 0.0f;
	FVector MotionOffset = FVector::ZeroVector;
};
