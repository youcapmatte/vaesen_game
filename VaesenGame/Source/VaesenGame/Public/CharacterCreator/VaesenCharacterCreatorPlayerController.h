#pragma once

#include "CoreMinimal.h"
#include "FrontEnd/VaesenFrontEndPlayerControllerBase.h"
#include "VaesenCharacterCreatorPlayerController.generated.h"

class AVaesenCharacterCreatorScene;

/**
 * The character creator map's controller: opens the level's creator scene, shows the creator panel
 * (UVaesenCharacterCreatorWidget unless ScreenWidgetClass says otherwise) and lets the arrow keys turn the preview.
 */
UCLASS()
class VAESENGAME_API AVaesenCharacterCreatorPlayerController : public AVaesenFrontEndPlayerControllerBase
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual TSubclassOf<UUserWidget> GetDefaultScreenWidgetClass() const override;

	/** The sex the creator starts with. */
	UPROPERTY(EditDefaultsOnly, Category = "Character Creator")
	FName StartSex = TEXT("female");

	/** Turntable speed for the arrow keys, degrees per second. */
	UPROPERTY(EditDefaultsOnly, Category = "Character Creator")
	float TurnSpeed = 120.f;

private:
	UPROPERTY()
	TObjectPtr<AVaesenCharacterCreatorScene> Scene;
};
