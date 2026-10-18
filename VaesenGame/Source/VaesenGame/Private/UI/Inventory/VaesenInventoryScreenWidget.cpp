#include "UI/Inventory/VaesenInventoryScreenWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Player/VaesenPlayerController.h"
#include "UI/Inventory/VaesenInventoryPanelWidget.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"

void UVaesenInventoryScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Takes keyboard focus, for Tab / I / Escape.
	SetIsFocusable(true);
}

void UVaesenInventoryScreenWidget::SetInventories(UVaesenInventoryComponent* PlayerInventory, UVaesenInventoryComponent* Container)
{
	PlayerInventoryShown = PlayerInventory;
	ContainerShown = Container;

	if (PlayerPanel)
	{
		PlayerPanel->SetInventory(PlayerInventory);
		PlayerPanel->SetQuickTransferTarget(Container);
	}
	if (ContainerPanel)
	{
		ContainerPanel->SetInventory(Container);
		ContainerPanel->SetQuickTransferTarget(PlayerInventory);
		ContainerPanel->SetVisibility(Container ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UVaesenInventoryScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Walked away from the chest (or it's gone): just the player's side again.
	const bool bHadContainer = ContainerPanel && ContainerPanel->GetInventory() != nullptr;
	if (bHadContainer)
	{
		const UVaesenInventoryComponent* Container = ContainerShown.Get();
		const AVaesenPlayerCharacter* Character = GetOwningPlayerPawn<AVaesenPlayerCharacter>();
		if (!Container || !Container->CanBeAccessedBy(Character))
		{
			SetInventories(PlayerInventoryShown.Get(), nullptr);
		}
	}
}

FReply UVaesenInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Tab || Key == EKeys::I || Key == EKeys::Escape)
	{
		if (AVaesenPlayerController* Controller = Cast<AVaesenPlayerController>(GetOwningPlayer()))
		{
			Controller->CloseInventory();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UVaesenInventoryScreenWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// Outside the panels: on the ground.
	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	UVaesenInventoryComponent* PlayerInventory = PlayerInventoryShown.Get();
	if (!Operation || !PlayerInventory)
	{
		return false;
	}
	PlayerInventory->RequestDropItem(Operation->SourceInventory, Operation->SourceIndex, Operation->Amount);
	return true;
}
