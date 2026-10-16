#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VaesenFrontEndPlayerController.generated.h"

/** Controller for the front-end map: shows the mouse cursor and routes input to the UI only. */
UCLASS()
class VAESENGAME_API AVaesenFrontEndPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
