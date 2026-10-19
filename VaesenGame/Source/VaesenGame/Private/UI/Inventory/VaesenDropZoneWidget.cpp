#include "UI/Inventory/VaesenDropZoneWidget.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/Border.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "UI/Inventory/VaesenItemDragDropOperation.h"

bool UVaesenDropZoneWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (ZoneBorder)
	{
		ZoneBorder->SetBrushColor(NormalColor);
	}

	const UVaesenItemDragDropOperation* Operation = Cast<UVaesenItemDragDropOperation>(InOperation);
	const AVaesenPlayerCharacter* Character = GetOwningPlayerPawn<AVaesenPlayerCharacter>();
	if (!Operation || !Character)
	{
		return false;
	}

	if (Operation->bFromEquipment)
	{
		if (UVaesenEquipmentComponent* Equipment = Character->GetEquipment())
		{
			Equipment->RequestDropEquipped(Operation->EquipmentSlot);
		}
	}
	else if (UVaesenInventoryComponent* PlayerInventory = Character->GetInventory())
	{
		PlayerInventory->RequestDropItem(Operation->SourceInventory, Operation->SourceIndex, Operation->Amount);
	}
	return true;
}

void UVaesenDropZoneWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	if (ZoneBorder && Cast<UVaesenItemDragDropOperation>(InOperation))
	{
		ZoneBorder->SetBrushColor(HoveredColor);
	}
}

void UVaesenDropZoneWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (ZoneBorder)
	{
		ZoneBorder->SetBrushColor(NormalColor);
	}
}
