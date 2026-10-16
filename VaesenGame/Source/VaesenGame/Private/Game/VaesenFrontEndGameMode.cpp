#include "Game/VaesenFrontEndGameMode.h"

#include "Player/VaesenFrontEndPlayerController.h"

AVaesenFrontEndGameMode::AVaesenFrontEndGameMode()
{
	PlayerControllerClass = AVaesenFrontEndPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
}
