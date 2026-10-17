#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndPlayerControllerBase.h"
#include "VaesenLoginPlayerController.generated.h"

class UAudioComponent;

/**
 * Login screen. Shows the login widget (UVaesenSettings::LoginScreenWidget) and plays UVaesenSettings::LoginMusic.
 * Start the game with -DevPlayer=<name> (e.g. -DevPlayer=ingrid) to skip this screen: logs in to the account with that
 * username (it must exist in the API) and character select preselects the dev character with that id.
 */
UCLASS()
class VAESENGAME_API AVaesenLoginPlayerController : public AVaesenFrontEndPlayerControllerBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual TSubclassOf<UUserWidget> GetDefaultScreenWidgetClass() const override;

private:
	void PlayLoginMusic();
	void TryDevAutoLogin();

	/** Stops by itself when the map changes. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicComponent;
};
