#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VaesenPlayerController.generated.h"

class UInputMappingContext;

/** Player controller used in the worlds. Adds the default Enhanced Input mapping for the local player. */
UCLASS()
class VAESENGAME_API AVaesenPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** Gameplay input mapping; assign in the Blueprint subclass (e.g. IMC_Default). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
};
