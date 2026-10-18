#include "Items/VaesenArmorItem.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"

UVaesenArmorItem::UVaesenArmorItem()
{
	Slot = EVaesenEquipmentSlot::Torso;
	AttachSocket = NAME_None;
}

#if WITH_EDITOR
void UVaesenArmorItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Armor only goes on the body.
	if (!IsBodySlot(Slot))
	{
		Slot = EVaesenEquipmentSlot::Torso;
	}
}
#endif

USkeletalMeshComponent* UVaesenArmorItem::GetBodyPart(const AVaesenPlayerCharacter* Character) const
{
	if (!Character)
	{
		return nullptr;
	}

	switch (Slot)
	{
	case EVaesenEquipmentSlot::Head:  return Character->GetHeadMesh();
	case EVaesenEquipmentSlot::Torso: return Character->GetTorsoMesh();
	case EVaesenEquipmentSlot::Arms:  return Character->GetArmsMesh();
	case EVaesenEquipmentSlot::Legs:  return Character->GetLegsMesh();
	case EVaesenEquipmentSlot::Feet:  return Character->GetFeetMesh();
	default:                          return nullptr;
	}
}

void UVaesenArmorItem::OnEquipped_Implementation(AVaesenPlayerCharacter* Character)
{
	Super::OnEquipped_Implementation(Character);

	USkeletalMeshComponent* BodyPart = GetBodyPart(Character);
	if (BodyPart && ArmorMesh)
	{
		PreviousMesh = BodyPart->GetSkeletalMeshAsset();
		BodyPart->SetSkeletalMeshAsset(ArmorMesh);
	}
}

void UVaesenArmorItem::OnUnequipped_Implementation(AVaesenPlayerCharacter* Character)
{
	USkeletalMeshComponent* BodyPart = GetBodyPart(Character);
	if (BodyPart && BodyPart->GetSkeletalMeshAsset() == ArmorMesh)
	{
		BodyPart->SetSkeletalMeshAsset(PreviousMesh.Get());
	}
	PreviousMesh.Reset();

	Super::OnUnequipped_Implementation(Character);
}
