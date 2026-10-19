#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenHUDWidget.generated.h"

class UVaesenMinimapWidget;
class UVaesenQuickBarWidget;

/**
 * The in-game HUD: one full-screen widget that all gameplay UI lives on (minimap, quick bar, and later health,
 * conditions, notifications...). Created by AVaesenHUD from Project Settings > Vaesen > HUD Widget; hidden while
 * full-screen menus like the inventory are open. Lay the parts out in the Widget Blueprint; each part works on its
 * own, following the player's character.
 *
 * Widget Blueprint (e.g. WBP_HUD, parent class VaesenHUDWidget), controls by these exact names:
 *   Minimap   (WBP_Minimap)   - optional
 *   QuickBar  (WBP_QuickBar)  - optional
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "HUD")
	UVaesenMinimapWidget* GetMinimap() const { return Minimap; }

	UFUNCTION(BlueprintPure, Category = "HUD")
	UVaesenQuickBarWidget* GetQuickBar() const { return QuickBar; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVaesenMinimapWidget> Minimap;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UVaesenQuickBarWidget> QuickBar;
};
