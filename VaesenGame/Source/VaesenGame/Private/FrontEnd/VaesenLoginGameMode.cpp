#include "FrontEnd/VaesenLoginGameMode.h"

#include "FrontEnd/VaesenLoginPlayerController.h"

AVaesenLoginGameMode::AVaesenLoginGameMode()
{
	PlayerControllerClass = AVaesenLoginPlayerController::StaticClass();
}
