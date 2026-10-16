#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndGameModeBase.h"
#include "VaesenLoginGameMode.generated.h"

/** Game mode for the login screen (L_Login). */
UCLASS()
class VAESENGAME_API AVaesenLoginGameMode : public AVaesenFrontEndGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenLoginGameMode();
};
