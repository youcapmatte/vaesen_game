#include "UI/Inventory/VaesenInventorySlotWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenItem.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"
#include "UI/Inventory/VaesenItemTooltipWidget.h"

void UVaesenInventorySlotWidget::SetSlot(UVaesenInventoryComponent* InInventory, int32 InSlotIndex)
{
	Inventory = InInventory;
	SlotIndex = InSlotIndex;
	Refresh();
}

UVaesenItem* UVaesenInventorySlotWidget::GetItem() const
{
	return Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
}

UVaesenInventoryComponent* UVaesenInventorySlotWidget::GetPlayerInventory() const
{
	const AVaesenPlayerCharacter* Character = GetOwningPlayerPawn<AVaesenPlayerCharacter>();
	return Character ? Character->GetInventory() : nullptr;
}

void UVaesenInventorySlotWidget::Refresh()
{
	UVaesenItem* Item = GetItem();

	if (ThumbnailImage)
	{
		const bool bHasThumbnail = Item && Item->Thumbnail;
		if (bHasThumbnail)
		{
			ThumbnailImage->SetBrushFromTexture(Item->Thumbnail);
		}
		ThumbnailImage->SetVisibility(bHasThumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (NameText)
	{
		// The name stands in for a missing thumbnail.
		const bool bShowName = Item && !Item->Thumbnail;
		NameText->SetText(bShowName ? Item->ItemName : FText::GetEmpty());
		NameText->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	ShowQuantity(Item ? Item->GetQuantity() : 0);

	if (Item && TooltipClass)
	{
		if (!Tooltip)
		{
			Tooltip = CreateWidget<UVaesenItemTooltipWidget>(GetOwningPlayer(), TooltipClass);
		}
		Tooltip->SetItem(Item);
		SetToolTip(Tooltip);
	}
	else
	{
		SetToolTip(nullptr);
	}
}

void UVaesenInventorySlotWidget::ShowQuantity(int32 Amount)
{
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Amount));
		QuantityText->SetVisibility(Amount > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

FReply UVaesenInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UVaesenInventoryComponent* PlayerInventory = GetPlayerInventory();
	if (!GetItem() || !PlayerInventory)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		PlayerInventory->RequestUseItem(Inventory, SlotIndex);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Shift-click: straight to the other open inventory.
		if (InMouseEvent.IsShiftDown() && QuickTransferTarget.IsValid())
		{
			PlayerInventory->RequestMoveItem(Inventory, SlotIndex, QuickTransferTarget.Get(), INDEX_NONE);
			return FReply::Handled();
		}
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVaesenInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	const UVaesenItem* Item = GetItem();
	if (!Item)
	{
		return;
	}

	UVaesenItemDragDropOperation* Operation = NewObject<UVaesenItemDragDropOperation>();
	Operation->SourceInventory = Inventory;
	Operation->SourceIndex = SlotIndex;
	// Ctrl: half the stack.
	const bool bSplit = FSlateApplication::Get().GetModifierKeys().IsControlDown() && Item->GetQuantity() > 1;
	Operation->Amount = bSplit ? Item->GetQuantity() / 2 : -1;

	// A copy of this slot follows the mouse.
	UVaesenInventorySlotWidget* Visual = CreateWidget<UVaesenInventorySlotWidget>(GetOwningPlayer(), GetClass());
	Visual->SetSlot(Inventory, SlotIndex);
	Visual->SetToolTip(nullptr);
	if (bSplit)
	{
		Visual->ShowQuantity(Operation->Amount);
	}
	Operation->DefaultDragVisual = Visual;
	Operation->Pivot = EDragPivot::CenterCenter;
	OutOperation = Operation;
}

bool UVaesenInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetHighlighted(false);

	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	UVaesenInventoryComponent* PlayerInventory = GetPlayerInventory();
	if (!Operation || !PlayerInventory || !Inventory)
	{
		return false;
	}

	PlayerInventory->RequestMoveItem(Operation->SourceInventory, Operation->SourceIndex, Inventory, SlotIndex, Operation->Amount);
	return true;
}

void UVaesenInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	SetHighlighted(true);
}

void UVaesenInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	SetHighlighted(false);
}

void UVaesenInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	SetHighlighted(true);
}

void UVaesenInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	SetHighlighted(false);
}

void UVaesenInventorySlotWidget::SetHighlighted(bool bHighlighted)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(bHighlighted ? HoveredColor : NormalColor);
	}
}
