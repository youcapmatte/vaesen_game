#include "FrontEnd/VaesenCharacterSelectPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/VaesenSettings.h"

TSubclassOf<UUserWidget> AVaesenCharacterSelectPlayerController::GetDefaultScreenWidgetClass() const
{
	return UVaesenSettings::Get()->CharacterSelectScreenWidget.LoadSynchronous();
}
