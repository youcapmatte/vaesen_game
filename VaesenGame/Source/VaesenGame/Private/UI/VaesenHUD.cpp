#include "UI/VaesenHUD.h"

#include "Core/VaesenSettings.h"
#include "GameFramework/PlayerController.h"
#include "UI/VaesenHUDWidget.h"

void AVaesenHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* Player = GetOwningPlayerController();
	const TSubclassOf<UVaesenHUDWidget> WidgetClass = UVaesenSettings::Get()->HUDWidget.LoadSynchronous();
	if (Player && Player->IsLocalController() && WidgetClass)
	{
		HUDWidget = CreateWidget<UVaesenHUDWidget>(Player, WidgetClass);
		// Below menus and loading screens.
		HUDWidget->AddToViewport(0);
	}
}

void AVaesenHUD::SetHUDVisible(bool bVisible)
{
	if (HUDWidget)
	{
		HUDWidget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
