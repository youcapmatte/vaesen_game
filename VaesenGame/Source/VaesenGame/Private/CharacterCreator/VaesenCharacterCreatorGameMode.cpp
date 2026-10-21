#include "CharacterCreator/VaesenCharacterCreatorGameMode.h"

#include "CharacterCreator/VaesenCharacterCreatorPlayerController.h"

AVaesenCharacterCreatorGameMode::AVaesenCharacterCreatorGameMode()
{
	PlayerControllerClass = AVaesenCharacterCreatorPlayerController::StaticClass();
}
