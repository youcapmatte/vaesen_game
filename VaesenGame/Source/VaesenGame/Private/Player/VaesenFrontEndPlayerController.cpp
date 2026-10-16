#include "Player/VaesenFrontEndPlayerController.h"

void AVaesenFrontEndPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		SetShowMouseCursor(true);
		SetInputMode(FInputModeUIOnly());
	}
}
