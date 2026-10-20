#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/VaesenDamageTypes.h"
#include "VaesenCombatHUDWidget.generated.h"

class UVaesenAbilitySystemComponent;

/**
 * The combat HUD, drawn entirely in C++ (it needs no controls, so the class itself works; a Widget Blueprint of it
 * can restyle the colours and sizes):
 *   - crosshair, only with a ranged weapon drawn or while aiming a throw: its gap is the weapon's current spread
 *     (wider moving and from the hip, tight when aiming);
 *   - hit marker when the server confirms a hit; a different colour for blocked, parried and killing hits;
 *   - ammo: loaded rounds / rounds of that ammo carried (pouches included);
 *   - a warning when the weapon in the hand is close to breaking (or broken);
 *   - the arc of a throw being aimed (this player only).
 */
UCLASS()
class VAESENGAME_API UVaesenCombatHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UVaesenCombatHUDWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor CrosshairColor = FLinearColor(0.95f, 0.92f, 0.85f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor HitColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor KillColor = FLinearColor(0.85f, 0.1f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor BlockedColor = FLinearColor(0.5f, 0.65f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor ParriedColor = FLinearColor(0.95f, 0.75f, 0.3f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	FLinearColor WarningColor = FLinearColor(0.95f, 0.55f, 0.2f);

	/** Length of each crosshair line, in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	float CrosshairLineLength = 10.0f;

	/** Smallest gap, so it never closes up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	float CrosshairMinGap = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat HUD")
	float HitMarkerSeconds = 0.3f;

	/** The server confirmed a hit (for Blueprint extras). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat HUD")
	void OnHitMarker(EVaesenHitOutcome Outcome);

private:
	UFUNCTION()
	void HandleHitConfirmed(EVaesenHitOutcome Outcome);

	/** Listens to the current pawn's ability system (rebinds when the pawn changes). */
	void BindToPawn();

	TWeakObjectPtr<UVaesenAbilitySystemComponent> BoundASC;

	// What to draw, worked out in NativeTick.
	bool bShowCrosshair = false;
	float SpreadPixels = 0.0f;
	FText AmmoText;
	FText WarningText;
	TArray<FVector2D> ThrowArc;
	double HitMarkerTime = -100.0;
	EVaesenHitOutcome HitMarkerOutcome = EVaesenHitOutcome::Hit;
	double Now = 0.0;
};
