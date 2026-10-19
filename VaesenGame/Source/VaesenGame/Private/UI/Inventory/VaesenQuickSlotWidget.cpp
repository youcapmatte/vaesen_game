#include "UI/Inventory/VaesenQuickSlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Inventory/VaesenQuickSlotComponent.h"
#include "Items/VaesenItem.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"
#include "UI/Inventory/VaesenItemTooltipWidget.h"

void UVaesenQuickSlotWidget::SetSlot(UVaesenQuickSlotComponent* InQuickSlots, int32 InIndex)
{
	QuickSlots = InQuickSlots;
	Index = InIndex;
	Refresh();
}

void UVaesenQuickSlotWidget::Refresh()
{
	const TSubclassOf<UVaesenItem> ItemClass = QuickSlots ? QuickSlots->GetSlot(Index) : nullptr;
	const UVaesenItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenItem>() : nullptr;
	const int32 Count = QuickSlots ? QuickSlots->GetSlotCount(Index) : 0;

	if (KeyText)
	{
		KeyText->SetText(FText::AsNumber(Index + 1));
	}
	if (ThumbnailImage)
	{
		const bool bHasThumbnail = Defaults && Defaults->Thumbnail;
		if (bHasThumbnail)
		{
			ThumbnailImage->SetBrushFromTexture(Defaults->Thumbnail);
		}
		ThumbnailImage->SetVisibility(bHasThumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		ThumbnailImage->SetRenderOpacity(Count > 0 ? 1.0f : EmptyOpacity);
	}
	if (NameText)
	{
		const bool bShowName = Defaults && !Defaults->Thumbnail;
		NameText->SetText(bShowName ? Defaults->ItemName : FText::GetEmpty());
		NameText->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		NameText->SetRenderOpacity(Count > 0 ? 1.0f : EmptyOpacity);
	}
	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Count));
		CountText->SetVisibility(Defaults ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// The tooltip describes one the player has, or the kind in general.
	UVaesenItem* Shown = QuickSlots ? QuickSlots->FindItemForSlot(Index) : nullptr;
	if (!Shown && ItemClass)
	{
		Shown = ItemClass->GetDefaultObject<UVaesenItem>();
	}
	if (Shown && TooltipClass)
	{
		if (!Tooltip)
		{
			Tooltip = CreateWidget<UVaesenItemTooltipWidget>(GetOwningPlayer(), TooltipClass);
		}
		Tooltip->SetItem(Shown);
		SetToolTip(Tooltip);
	}
	else
	{
		SetToolTip(nullptr);
	}
}

bool UVaesenQuickSlotWidget::Accepts(const UDragDropOperation* InOperation) const
{
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	if (!Operation || Operation->bFromEquipment || !Operation->SourceInventory || !QuickSlots)
	{
		return false;
	}
	const UVaesenItem* Item = Operation->SourceInventory->GetItemAt(Operation->SourceIndex);
	return Item && Item->CanBeQuickSlotted();
}

FReply UVaesenQuickSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!QuickSlots || !QuickSlots->GetSlot(Index))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		QuickSlots->RequestClear(Index);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		QuickSlots->RequestUse(Index);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

bool UVaesenQuickSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
	if (Accepts(InOperation))
	{
		const UVaesenItemDragDropOperation* Operation = CastChecked<UVaesenItemDragDropOperation>(InOperation);
		QuickSlots->RequestAssign(Index, Operation->SourceInventory->GetItemAt(Operation->SourceIndex)->GetClass());
	}
	// Handled either way: the item stays where it is.
	return true;
}

void UVaesenQuickSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(HoveredColor);
	}
}

void UVaesenQuickSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
}

void UVaesenQuickSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(Accepts(InOperation) ? HoveredColor : RefusedColor);
	}
}

void UVaesenQuickSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
}
