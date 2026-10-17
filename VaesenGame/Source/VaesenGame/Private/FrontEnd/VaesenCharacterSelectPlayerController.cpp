#include "FrontEnd/VaesenCharacterSelectPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/VaesenMusicSubsystem.h"
#include "Core/VaesenSettings.h"

void AVaesenCharacterSelectPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (UVaesenMusicSubsystem* Music = GetGameInstance()->GetSubsystem<UVaesenMusicSubsystem>())
		{
			Music->PlayMusic(UVaesenSettings::Get()->LoginMusic.LoadSynchronous());
		}
	}
}

TSubclassOf<UUserWidget> AVaesenCharacterSelectPlayerController::GetDefaultScreenWidgetClass() const
{
	return UVaesenSettings::Get()->CharacterSelectScreenWidget.LoadSynchronous();
}
