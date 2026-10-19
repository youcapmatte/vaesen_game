#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenMinimapWidget.generated.h"

class AVaesenMinimapCapture;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * The minimap: a round, live picture of the level around the player from above (AVaesenMinimapCapture), with a
 * ring border and the player's arrow in the middle. It turns with the player's view, so up is where they look (or
 * stays north-up, with bRotateWithView off).
 *
 * Part of the HUD (WBP_HUD). The camera is spawned on this player's machine while the widget is on screen.
 *
 * Widget Blueprint (e.g. WBP_Minimap, parent class VaesenMinimapWidget), controls by these exact names:
 *   MapImage  (Image)  - drawn with MapMaterial, which gets the live map as its MapTexture parameter
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** UI material that draws the map round with its border (M_Minimap); needs a MapTexture texture parameter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UMaterialInterface> MapMaterial;

	/**
	 * How the map is drawn: a post-process material on its camera (M_MinimapInk: ink on parchment). Empty = a plain
	 * picture.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<UMaterialInterface> MapStyle;

	/** The map camera (make a Blueprint of it to change how much it shows, how often, its exposure). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	TSubclassOf<AVaesenMinimapCapture> CaptureClass;

	/** Up on the map is where the player looks. Off: north (the world's +X) is always up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	bool bRotateWithView = true;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> MapImage;

private:
	UPROPERTY(Transient)
	TObjectPtr<AVaesenMinimapCapture> MapCapture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MapMaterialInstance;
};
