#include "UI/Inventory/VaesenInventoryPanelWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenItem.h"
#include "UI/Inventory/VaesenInventorySlotWidget.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"

#define LOCTEXT_NAMESPACE "VaesenInventoryPanel"

void UVaesenInventoryPanelWidget::SetInventory(UVaesenInventoryComponent* InInventory)
{
	if (Inventory == InInventory)
	{
		Refresh();
		return;
	}

	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UVaesenInventoryPanelWidget::Refresh);
	}
	Inventory = InInventory;
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.AddUniqueDynamic(this, &UVaesenInventoryPanelWidget::Refresh);
	}
	RebuildSlots();
	Refresh();
}

void UVaesenInventoryPanelWidget::SetQuickTransferTarget(UVaesenInventoryComponent* Target)
{
	QuickTransferTarget = Target;
	for (UVaesenInventorySlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->SetQuickTransferTarget(Target);
	}
}

void UVaesenInventoryPanelWidget::NativeDestruct()
{
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UVaesenInventoryPanelWidget::Refresh);
	}
	Super::NativeDestruct();
}

void UVaesenInventoryPanelWidget::RebuildSlots()
{
	SlotWidgets.Reset();
	if (!SlotGrid)
	{
		return;
	}
	SlotGrid->ClearChildren();
	if (!Inventory || !SlotWidgetClass)
	{
		return;
	}

	const int32 CellCount = GetCellCount();
	for (int32 SlotIndex = 0; SlotIndex < CellCount; ++SlotIndex)
	{
		UVaesenInventorySlotWidget* SlotWidget = CreateWidget<UVaesenInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		SlotWidget->SetSlot(Inventory, SlotIndex);
		SlotWidget->SetQuickTransferTarget(QuickTransferTarget.Get());
		SlotGrid->AddChildToUniformGrid(SlotWidget, SlotIndex / Columns, SlotIndex % Columns);
		SlotWidgets.Add(SlotWidget);
	}
}

int32 UVaesenInventoryPanelWidget::GetCellCount() const
{
	if (!Inventory)
	{
		return 0;
	}

	const TArray<UVaesenItem*>& Positions = Inventory->GetSlots();
	int32 Gaps = 0;
	for (const UVaesenItem* Item : Positions)
	{
		Gaps += Item ? 0 : 1;
	}

	int32 Cells;
	if (Inventory->HasUnlimitedCapacity())
	{
		// Always a free row to drop into.
		Cells = Positions.Num() + Columns;
		Cells = FMath::DivideAndRoundUp(Cells, Columns) * Columns;
	}
	else
	{
		// Gaps already count as empty cells.
		Cells = Positions.Num() + FMath::Max(0, Inventory->GetFreeSlotCount() - Gaps);
	}
	return FMath::Max(Cells, 1);
}

void UVaesenInventoryPanelWidget::Refresh()
{
	if (!Inventory)
	{
		return;
	}

	// Stacks come and go (and arrive from the server after the panel may already be up).
	if (SlotWidgets.Num() != GetCellCount())
	{
		RebuildSlots();
	}
	for (UVaesenInventorySlotWidget* SlotWidget : SlotWidgets)
	{
		SlotWidget->Refresh();
	}

	if (TitleText)
	{
		TitleText->SetText(Inventory->GetDisplayName());
	}
	if (CapacityText)
	{
		CapacityText->SetText(Inventory->HasUnlimitedCapacity()
			? FText::Format(LOCTEXT("CapacityUnlimited", "{0} / unlimited"), FText::AsNumber(Inventory->GetUsedSlotCount()))
			: FText::Format(LOCTEXT("Capacity", "{0} / {1}"), FText::AsNumber(Inventory->GetUsedSlotCount()), FText::AsNumber(Inventory->Capacity)));
	}

	const float Weight = Inventory->GetCurrentWeight();
	if (WeightText)
	{
		FNumberFormattingOptions OneDecimal;
		OneDecimal.MinimumFractionalDigits = 0;
		OneDecimal.MaximumFractionalDigits = 1;
		WeightText->SetText(FText::Format(LOCTEXT("Weight", "{0} kg"), FText::AsNumber(Weight, &OneDecimal)));
		WeightText->SetVisibility(Weight > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

bool UVaesenInventoryPanelWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Onto the panel from another inventory: wherever it fits here.
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	const AVaesenPlayerCharacter* Character = GetOwningPlayerPawn<AVaesenPlayerCharacter>();
	UVaesenInventoryComponent* PlayerInventory = Character ? Character->GetInventory() : nullptr;
	if (!Operation || !PlayerInventory || !Inventory)
	{
		return false;
	}
	if (Operation->bFromEquipment)
	{
		if (UVaesenEquipmentComponent* Equipment = Character->GetEquipment())
		{
			Equipment->RequestUnequip(Operation->EquipmentSlot, Inventory);
		}
		return true;
	}
	if (Operation->SourceInventory != Inventory)
	{
		PlayerInventory->RequestMoveItem(Operation->SourceInventory, Operation->SourceIndex, Inventory, INDEX_NONE, Operation->Amount);
	}
	// Handled either way, so dropping on your own panel doesn't throw the item on the ground.
	return true;
}

#undef LOCTEXT_NAMESPACE
