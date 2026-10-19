#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenEquipmentSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVaesenEquipmentComponent;
class UVaesenItemTooltipWidget;

/**
 * One equipment slot (Head, Chest, Main Hand...). Drop an item from an inventory on it to put it on (only items for
 * this slot are taken); drag it back to an inventory, or right-click, to take it off. Hover shows the item's tooltip.
 *
 * Widget Blueprint (e.g. WBP_EquipmentSlot, parent class VaesenEquipmentSlotWidget), controls by these exact names:
 *   SlotBorder      (Border)  - the background; tinted on hover
 *   SlotLabelText   (Text)    - the slot's name, e.g. "Head"
 *   ThumbnailImage  (Image)   - the item's thumbnail; hidden when it has none
 *   NameText        (Text)    - the item's name, shown when it has no thumbnail
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenEquipmentSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSlot(UVaesenEquipmentComponent* InEquipment, EVaesenEquipmentSlot InSlot);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Refresh();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UVaesenItemTooltipWidget> TooltipClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FLinearColor NormalColor = FLinearColor(0.03f, 0.025f, 0.02f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FLinearColor HoveredColor = FLinearColor(0.16f, 0.12f, 0.07f, 0.95f);

	/** While dragging something that can't go in this slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FLinearColor RefusedColor = FLinearColor(0.25f, 0.05f, 0.04f, 0.9f);

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquipmentComponent> Equipment;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EVaesenEquipmentSlot EquipmentSlot = EVaesenEquipmentSlot::Head;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotLabelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

private:
	/** Whether what's being dragged may go in this slot. */
	bool Accepts(const UDragDropOperation* Operation) const;

	UPROPERTY(Transient)
	TObjectPtr<UVaesenItemTooltipWidget> Tooltip;
};
