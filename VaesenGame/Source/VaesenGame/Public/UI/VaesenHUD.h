#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "VaesenHUD.generated.h"

class UVaesenHUDWidget;

/**
 * The HUD of a player in the world (each player has one, on their own machine only). Puts the HUD widget (Project
 * Settings > Vaesen > HUD Widget, e.g. WBP_HUD) on screen and hides it while full-screen menus are open.
 */
UCLASS()
class VAESENGAME_API AVaesenHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "HUD")
	UVaesenHUDWidget* GetHUDWidget() const { return HUDWidget; }

	/** Shows or hides the gameplay HUD (e.g. hidden while the inventory screen is open). */
	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetHUDVisible(bool bVisible);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVaesenHUDWidget> HUDWidget;
};
