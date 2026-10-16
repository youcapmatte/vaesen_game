#include "FrontEnd/VaesenCharacterSelectGameMode.h"

#include "FrontEnd/VaesenCharacterSelectPlayerController.h"

AVaesenCharacterSelectGameMode::AVaesenCharacterSelectGameMode()
{
	PlayerControllerClass = AVaesenCharacterSelectPlayerController::StaticClass();
}
