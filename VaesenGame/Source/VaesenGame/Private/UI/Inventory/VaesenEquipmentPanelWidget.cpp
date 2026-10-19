#include "UI/Inventory/VaesenEquipmentPanelWidget.h"

#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "UI/Inventory/VaesenEquipmentSlotWidget.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"

void UVaesenEquipmentPanelWidget::SetEquipment(UVaesenEquipmentComponent* InEquipment)
{
	if (Equipment == InEquipment && SlotWidgets.Num() > 0)
	{
		Refresh();
		return;
	}

	if (Equipment)
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}
	Equipment = InEquipment;
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}

	SlotWidgets.Reset();
	if (!SlotGrid)
	{
		return;
	}
	SlotGrid->ClearChildren();
	if (!Equipment || !SlotWidgetClass)
	{
		return;
	}

	const TArray<EVaesenEquipmentSlot> Slots = UVaesenEquipmentComponent::GetPlayerSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		UVaesenEquipmentSlotWidget* SlotWidget = CreateWidget<UVaesenEquipmentSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		SlotWidget->SetSlot(Equipment, Slots[Index]);
		SlotGrid->AddChildToUniformGrid(SlotWidget, Index / 2, Index % 2);
		SlotWidgets.Add(SlotWidget);
	}
}

void UVaesenEquipmentPanelWidget::NativeDestruct()
{
	if (Equipment)
	{
		Equipment->OnEquipmentChanged.RemoveDynamic(this, &UVaesenEquipmentPanelWidget::Refresh);
	}
	Super::NativeDestruct();
}

void UVaesenEquipmentPanelWidget::Refresh()
{
	for (UVaesenEquipmentSlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->Refresh();
	}
}

bool UVaesenEquipmentPanelWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// An item from an inventory: on, in its own slot.
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	if (Operation && !Operation->bFromEquipment && Operation->SourceInventory && Equipment
		&& Equipment->CanEquip(Operation->SourceInventory->GetItemAt(Operation->SourceIndex)))
	{
		Equipment->RequestEquipFromInventory(Operation->SourceInventory, Operation->SourceIndex);
	}
	return true;
}
