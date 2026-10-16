#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndGameModeBase.h"
#include "VaesenCharacterSelectGameMode.generated.h"

/** Game mode for the character select screen (L_CharacterSelect). */
UCLASS()
class VAESENGAME_API AVaesenCharacterSelectGameMode : public AVaesenFrontEndGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenCharacterSelectGameMode();
};
