#include "Items/VaesenEquippableItem.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

#define LOCTEXT_NAMESPACE "VaesenItems"

UVaesenEquippableItem::UVaesenEquippableItem()
{
	// One per stack: each is worn or held on its own.
	bStackable = false;
	MaxStackSize = 1;
	UseActionText = LOCTEXT("EquipUse", "Equip");
}

#undef LOCTEXT_NAMESPACE

AVaesenBaseCharacter* UVaesenEquippableItem::GetEquippedCharacter() const
{
	return EquippedCharacter.Get();
}

bool UVaesenEquippableItem::CanUse_Implementation(const AVaesenPlayerCharacter* Character) const
{
	const UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr;
	return Equipment && Equipment->CanEquip(this);
}

void UVaesenEquippableItem::Use_Implementation(AVaesenPlayerCharacter* Character)
{
	if (UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr)
	{
		Equipment->EquipFromInventory(GetOwningInventory(), this);
	}
}

void UVaesenEquippableItem::Equip(AVaesenBaseCharacter* Character)
{
	if (!Character || EquippedCharacter.Get() == Character)
	{
		return;
	}
	Unequip();
	EquippedCharacter = Character;
	OnEquipped(Character);
}

void UVaesenEquippableItem::Unequip()
{
	if (AVaesenBaseCharacter* Character = EquippedCharacter.Get())
	{
		OnUnequipped(Character);
	}
	EquippedCharacter.Reset();
}

void UVaesenEquippableItem::OnEquipped_Implementation(AVaesenBaseCharacter* Character)
{
	if (!EquippedMesh)
	{
		return;
	}

	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Character);
	Component->SetStaticMesh(EquippedMesh);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetupAttachment(Character->GetMesh(), AttachSocket);
	Component->RegisterComponent();
	EquippedMeshComponent = Component;
}

void UVaesenEquippableItem::OnUnequipped_Implementation(AVaesenBaseCharacter* Character)
{
	if (UStaticMeshComponent* Component = EquippedMeshComponent.Get())
	{
		Component->DestroyComponent();
	}
	EquippedMeshComponent.Reset();
}
