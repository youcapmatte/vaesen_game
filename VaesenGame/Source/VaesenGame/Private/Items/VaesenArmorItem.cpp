#include "Items/VaesenArmorItem.h"

#include "Engine/SkeletalMesh.h"

UVaesenArmorItem::UVaesenArmorItem()
{
	Slot = EVaesenEquipmentSlot::Chest;
	AttachSocket = NAME_None;
}

#if WITH_EDITOR
void UVaesenArmorItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Armor only goes in an armor slot.
	if (!IsArmorSlot(Slot))
	{
		Slot = EVaesenEquipmentSlot::Chest;
	}
}
#endif
