#include "Player/VaesenPlayerController.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Core/VaesenMusicSubsystem.h"
#include "Core/VaesenSettings.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "UI/Inventory/VaesenInventoryScreenWidget.h"
#include "UI/VaesenHUD.h"

void AVaesenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	// The front end leaves a UI-only input mode and a visible cursor behind; in the world the mouse looks around.
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);

	// Entering a world ends the front-end music; the world plays its own.
	if (UVaesenMusicSubsystem* Music = GetGameInstance()->GetSubsystem<UVaesenMusicSubsystem>())
	{
		Music->StopMusic();
	}

	if (!DefaultMappingContext)
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

bool AVaesenPlayerController::IsInventoryOpen() const
{
	return InventoryScreen && InventoryScreen->IsInViewport();
}

void AVaesenPlayerController::ToggleInventory()
{
	if (IsInventoryOpen())
	{
		CloseInventory();
	}
	else
	{
		OpenInventory();
	}
}

void AVaesenPlayerController::OpenInventory(UVaesenInventoryComponent* Container)
{
	const AVaesenPlayerCharacter* PlayerCharacter = Cast<AVaesenPlayerCharacter>(GetPawn());
	if (!IsLocalController() || !PlayerCharacter || !PlayerCharacter->GetInventory())
	{
		return;
	}

	if (!InventoryScreen)
	{
		const TSubclassOf<UVaesenInventoryScreenWidget> ScreenClass = UVaesenSettings::Get()->InventoryScreenWidget.LoadSynchronous();
		if (!ScreenClass)
		{
			return;
		}
		InventoryScreen = CreateWidget<UVaesenInventoryScreenWidget>(this, ScreenClass);
	}

	InventoryScreen->SetInventories(PlayerCharacter->GetInventory(), Container);
	if (!InventoryScreen->IsInViewport())
	{
		InventoryScreen->AddToViewport(10);
		if (AVaesenHUD* GameHUD = GetHUD<AVaesenHUD>())
		{
			GameHUD->SetHUDVisible(false);
		}

		// Mouse for the UI, keys still walk; the view doesn't turn while the cursor is out.
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(InventoryScreen->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
		SetShowMouseCursor(true);
		SetIgnoreLookInput(true);
	}
}

void AVaesenPlayerController::CloseInventory()
{
	if (!IsInventoryOpen())
	{
		return;
	}

	InventoryScreen->RemoveFromParent();
	if (AVaesenHUD* GameHUD = GetHUD<AVaesenHUD>())
	{
		GameHUD->SetHUDVisible(true);
	}
	SetInputMode(FInputModeGameOnly());
	SetShowMouseCursor(false);
	ResetIgnoreLookInput();
}
