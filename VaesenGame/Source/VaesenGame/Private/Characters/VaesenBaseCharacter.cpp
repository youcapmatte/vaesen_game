#include "Characters/VaesenBaseCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenItemTypes.h"

AVaesenBaseCharacter::AVaesenBaseCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Characters turn toward their movement direction rather than following the controller's rotation.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);

	HeadMesh = CreateBodyPart(TEXT("HeadMesh"));
	HandsMesh = CreateBodyPart(TEXT("HandsMesh"));
	LegsMesh = CreateBodyPart(TEXT("LegsMesh"));
	FeetMesh = CreateBodyPart(TEXT("FeetMesh"));
	BackMesh = CreateBodyPart(TEXT("BackMesh"));

	Equipment = CreateDefaultSubobject<UVaesenEquipmentComponent>(TEXT("Equipment"));
	// Item subobjects (equipped items, and inventories on subclasses) replicate through the registered list.
	bReplicateUsingRegisteredSubObjectList = true;
}

USkeletalMeshComponent* AVaesenBaseCharacter::CreateBodyPart(FName Name)
{
	// Sits exactly on the torso (Mesh) and copies its pose; looks only, no collision of its own.
	USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(Name);
	Part->SetupAttachment(GetMesh());
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);
	Part->SetCanEverAffectNavigation(false);
	Part->bReceivesDecals = GetMesh()->bReceivesDecals;
	return Part;
}

void AVaesenBaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	for (USkeletalMeshComponent* Part : { HeadMesh.Get(), HandsMesh.Get(), LegsMesh.Get(), FeetMesh.Get(), BackMesh.Get() })
	{
		if (Part)
		{
			Part->SetLeaderPoseComponent(GetMesh());
		}
	}
}

TArray<USkeletalMeshComponent*> AVaesenBaseCharacter::GetBodyParts() const
{
	return { GetMesh(), HeadMesh.Get(), HandsMesh.Get(), LegsMesh.Get(), FeetMesh.Get(), BackMesh.Get() };
}

USkeletalMeshComponent* AVaesenBaseCharacter::GetBodyPartFor(EVaesenEquipmentSlot Slot) const
{
	switch (Slot)
	{
	case EVaesenEquipmentSlot::Head:  return HeadMesh;
	case EVaesenEquipmentSlot::Chest: return GetMesh();
	case EVaesenEquipmentSlot::Hands: return HandsMesh;
	case EVaesenEquipmentSlot::Legs:  return LegsMesh;
	case EVaesenEquipmentSlot::Feet:  return FeetMesh;
	case EVaesenEquipmentSlot::Back:  return BackMesh;
	default:                          return nullptr;
	}
}
