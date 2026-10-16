#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VaesenCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
struct FInputActionValue;

/** Third-person player character. Input actions are assigned in the Blueprint subclass (e.g. BP_VaesenCharacter). */
UCLASS()
class VAESENGAME_API AVaesenCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVaesenCharacter();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

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
};
