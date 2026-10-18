#include "Items/VaesenEquippableItem.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

UVaesenEquippableItem::UVaesenEquippableItem()
{
	// One per stack: each is worn or held on its own.
	bStackable = false;
	MaxStackSize = 1;
}

void UVaesenEquippableItem::Equip(AVaesenPlayerCharacter* Character)
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
	if (AVaesenPlayerCharacter* Character = EquippedCharacter.Get())
	{
		OnUnequipped(Character);
	}
	EquippedCharacter.Reset();
}

void UVaesenEquippableItem::OnEquipped_Implementation(AVaesenPlayerCharacter* Character)
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

void UVaesenEquippableItem::OnUnequipped_Implementation(AVaesenPlayerCharacter* Character)
{
	if (UStaticMeshComponent* Component = EquippedMeshComponent.Get())
	{
		Component->DestroyComponent();
	}
	EquippedMeshComponent.Reset();
}
