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
 * person: the camera follows the head bone, the head is hidden for this player only, and the body turns with the
 * view. One step out returns to third person. The view mode replicates, so the server and other players see the
 * character turn the right way.
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

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

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

	/** Bone the first-person camera follows and that is hidden for the owner in first person. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person")
	FName HeadBoneName = TEXT("head");

	/** Eye position relative to the head bone: forward along the character's facing, and up. */
	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonForwardOffset = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|First Person", meta = (Units = "cm"))
	float FirstPersonUpOffset = 8.0f;

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
};
