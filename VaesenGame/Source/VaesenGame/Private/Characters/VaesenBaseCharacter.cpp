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
#include "Items/VaesenMeleeWeaponItem.h"
#include "Items/VaesenWeaponItem.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/VaesenWeaponActor.h"
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
	// Crouching (C): the movement component shrinks the capsule and replicates it; the anim graphs crouch.
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 150.f;

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
		UVaesenGA_Holster::StaticClass(), UVaesenGA_SwapWeapon::StaticClass(), UVaesenGA_Dodge::StaticClass(),
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

		// The bodies answer: a flinch, or the guard beating the blow aside (and a blocked swing bouncing off).
		AActor* Attacker = Parameters.EffectContext.GetInstigator();
		if (!bReduced && Parameters.RawMagnitude > 0.0f)
		{
			PlayHitReaction(Attacker ? Attacker->GetActorLocation() : FVector(Parameters.Location));
		}
		else if (bReduced)
		{
			PlayBlockImpact();
			if (AVaesenBaseCharacter* Swinger = Cast<AVaesenBaseCharacter>(Attacker);
				Swinger && Swinger != this && GameplayCueTag.MatchesTagExact(VaesenTags::GameplayCue_Hit_Blocked))
			{
				Swinger->PlaySwingBlocked();
			}
		}
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}

namespace
{
	/** One of Montages at random (null if there are none). */
	UAnimMontage* PickOne(const TArray<TObjectPtr<UAnimMontage>>& Montages)
	{
		return Montages.Num() > 0 ? Montages[FMath::RandRange(0, Montages.Num() - 1)].Get() : nullptr;
	}
}

void AVaesenBaseCharacter::PlayHitReaction(const FVector& From)
{
	static const FGameplayTagContainer Busy = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		VaesenTags::State_Attacking, VaesenTags::State_Blocking, VaesenTags::State_Dodging, VaesenTags::State_Dead });
	if (GetNetMode() == NM_DedicatedServer || (AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(Busy)))
	{
		return;
	}
	const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
	const FVaesenWeaponMontages& Montages = Weapon ? Weapon->GetCategoryDefaults().Montages
		: UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).Montages;
	const bool bFromBehind = FVector::DotProduct((From - GetActorLocation()).GetSafeNormal2D(), GetActorForwardVector()) < -0.3f;
	UAnimMontage* Reaction = bFromBehind && Montages.HitReactionBehind ? Montages.HitReactionBehind.Get() : PickOne(Montages.HitReactions);
	if (Reaction)
	{
		PlayAnimMontage(Reaction);
	}
}

void AVaesenBaseCharacter::PlayBlockImpact()
{
	const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
	if (UAnimMontage* Impact = Weapon && GetNetMode() != NM_DedicatedServer ? PickOne(Weapon->GetCategoryDefaults().Montages.BlockImpacts) : nullptr)
	{
		PlayAnimMontage(Impact);
	}
}

void AVaesenBaseCharacter::PlaySwingBlocked()
{
	const UVaesenMeleeWeaponItem* Weapon = Equipment ? Cast<UVaesenMeleeWeaponItem>(Equipment->GetDrawnWeapon()) : nullptr;
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Weapon || !Anim)
	{
		return;
	}
	// The swing playing now (it may already be stopping: the server cancels the attack), bounced off.
	const UAnimMontage* Swing = Anim->GetCurrentActiveMontage();
	for (const FAnimMontageInstance* Instance : Anim->MontageInstances)
	{
		if (Instance && Instance->Montage && Weapon->GetBlockedMontage(Instance->Montage))
		{
			Swing = Instance->Montage;
		}
	}
	if (UAnimMontage* Bounce = Weapon->GetBlockedMontage(Swing))
	{
		// From as far into the bounce as the swing had got (they start alike).
		float Position = 0.0f;
		if (const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Swing))
		{
			Position = Instance->GetPosition();
		}
		Anim->Montage_Play(Bounce, 1.0f, EMontagePlayReturnType::MontageLength, FMath::Min(Position, Bounce->GetPlayLength() * 0.5f));
	}
}

void AVaesenBaseCharacter::PlayVocal(bool bPain)
{
	USoundBase* Sound = bPain ? PainSound.Get() : EffortSound.Get();
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetMesh(), TEXT("head"));
	}
}

#if WITH_EDITOR
bool AVaesenBaseCharacter::ReplaceDefaultAbility(UClass* From, UClass* To)
{
	bool bReplaced = false;
	for (TSubclassOf<UVaesenGameplayAbility>& Ability : DefaultAbilities)
	{
		if (Ability.Get() == From && To && To->IsChildOf(UVaesenGameplayAbility::StaticClass()))
		{
			Ability = To;
			bReplaced = true;
		}
	}
	// One added since the Blueprint saved its list (the dodge): granted too.
	if (!bReplaced && To && To->IsChildOf(UVaesenGameplayAbility::StaticClass()) && !DefaultAbilities.Contains(To))
	{
		DefaultAbilities.Add(To);
		bReplaced = true;
	}
	return bReplaced;
}
#endif

void AVaesenBaseCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Gear rattle without footstep notifies: by distance walked on the ground.
	if (GetNetMode() == NM_DedicatedServer || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	GearRattleDistance += GetVelocity().Size2D() * DeltaSeconds;
	if (GearRattleDistance >= GearRattleStride)
	{
		GearRattleDistance = 0.0f;
		if (GetWorld()->GetTimeSeconds() - LastFootstepNotifyTime > 1.0)
		{
			PlayGearRattle();
		}
	}
}

void AVaesenBaseCharacter::OnFootstep()
{
	LastFootstepNotifyTime = GetWorld()->GetTimeSeconds();
	PlayGearRattle();
}

void AVaesenBaseCharacter::PlayGearRattle()
{
	if (!Equipment || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const float MaxSpeed = FMath::Max(GetCharacterMovement()->MaxWalkSpeed, 1.0f);
	const float Volume = FMath::Clamp(GetVelocity().Size2D() / MaxSpeed, 0.3f, 1.0f);
	for (const FVaesenEquippedItem& Entry : Equipment->GetEquippedItems())
	{
		const UVaesenWeaponItem* Weapon = Cast<UVaesenWeaponItem>(Entry.Item);
		AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr;
		USoundBase* Rattle = Weapon && !Equipment->IsInHand(Entry.Slot) ? Weapon->GetSound(EVaesenWeaponSound::GearRattle) : nullptr;
		if (Actor && Rattle)
		{
			UGameplayStatics::SpawnSoundAttached(Rattle, Actor->GetRootComponent(), NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, Volume);
		}
	}
}
