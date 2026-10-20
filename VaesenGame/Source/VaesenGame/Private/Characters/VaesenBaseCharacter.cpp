#include "Characters/VaesenBaseCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenItemTypes.h"
#include "Abilities/VaesenGeneralAbilities.h"
#include "Abilities/VaesenMeleeAbilities.h"
#include "Abilities/VaesenRangedAbilities.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayEffects.h"
#include "Combat/VaesenGameplayTags.h"
#include "Items/VaesenWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

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

	// Combat: health, stamina, protection, and the abilities (granted in BeginPlay on the server).
	AbilitySystem = CreateDefaultSubobject<UVaesenAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<UVaesenAttributeSet>(TEXT("Attributes"));
	DefaultAbilities = {
		UVaesenGA_FireWeapon::StaticClass(), UVaesenGA_Reload::StaticClass(), UVaesenGA_CycleAction::StaticClass(),
		UVaesenGA_Aim::StaticClass(), UVaesenGA_DrawBow::StaticClass(), UVaesenGA_MeleeLight::StaticClass(),
		UVaesenGA_MeleeHeavy::StaticClass(), UVaesenGA_Block::StaticClass(), UVaesenGA_Throw::StaticClass(),
		UVaesenGA_Holster::StaticClass(), UVaesenGA_SwapWeapon::StaticClass(),
	};
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

// --- Combat ---

UAbilitySystemComponent* AVaesenBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AVaesenBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;

	AbilitySystem->InitAbilityActorInfo(this, this);
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ VaesenTags::State_Blocking, VaesenTags::State_Aiming, VaesenTags::State_DrawingBow,
		VaesenTags::State_Throwing, VaesenTags::State_Staggered })
	{
		AbilitySystem->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AVaesenBaseCharacter::OnCombatStateChanged);
	}

	if (HasAuthority())
	{
		// Melee traces and montage notifies need the bones to move on the server too, even where nobody sees them.
		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		if (!bAbilitiesGranted)
		{
			bAbilitiesGranted = true;
			for (const TSubclassOf<UVaesenGameplayAbility>& AbilityClass : DefaultAbilities)
			{
				if (AbilityClass)
				{
					const UVaesenGameplayAbility* Defaults = AbilityClass->GetDefaultObject<UVaesenGameplayAbility>();
					AbilitySystem->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, static_cast<int32>(Defaults->InputBinding), this));
				}
			}
			const FGameplayEffectSpecHandle Regen = AbilitySystem->MakeOutgoingSpec(UVaesenGE_StaminaRegen::StaticClass(), 1.0f, AbilitySystem->MakeEffectContext());
			if (Regen.IsValid())
			{
				AbilitySystem->ApplyGameplayEffectSpecToSelf(*Regen.Data.Get());
			}
		}
	}
}

void AVaesenBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// The controller is part of the actor info (abilities read the player's camera through it).
	AbilitySystem->InitAbilityActorInfo(this, this);
}

void AVaesenBaseCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	AbilitySystem->InitAbilityActorInfo(this, this);
}

bool AVaesenBaseCharacter::WantsToFaceAim() const
{
	static const FGameplayTagContainer FacingStates = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		VaesenTags::State_Aiming, VaesenTags::State_Blocking, VaesenTags::State_DrawingBow, VaesenTags::State_Throwing });
	return AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(FacingStates);
}

void AVaesenBaseCharacter::UpdateRotationMode()
{
	const bool bFaceAim = WantsToFaceAim();
	bUseControllerRotationYaw = bFaceAim;
	GetCharacterMovement()->bOrientRotationToMovement = !bFaceAim;
}

void AVaesenBaseCharacter::OnCombatStateChanged(const FGameplayTag Tag, int32 NewCount)
{
	// Slower while guarding, aiming or off balance (the slowest wins); same on the owner and the server.
	float Scale = 1.0f;
	if (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Blocking))
	{
		Scale = FMath::Min(Scale, BlockingSpeedScale);
	}
	if (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Aiming) || AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_DrawingBow)
		|| AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Throwing))
	{
		Scale = FMath::Min(Scale, AimingSpeedScale);
	}
	if (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Staggered))
	{
		Scale = FMath::Min(Scale, StaggeredSpeedScale);
	}
	if (BaseWalkSpeed > 0.0f)
	{
		GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * Scale;
	}
	UpdateRotationMode();

	// Parried: the swing stops, and the stagger plays.
	if (Tag == VaesenTags::State_Staggered && NewCount > 0)
	{
		const FGameplayTagContainer Attacks(VaesenTags::Ability_Attack);
		AbilitySystem->CancelAbilities(&Attacks);

		const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
		UAnimMontage* Stagger = Weapon ? Weapon->GetMontage(&FVaesenWeaponMontages::Stagger)
			: UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).Montages.Stagger.Get();
		if (Stagger)
		{
			PlayAnimMontage(Stagger);
		}
	}
}

void AVaesenBaseCharacter::HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::Executed && GameplayCueTag.MatchesTag(VaesenTags::GameplayCue_Hit))
	{
		FHitResult Hit;
		if (const FHitResult* ContextHit = Parameters.EffectContext.GetHitResult())
		{
			Hit = *ContextHit;
		}
		else
		{
			Hit.ImpactPoint = Parameters.Location;
			Hit.ImpactNormal = Parameters.Normal;
		}
		if (Parameters.PhysicalMaterial.IsValid())
		{
			Hit.PhysMaterial = const_cast<UPhysicalMaterial*>(Parameters.PhysicalMaterial.Get());
		}
		const FGameplayTag DamageType = Parameters.AggregatedSourceTags.Num() > 0 ? Parameters.AggregatedSourceTags.First() : FGameplayTag();
		const bool bReduced = GameplayCueTag.MatchesTagExact(VaesenTags::GameplayCue_Hit_Blocked) || GameplayCueTag.MatchesTagExact(VaesenTags::GameplayCue_Hit_Parried);
		if (!bReduced)
		{
			UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit);
		}
		if (!bReduced && Parameters.RawMagnitude > 0.0f)
		{
			PlayVocal(true);
		}
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

void AVaesenBaseCharacter::PlayVocal(bool bPain)
{
	USoundBase* Sound = bPain ? PainSound.Get() : EffortSound.Get();
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetMesh(), TEXT("head"));
	}
}
