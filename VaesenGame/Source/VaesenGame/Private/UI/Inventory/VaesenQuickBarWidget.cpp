#include "UI/Inventory/VaesenQuickBarWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Inventory/VaesenQuickSlotComponent.h"
#include "UI/Inventory/VaesenQuickSlotWidget.h"

void UVaesenQuickBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AVaesenPlayerCharacter* Character = GetOwningPlayerPawn<AVaesenPlayerCharacter>();
	UVaesenQuickSlotComponent* CurrentQuickSlots = Character ? Character->GetQuickSlots() : nullptr;
	if (CurrentQuickSlots != QuickSlots)
	{
		Bind(CurrentQuickSlots, Character ? Character->GetInventory() : nullptr);
	}
}

void UVaesenQuickBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UVaesenQuickBarWidget::Bind(UVaesenQuickSlotComponent* InQuickSlots, UVaesenInventoryComponent* InInventory)
{
	Unbind();
	QuickSlots = InQuickSlots;
	Inventory = InInventory;
	if (QuickSlots)
	{
		QuickSlots->OnQuickSlotsChanged.AddUniqueDynamic(this, &UVaesenQuickBarWidget::Refresh);
	}
	if (Inventory)
	{
		// Counts change with the inventory.
		Inventory->OnInventoryUpdated.AddUniqueDynamic(this, &UVaesenQuickBarWidget::Refresh);
	}
	Refresh();
}

void UVaesenQuickBarWidget::Unbind()
{
	if (QuickSlots)
	{
		QuickSlots->OnQuickSlotsChanged.RemoveDynamic(this, &UVaesenQuickBarWidget::Refresh);
	}
	if (Inventory)
	{
		Inventory->OnInventoryUpdated.RemoveDynamic(this, &UVaesenQuickBarWidget::Refresh);
	}
	QuickSlots = nullptr;
	Inventory = nullptr;
}

void UVaesenQuickBarWidget::Refresh()
{
	const int32 Count = QuickSlots ? QuickSlots->GetSlots().Num() : 0;

	// More or fewer slots (a belt put on or taken off): rebuild the row.
	if (SlotRow && SlotWidgetClass && SlotWidgets.Num() != Count)
	{
		SlotRow->ClearChildren();
		SlotWidgets.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			UVaesenQuickSlotWidget* SlotWidget = CreateWidget<UVaesenQuickSlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			UPanelSlot* PanelSlot = SlotRow->AddChild(SlotWidget);
			if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot); BoxSlot && Index > 0)
			{
				BoxSlot->SetPadding(FMargin(SlotSpacing, 0.0f, 0.0f, 0.0f));
			}
			SlotWidgets.Add(SlotWidget);
		}
	}

	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		SlotWidgets[Index]->SetSlot(QuickSlots, Index);
	}
}
