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

	const TSubclassOf<UUserWidget> WidgetClass = ScreenWidgetClass ? ScreenWidgetClass : GetDefaultScreenWidgetClass();
	if (WidgetClass)
	{
		ScreenWidget = CreateWidget<UUserWidget>(this, WidgetClass);
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
