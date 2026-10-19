#include "UI/Inventory/VaesenEquipmentSlotWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenEquippableItem.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"
#include "UI/Inventory/VaesenItemTooltipWidget.h"

void UVaesenEquipmentSlotWidget::SetSlot(UVaesenEquipmentComponent* InEquipment, EVaesenEquipmentSlot InSlot)
{
	Equipment = InEquipment;
	EquipmentSlot = InSlot;
	Refresh();
}

void UVaesenEquipmentSlotWidget::Refresh()
{
	UVaesenEquippableItem* Item = Equipment ? Equipment->GetItemInSlot(EquipmentSlot) : nullptr;

	if (SlotLabelText)
	{
		SlotLabelText->SetText(UEnum::GetDisplayValueAsText(EquipmentSlot));
	}
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
		const bool bShowName = Item && !Item->Thumbnail;
		NameText->SetText(bShowName ? Item->ItemName : FText::GetEmpty());
		NameText->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

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

bool UVaesenEquipmentSlotWidget::Accepts(const UDragDropOperation* InOperation) const
{
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	if (!Operation || Operation->bFromEquipment || !Operation->SourceInventory || !Equipment)
	{
		return false;
	}
	return Equipment->CanEquipInSlot(Operation->SourceInventory->GetItemAt(Operation->SourceIndex), EquipmentSlot);
}

FReply UVaesenEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!Equipment || !Equipment->GetItemInSlot(EquipmentSlot))
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	// Right-click: off, into the player's own inventory.
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Equipment->RequestUnequip(EquipmentSlot);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UVaesenEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!Equipment || !Equipment->GetItemInSlot(EquipmentSlot))
	{
		return;
	}

	UVaesenItemDragDropOperation* Operation = NewObject<UVaesenItemDragDropOperation>();
	Operation->bFromEquipment = true;
	Operation->EquipmentSlot = EquipmentSlot;

	UVaesenEquipmentSlotWidget* Visual = CreateWidget<UVaesenEquipmentSlotWidget>(GetOwningPlayer(), GetClass());
	Visual->SetSlot(Equipment, EquipmentSlot);
	Visual->SetToolTip(nullptr);
	Operation->DefaultDragVisual = Visual;
	Operation->Pivot = EDragPivot::CenterCenter;
	OutOperation = Operation;
}

bool UVaesenEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}

	// Handled either way: an item for another slot just doesn't go on.
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	if (Accepts(InOperation))
	{
		Equipment->RequestEquipFromInventory(Operation->SourceInventory, Operation->SourceIndex);
	}
	else if (Equipment && Operation && !Operation->bFromEquipment && Operation->SourceInventory)
	{
		const UVaesenItem* Refused = Operation->SourceInventory->GetItemAt(Operation->SourceIndex);
		Equipment->PlayEquipRejectedSound(Refused ? Refused->GetClass() : nullptr);
	}
	return true;
}

void UVaesenEquipmentSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(HoveredColor);
	}
}

void UVaesenEquipmentSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
}

void UVaesenEquipmentSlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(Accepts(InOperation) ? HoveredColor : RefusedColor);
	}
}

void UVaesenEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(NormalColor);
	}
}
