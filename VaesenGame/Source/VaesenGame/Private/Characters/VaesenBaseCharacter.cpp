#include "Characters/VaesenBaseCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VaesenAttributeSet.h"
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
	HeadOverMesh = CreateBodyPart(TEXT("HeadOverMesh"));
	ChestOverMesh = CreateBodyPart(TEXT("ChestOverMesh"));
	HandsOverMesh = CreateBodyPart(TEXT("HandsOverMesh"));
	LegsOverMesh = CreateBodyPart(TEXT("LegsOverMesh"));
	FeetOverMesh = CreateBodyPart(TEXT("FeetOverMesh"));

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

	for (USkeletalMeshComponent* Part : GetBodyParts())
	{
		if (Part && Part != GetMesh())
		{
			Part->SetLeaderPoseComponent(GetMesh());
		}
	}
}

TArray<USkeletalMeshComponent*> AVaesenBaseCharacter::GetBodyParts() const
{
	return { GetMesh(), HeadMesh.Get(), HandsMesh.Get(), LegsMesh.Get(), FeetMesh.Get(), BackMesh.Get(),
		HeadOverMesh.Get(), ChestOverMesh.Get(), HandsOverMesh.Get(), LegsOverMesh.Get(), FeetOverMesh.Get() };
}

USkeletalMeshComponent* AVaesenBaseCharacter::GetWornOverLayerFor(EVaesenEquipmentSlot Slot) const
{
	switch (Slot)
	{
	case EVaesenEquipmentSlot::Head:  return HeadOverMesh;
	case EVaesenEquipmentSlot::Chest: return ChestOverMesh;
	case EVaesenEquipmentSlot::Hands: return HandsOverMesh;
	case EVaesenEquipmentSlot::Legs:  return LegsOverMesh;
	case EVaesenEquipmentSlot::Feet:  return FeetOverMesh;
	default:                          return nullptr;
	}
}

UAbilitySystemComponent* AVaesenBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem.Get();
}

UVaesenAttributeSet* AVaesenBaseCharacter::GetAttributes() const
{
	const UAbilitySystemComponent* System = AbilitySystem.Get();
	return System ? const_cast<UVaesenAttributeSet*>(System->GetSet<UVaesenAttributeSet>()) : nullptr;
}

void AVaesenBaseCharacter::InitAbilitySystem(UAbilitySystemComponent* InAbilitySystem, AActor* OwnerActor)
{
	if (!InAbilitySystem)
	{
		return;
	}
	AbilitySystem = InAbilitySystem;
	InAbilitySystem->InitAbilityActorInfo(OwnerActor, this);

	if (!HasAuthority())
	{
		return;
	}
	for (const TSubclassOf<UGameplayAbility>& Ability : StartupAbilities)
	{
		if (Ability && !InAbilitySystem->FindAbilitySpecFromClass(Ability))
		{
			InAbilitySystem->GiveAbility(FGameplayAbilitySpec(Ability, 1, INDEX_NONE, this));
		}
	}
	// Once per ability system: a player's outlasts their character.
	static const FName StartupEffectsAppliedTag(TEXT("VaesenStartupEffectsApplied"));
	if (!InAbilitySystem->ComponentTags.Contains(StartupEffectsAppliedTag))
	{
		InAbilitySystem->ComponentTags.Add(StartupEffectsAppliedTag);
		FGameplayEffectContextHandle Context = InAbilitySystem->MakeEffectContext();
		Context.AddSourceObject(this);
		for (const TSubclassOf<UGameplayEffect>& Effect : StartupEffects)
		{
			if (Effect)
			{
				const FGameplayEffectSpecHandle Spec = InAbilitySystem->MakeOutgoingSpec(Effect, 1.0f, Context);
				if (Spec.IsValid())
				{
					InAbilitySystem->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
				}
			}
		}
	}
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
