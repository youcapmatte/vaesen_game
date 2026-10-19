#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "VaesenVitalsWidget.generated.h"

class UAbilitySystemComponent;
class UImage;
class UMaterialInstanceDynamic;
class UPanelWidget;
class UTextBlock;
struct FOnAttributeChangeData;

/**
 * The player's vitals at the top left of the HUD, like The Witcher 3's: a medallion, the health bar beside it, a
 * thinner stamina bar under it, and a row of status badges below (poisoned, bleeding, frightened...).
 *
 * The bars follow the Gameplay Ability System: Health / MaxHealth and Stamina / MaxStamina in the character's
 * UVaesenAttributeSet, updated whenever a gameplay effect (or anything else) changes them. A lost chunk lingers a
 * moment in a lighter colour before draining away. Status badges are the character's Status.* gameplay tags (granted by
 * effects, or added directly), with their icon from Project Settings > Vaesen > Status Icons and the time left if an
 * effect grants them.
 *
 * Widget Blueprint (e.g. WBP_Vitals, parent class VaesenVitalsWidget), controls by these exact names:
 *   HealthBar    (Image)  - drawn with a bar material (M_VitalBar) with Percent, Trail and FillColor parameters
 *   StaminaBar   (Image)  - the same
 *   HealthText   (Text)   - optional, e.g. "80 / 100"
 *   StaminaText  (Text)   - optional
 *   StatusRow    (any panel, e.g. Horizontal Box) - filled with a badge per status
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenVitalsWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals")
	FLinearColor HealthColor = FLinearColor(0.55f, 0.04f, 0.03f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals")
	FLinearColor StaminaColor = FLinearColor(0.82f, 0.68f, 0.25f);

	/** How long a lost chunk stays before draining away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0", Units = "s"))
	float TrailDelay = 0.6f;

	/** How fast bars move to their new value (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "0.1"))
	float BarSpeed = 12.0f;

	/** The tag whose children are shown as statuses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vitals")
	FGameplayTag StatusParentTag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StaminaBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StatusRow;

private:
	/** One bar's state: where it's going, what's shown, and the lingering chunk. */
	struct FBar
	{
		float Target = 1.0f;
		float Shown = 1.0f;
		float Trail = 1.0f;
		float TrailHold = 0.0f;
		float Current = 0.0f;
		float Max = 0.0f;
	};

	/** Follows the player's character's ability system (it arrives, or changes, after the HUD is up). */
	void Bind(UAbilitySystemComponent* InAbilitySystem);
	void Unbind();

	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void OnStaminaChanged(const FOnAttributeChangeData& Data);
	void ReadAttributes();

	void SetTarget(FBar& Bar, float Current, float Max);
	void TickBar(FBar& Bar, UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& Material, const FLinearColor& Color, float DeltaTime);

	/** Rebuilds the badges when the statuses change, and updates their timers. */
	void UpdateStatuses();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;

	FBar Health;
	FBar Stamina;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HealthMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> StaminaMaterial;

	FGameplayTagContainer ShownStatuses;
	float StatusTimer = 0.0f;

	/** Each status badge's timer text, by tag. */
	TMap<FGameplayTag, TWeakObjectPtr<UTextBlock>> StatusTimers;
};
