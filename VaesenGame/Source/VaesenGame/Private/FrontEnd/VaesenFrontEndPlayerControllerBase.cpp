#include "FrontEnd/VaesenFrontEndPlayerControllerBase.h"

#include "Blueprint/UserWidget.h"

void AVaesenFrontEndPlayerControllerBase::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	SetShowMouseCursor(true);

	if (ScreenWidgetClass)
	{
		ScreenWidget = CreateWidget<UUserWidget>(this, ScreenWidgetClass);
		ScreenWidget->AddToViewport();

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ScreenWidget->TakeWidget());
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeUIOnly());
	}
}
