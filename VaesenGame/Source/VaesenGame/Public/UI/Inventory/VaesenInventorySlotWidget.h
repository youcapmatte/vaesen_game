#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenInventorySlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVaesenInventoryComponent;
class UVaesenItem;
class UVaesenItemTooltipWidget;

/**
 * One slot of an inventory. Drag to move (onto an empty slot), merge (onto the same item) or swap; hold Ctrl while
 * starting a drag to take half the stack. Right-click uses the item; Shift-click sends it to the other open
 * inventory (chest <-> player). Hover shows the item's tooltip.
 *
 * Widget Blueprint (e.g. WBP_InventorySlot, parent class VaesenInventorySlotWidget), controls by these exact names:
 *   SlotBorder      (Border)  - the slot's background; tinted on hover
 *   ThumbnailImage  (Image)   - the item's thumbnail; hidden when it has none
 *   NameText        (Text)    - the item's name, shown when it has no thumbnail
 *   QuantityText    (Text)    - the stack's count; hidden for single items
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows slot SlotIndex of Inventory. */
	void SetSlot(UVaesenInventoryComponent* InInventory, int32 InSlotIndex);

	/** Where Shift-click sends the item (the other open inventory), or null. */
	void SetQuickTransferTarget(UVaesenInventoryComponent* Target) { QuickTransferTarget = Target; }

	/** Re-reads the item. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Refresh();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UVaesenItem* GetItem() const;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** The tooltip shown over items. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<UVaesenItemTooltipWidget> TooltipClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FLinearColor NormalColor = FLinearColor(0.03f, 0.025f, 0.02f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FLinearColor HoveredColor = FLinearColor(0.16f, 0.12f, 0.07f, 0.95f);

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UVaesenInventoryComponent> Inventory;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

private:
	/** The local player's inventory, through which every change is requested. */
	UVaesenInventoryComponent* GetPlayerInventory() const;

	void SetHighlighted(bool bHighlighted);

	/** Shows Amount instead of the stack's count (on a drag visual). */
	void ShowQuantity(int32 Amount);

	UPROPERTY(Transient)
	TObjectPtr<UVaesenItemTooltipWidget> Tooltip;

	TWeakObjectPtr<UVaesenInventoryComponent> QuickTransferTarget;
};
