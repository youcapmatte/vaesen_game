#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenLoadingScreenWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

/**
 * The loading screen shown while traveling between maps (driven by UVaesenLoadingScreenSubsystem).
 *
 * Widget Blueprint (e.g. WBP_LoadingScreen, parent class VaesenLoadingScreenWidget), controls by these exact names:
 *   BackgroundImage   (Image)          - full-screen art: the map's own, else a random one from the settings
 *   MapNameText       (Text)           - e.g. "Gyllencreutz"
 *   TipText           (Text)           - a random tip from the settings
 *   ProgressBar       (Progress Bar)
 *   StatusText        (Text)           - "Loading Gyllencreutz...", "Preparing..."
 * Add a Circular Throbber anywhere; it needs no name, since it spins whenever the screen is shown.
 * The subsystem fades the whole widget in and out (render opacity); don't animate the root's opacity yourself.
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built, so each feature can be tried on its
 * own; whatever is missing is skipped. Make them BindWidget when the UI is done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBackground(UTexture2D* Texture);
	void SetMapName(const FText& MapName);
	void SetTip(const FText& Tip);
	void SetStatus(const FText& Status);

	/** 0..1. */
	void SetProgress(float Progress);

	/** Called when the screen appears, before it fades in: e.g. start an animation. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vaesen|Loading")
	void OnLoadingScreenShown();

	/** Called when loading is done and the screen starts fading out. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vaesen|Loading")
	void OnLoadingFinished();

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MapNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TipText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
};
