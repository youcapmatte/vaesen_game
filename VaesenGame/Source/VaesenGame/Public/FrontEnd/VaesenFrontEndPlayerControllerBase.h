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

	/** The screen to show, e.g. WBP_Login. Assign in the Blueprint subclass. */
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UUserWidget> ScreenWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI")
	TObjectPtr<UUserWidget> ScreenWidget;
};
