#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenInventoryPanelWidget.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UVaesenInventoryComponent;
class UVaesenInventorySlotWidget;

/**
 * One inventory as a grid: a cell per stack, then as many empty cells as there are free slots (one spare row when
 * the inventory is unlimited). Shows its name, how full it is and its total weight. Dropping an item on the panel
 * (not on a cell) puts it wherever it fits.
 *
 * Widget Blueprint (e.g. WBP_InventoryPanel, parent class VaesenInventoryPanelWidget), controls by these exact names:
 *   TitleText     (Text)               - the inventory's name
 *   CapacityText  (Text)               - used / total slots ("used / unlimited" when unlimited)
 *   WeightText    (Text)               - total weight, for information; hidden when nothing weighs anything
 *   SlotGrid      (Uniform Grid Panel) - filled with SlotWidgetClass, Columns wide
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenInventoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows this inventory (null: shows nothing). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetInventory(UVaesenInventoryComponent* InInventory);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UVaesenInventoryComponent* GetInventory() const { return Inventory; }

	/** Where Shift-click on a slot sends items (the other open inventory), or null. */
	void SetQuickTransferTarget(UVaesenInventoryComponent* Target);

protected:
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UVaesenInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Columns = 6;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UVaesenInventoryComponent> Inventory;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapacityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeightText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> SlotGrid;

private:
	/** Slots, header and weight; rebuilds the grid if the number of slots changed. */
	UFUNCTION()
	void Refresh();

	void RebuildSlots();

	/** How many cells to show: every position, plus empty ones for the free slots (a spare row when unlimited). */
	int32 GetCellCount() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaesenInventorySlotWidget>> SlotWidgets;

	TWeakObjectPtr<UVaesenInventoryComponent> QuickTransferTarget;
};
