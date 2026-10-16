#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndPlayerControllerBase.h"
#include "VaesenCharacterSelectPlayerController.generated.h"

/**
 * Character select screen, reached after logging in: pick a character (and later a realm) and enter the world.
 * Shows UVaesenSettings::CharacterSelectScreenWidget.
 */
UCLASS()
class VAESENGAME_API AVaesenCharacterSelectPlayerController : public AVaesenFrontEndPlayerControllerBase
{
	GENERATED_BODY()

protected:
	virtual TSubclassOf<UUserWidget> GetDefaultScreenWidgetClass() const override;
};
