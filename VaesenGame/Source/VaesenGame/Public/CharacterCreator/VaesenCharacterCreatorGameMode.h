#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndGameModeBase.h"
#include "VaesenCharacterCreatorGameMode.generated.h"

/** Game mode for the character creator map: no player character or gameplay HUD, just the creator scene's camera. */
UCLASS()
class VAESENGAME_API AVaesenCharacterCreatorGameMode : public AVaesenFrontEndGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenCharacterCreatorGameMode();
};
