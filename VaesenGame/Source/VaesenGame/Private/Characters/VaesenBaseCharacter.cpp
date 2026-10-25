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
#include "AbilitySystemGlobals.h"
#include "AIController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

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
	return LockTarget || (AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(FacingStates));
}

bool AVaesenBaseCharacter::CanJumpInternal_Implementation() const
{
	static const FGameplayTagContainer AimingStates = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		VaesenTags::State_Aiming, VaesenTags::State_DrawingBow, VaesenTags::State_Throwing });
	if (LockTarget || (AbilitySystem && AbilitySystem->HasAnyMatchingGameplayTags(AimingStates)))
	{
		return false;
	}
	return Super::CanJumpInternal_Implementation();
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
			UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit, Parameters.AggregatedTargetTags.HasTagExact(VaesenTags::Data_Projectile)
			? EVaesenImpactDelivery::Shot : EVaesenImpactDelivery::ByHand);
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

	// A lock lets go when it can't hold: the target fell or got away, or a gun or bow was drawn.
	if (LockTarget && (HasAuthority() || IsLocallyControlled()) && (!IsValid(LockTarget) || !CanLockOnto(LockTarget, LockBreakRange)))
	{
		SetLockTarget(nullptr);
	}

	// Locked: a player's view turns to the target (the camera, and with it the strafing body). An AI looks at it (focus).
	if (const APlayerController* Player = LockTarget && IsLocallyControlled() ? Cast<APlayerController>(Controller) : nullptr)
	{
		const FVector From = Player->PlayerCameraManager ? Player->PlayerCameraManager->GetCameraLocation() : GetPawnViewLocation();
		FRotator Desired = (GetLockPoint(LockTarget) - From).Rotation();
		// A little above the target, so the fighter stays in view below it.
		Desired.Pitch = FMath::Clamp(Desired.Pitch - 8.0f, -35.0f, 20.0f);
		Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaSeconds, 10.0f));
	}

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

// --- Target lock ---

void AVaesenBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVaesenBaseCharacter, LockTarget);
}

FVector AVaesenBaseCharacter::GetLockPoint(const AActor* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	// The chest: a little above the middle of what it's made of.
	const FBox Bounds = Target->GetComponentsBoundingBox(true);
	return Bounds.IsValid ? Bounds.GetCenter() + FVector(0.0, 0.0, Bounds.GetExtent().Z * 0.3) : Target->GetActorLocation();
}

bool AVaesenBaseCharacter::CanLockOnto(const AActor* Target, float Range) const
{
	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!Target || Target == this || !TargetASC || TargetASC->HasMatchingGameplayTag(VaesenTags::State_Dead))
	{
		return false;
	}
	if (FVector::Dist(GetActorLocation(), Target->GetActorLocation()) > Range)
	{
		return false;
	}
	// Melee weapons and bare hands only: a gun or bow aims instead.
	const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
	return !Weapon || !Weapon->IsRanged();
}

AActor* AVaesenBaseCharacter::FindLockTarget(const FVector& ViewOrigin, const FVector& ViewDirection) const
{
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	const FVector Forward = ViewDirection.GetSafeNormal();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!CanLockOnto(Candidate, LockRange))
		{
			continue;
		}
		const FVector Point = GetLockPoint(Candidate);
		const FVector ToIt = Point - ViewOrigin;
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToIt.GetSafeNormal(), Forward), -1.0f, 1.0f)));
		if (Angle > LockAngle)
		{
			continue;
		}
		// In sight: nothing else in the way.
		FHitResult Blocker;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenLockSight), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Blocker, ViewOrigin, Point, ECC_Visibility, Params) && Blocker.GetActor() != Candidate)
		{
			continue;
		}
		// Nearest the middle of the view first, then the nearest (a degree weighs as much as a metre and a half).
		const float Score = Angle + ToIt.Size() / 150.0f;
		if (Score < BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
	}
	return Best;
}

void AVaesenBaseCharacter::ToggleTargetLock()
{
	AActor* Target = nullptr;
	if (!LockTarget)
	{
		FVector ViewOrigin;
		FRotator ViewRotation;
		GetActorEyesViewPoint(ViewOrigin, ViewRotation);
		if (const APlayerController* Player = Cast<APlayerController>(GetController()); Player && Player->PlayerCameraManager)
		{
			ViewOrigin = Player->PlayerCameraManager->GetCameraLocation();
			ViewRotation = Player->PlayerCameraManager->GetCameraRotation();
		}
		Target = FindLockTarget(ViewOrigin, ViewRotation.Vector());
		if (!Target)
		{
			return;
		}
	}
	SetLockTarget(Target);
	if (!HasAuthority())
	{
		ServerSetLockTarget(Target);
	}
}

void AVaesenBaseCharacter::ServerSetLockTarget_Implementation(AActor* Target)
{
	// The server's own check (a little further: the client saw it a moment ago).
	SetLockTarget(Target && CanLockOnto(Target, LockRange * 1.2f) ? Target : nullptr);
}

void AVaesenBaseCharacter::SetLockTarget(AActor* Target)
{
	if (LockTarget == Target)
	{
		return;
	}
	LockTarget = Target;
	OnRep_LockTarget();
}

void AVaesenBaseCharacter::OnRep_LockTarget()
{
	// Facing the target (strafing) or the way it walks; the weapon's stance follows (it may have a locked layer).
	UpdateRotationMode();
	if (AAIController* AI = HasAuthority() ? Cast<AAIController>(Controller) : nullptr)
	{
		if (LockTarget)
		{
			AI->SetFocus(LockTarget);
		}
		else
		{
			AI->ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
	if (Equipment)
	{
		Equipment->RefreshDrawnLook();
	}
}
