#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenQuickSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UVaesenItemTooltipWidget;
class UVaesenQuickSlotComponent;

/**
 * One quick slot: its key, the item's picture (or name) and how many the player has (faded for a moment at 0,
 * until the server empties the slot).
 * Drop an item from an inventory on it to put that kind of item there (only usable, unworn things fit); right-click
 * clears it; clicking uses one (in the inventory screen).
 *
 * Widget Blueprint (e.g. WBP_QuickSlot, parent class VaesenQuickSlotWidget), controls by these exact names:
 *   SlotBorder      (Border)  - the background; tinted on hover
 *   KeyText         (Text)    - the key, "1".."8"
 *   ThumbnailImage  (Image)   - the item's thumbnail; hidden when it has none
 *   NameText        (Text)    - the item's name, shown when it has no thumbnail
 *   CountText       (Text)    - how many the player has
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSlot(UVaesenQuickSlotComponent* InQuickSlots, int32 InIndex);

	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void Refresh();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quick Slots")
	TSubclassOf<UVaesenItemTooltipWidget> TooltipClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Slots")
	FLinearColor NormalColor = FLinearColor(0.03f, 0.025f, 0.02f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Slots")
	FLinearColor HoveredColor = FLinearColor(0.16f, 0.12f, 0.07f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Slots")
	FLinearColor RefusedColor = FLinearColor(0.25f, 0.05f, 0.04f, 0.9f);

	/** Opacity of the picture and name when the player has none left. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Slots", meta = (ClampMin = "0", ClampMax = "1"))
	float EmptyOpacity = 0.35f;

	UPROPERTY(BlueprintReadOnly, Category = "Quick Slots")
	TObjectPtr<UVaesenQuickSlotComponent> QuickSlots;

	UPROPERTY(BlueprintReadOnly, Category = "Quick Slots")
	int32 Index = 0;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountText;

private:
	/** Whether what's being dragged can go in a quick slot. */
	bool Accepts(const UDragDropOperation* Operation) const;

	UPROPERTY(Transient)
	TObjectPtr<UVaesenItemTooltipWidget> Tooltip;
};
