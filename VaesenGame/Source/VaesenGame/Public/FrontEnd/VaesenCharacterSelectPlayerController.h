#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndPlayerControllerBase.h"
#include "VaesenCharacterSelectPlayerController.generated.h"

/** Character select screen, reached after logging in: pick a character (and later a realm) and enter the world. */
UCLASS()
class VAESENGAME_API AVaesenCharacterSelectPlayerController : public AVaesenFrontEndPlayerControllerBase
{
	GENERATED_BODY()
};
