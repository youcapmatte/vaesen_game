#include "Player/VaesenPlayerController.h"

#include "Core/VaesenMusicSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

void AVaesenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

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
