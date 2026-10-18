#include "UI/Inventory/VaesenInventoryPanelWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Inventory/VaesenInventoryComponent.h"
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

	const int32 SlotCount = Inventory->GetSlots().Num();
	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		UVaesenInventorySlotWidget* SlotWidget = CreateWidget<UVaesenInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
		SlotWidget->SetSlot(Inventory, SlotIndex);
		SlotWidget->SetQuickTransferTarget(QuickTransferTarget.Get());
		SlotGrid->AddChildToUniformGrid(SlotWidget, SlotIndex / Columns, SlotIndex % Columns);
		SlotWidgets.Add(SlotWidget);
	}
}

void UVaesenInventoryPanelWidget::Refresh()
{
	if (!Inventory)
	{
		return;
	}

	// The slots arrive from the server after the panel may already be up.
	if (SlotWidgets.Num() != Inventory->GetSlots().Num())
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
		CapacityText->SetText(FText::Format(LOCTEXT("Capacity", "{0} / {1}"), FText::AsNumber(Inventory->GetUsedSlotCount()), FText::AsNumber(Inventory->GetSlots().Num())));
	}

	const bool bWeightLimit = Inventory->WeightCapacity > 0.0f;
	const float Weight = Inventory->GetCurrentWeight();
	if (WeightText)
	{
		FNumberFormattingOptions OneDecimal;
		OneDecimal.MinimumFractionalDigits = 0;
		OneDecimal.MaximumFractionalDigits = 1;
		WeightText->SetText(FText::Format(LOCTEXT("Weight", "{0} / {1} kg"), FText::AsNumber(Weight, &OneDecimal), FText::AsNumber(Inventory->WeightCapacity, &OneDecimal)));
		WeightText->SetVisibility(bWeightLimit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (WeightBar)
	{
		WeightBar->SetPercent(bWeightLimit ? FMath::Clamp(Weight / Inventory->WeightCapacity, 0.0f, 1.0f) : 0.0f);
		WeightBar->SetVisibility(bWeightLimit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
	if (Operation->SourceInventory != Inventory)
	{
		PlayerInventory->RequestMoveItem(Operation->SourceInventory, Operation->SourceIndex, Inventory, INDEX_NONE, Operation->Amount);
	}
	// Handled either way, so dropping on your own panel doesn't throw the item on the ground.
	return true;
}

#undef LOCTEXT_NAMESPACE
