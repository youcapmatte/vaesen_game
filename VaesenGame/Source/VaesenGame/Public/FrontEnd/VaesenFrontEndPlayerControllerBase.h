#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VaesenFrontEndPlayerControllerBase.generated.h"

class UUserWidget;

/**
 * Base for the menu-map controllers: shows the mouse cursor, routes input to the UI only,
 * and puts the screen's widget on the viewport.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenFrontEndPlayerControllerBase : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	/** Screen to show when ScreenWidgetClass isn't set in a Blueprint subclass; typically read from UVaesenSettings. */
	virtual TSubclassOf<UUserWidget> GetDefaultScreenWidgetClass() const { return nullptr; }

	/** The screen to show, e.g. WBP_Login. Overrides the default from Project Settings > Game > Vaesen. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> ScreenWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> ScreenWidget;
};
