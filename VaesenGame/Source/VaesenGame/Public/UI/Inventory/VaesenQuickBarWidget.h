#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenQuickBarWidget.generated.h"

class UPanelWidget;
class UVaesenInventoryComponent;
class UVaesenQuickSlotComponent;
class UVaesenQuickSlotWidget;

/**
 * The player's quick slots in a row, keys 1-8. On the HUD (WBP_HUD) during play and inside the inventory screen,
 * where items are dragged onto it. Follows the player's character by itself, and keeps the counts up to date.
 *
 * Widget Blueprint (e.g. WBP_QuickBar, parent class VaesenQuickBarWidget), controls by these exact names:
 *   SlotRow  (any panel, e.g. Horizontal Box) - filled with SlotWidgetClass, one per quick slot
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenQuickBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quick Slots")
	TSubclassOf<UVaesenQuickSlotWidget> SlotWidgetClass;

	/** Gap between slots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quick Slots")
	float SlotSpacing = 6.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> SlotRow;

private:
	/** Follows the player's character (which may arrive, or change, after the bar is up). */
	void Bind(UVaesenQuickSlotComponent* InQuickSlots, UVaesenInventoryComponent* InInventory);
	void Unbind();

	UFUNCTION()
	void Refresh();

	UPROPERTY(Transient)
	TObjectPtr<UVaesenQuickSlotComponent> QuickSlots;

	UPROPERTY(Transient)
	TObjectPtr<UVaesenInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaesenQuickSlotWidget>> SlotWidgets;
};
