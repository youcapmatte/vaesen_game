#include "Abilities/VaesenMeleeAbilities.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "TimerManager.h"
#include "Weapons/VaesenWeaponActor.h"
#include "Weapons/VaesenWeaponData.h"

// --- Attacks ---

UVaesenGA_MeleeAttack::UVaesenGA_MeleeAttack()
{
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Melee, VaesenTags::Ability_Attack, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Attacking);
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ VaesenTags::State_Attacking, VaesenTags::State_SwitchingWeapon, VaesenTags::State_Reloading,
		VaesenTags::State_Cycling, VaesenTags::State_Throwing, VaesenTags::State_DrawingBow })
	{
		ActivationBlockedTags.AddTag(Tag);
	}
	// Attacking out of a block drops the block.
	CancelAbilitiesWithTag.AddTag(VaesenTags::Ability_Block);
}

UVaesenGA_MeleeLight::UVaesenGA_MeleeLight()
{
	InputBinding = EVaesenAbilityInput::Attack;
	bHeavy = false;
}

UVaesenGA_MeleeHeavy::UVaesenGA_MeleeHeavy()
{
	InputBinding = EVaesenAbilityInput::HeavyAttack;
	bHeavy = true;
}

bool UVaesenGA_MeleeAttack::ResolveAttack(const FGameplayAbilityActorInfo* ActorInfo, FVaesenMeleeAttack& OutAttack) const
{
	const UVaesenWeaponItem* Weapon = GetDrawnWeapon(ActorInfo);
	const UVaesenWeaponCategoryDefaults* Defaults = UVaesenWeaponCategoryDefaults::Get();
	if (!Weapon)
	{
		// Bare hands are always there.
		const FVaesenCategoryDefaults& Unarmed = Defaults->GetCategory(EVaesenWeaponCategory::Unarmed);
		if (bHeavy && !Unarmed.bHasHeavyAttack)
		{
			return false;
		}
		OutAttack = bHeavy ? Unarmed.HeavyAttack : Unarmed.LightAttack;
		if (!OutAttack.Montage)
		{
			OutAttack.Montage = Unarmed.Montages.Attack;
		}
		return true;
	}
	if (Weapon->IsBroken())
	{
		return false;
	}

	if (const UVaesenMeleeWeaponItem* Melee = Cast<UVaesenMeleeWeaponItem>(Weapon))
	{
		if (!Melee->GetAttack(bHeavy, OutAttack))
		{
			return false;
		}
	}
	else
	{
		// A gun or bow: left mouse fires, so only the heavy attack strikes (the butt, the stock).
		if (!bHeavy)
		{
			return false;
		}
		OutAttack = Weapon->GetCategoryDefaults().LightAttack;
	}
	if (!OutAttack.Montage)
	{
		OutAttack.Montage = Weapon->GetMontage(&FVaesenWeaponMontages::Attack);
	}

	// Further into a combo: that swing's montage instead.
	const int32 Step = GetNextComboStep();
	if (Step > 0 && OutAttack.ComboSteps.IsValidIndex(Step - 1) && OutAttack.ComboSteps[Step - 1].Montage)
	{
		OutAttack.Montage = OutAttack.ComboSteps[Step - 1].Montage;
		OutAttack.BlockedMontage = OutAttack.ComboSteps[Step - 1].BlockedMontage;
		OutAttack.MontageSection = NAME_None;
	}
	return true;
}

int32 UVaesenGA_MeleeAttack::GetNextComboStep() const
{
	const UWorld* World = GetWorld();
	const bool bInWindow = World && LastSwingOverTime >= 0.0 && World->GetTimeSeconds() - LastSwingOverTime <= ComboWindow;
	return bInWindow ? ComboStep : 0;
}

float UVaesenGA_MeleeAttack::GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const
{
	FVaesenMeleeAttack Resolved;
	return ResolveAttack(ActorInfo, Resolved) ? Resolved.StaminaCost : 0.0f;
}

bool UVaesenGA_MeleeAttack::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	FVaesenMeleeAttack Resolved;
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags) && ResolveAttack(ActorInfo, Resolved);
}

void UVaesenGA_MeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	const int32 Step = GetNextComboStep();
	if (!ResolveAttack(ActorInfo, Attack) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	bWindowOpen = false;
	// The swing after this one (round to the first again after the last).
	ComboStep = (Step + 1) % (Attack.ComboSteps.Num() + 1);
	ComboWindow = Attack.ComboWindow;
	LastSwingOverTime = -1.0;

	UAnimMontage* Montage = Attack.Montage;
	float Length = PlayAbilityMontage(Montage, 1.0f, Attack.MontageSection);
	if (Montage && Length > 0.0f && Attack.MontageSection != NAME_None)
	{
		const int32 Section = Montage->GetSectionIndex(Attack.MontageSection);
		Length = Section != INDEX_NONE ? Montage->GetSectionLength(Section) : Length;
	}

	AVaesenBaseCharacter* Character = GetVaesenCharacter(ActorInfo);
	if (HasAuthority(&ActivationInfo))
	{
		if (Length > 0.0f && UVaesenCombatLibrary::MontageHasHitWindow(Montage) && Character)
		{
			// The animation says when the blade is live.
			HitWindowHandle = Character->OnMeleeHitWindow.AddUObject(this, &UVaesenGA_MeleeAttack::OnHitWindowNotify);
		}
		else
		{
			GetTimers().SetTimer(WindowOpenTimer, this, &UVaesenGA_MeleeAttack::OpenHitWindow, FMath::Max(Attack.HitWindowStart, 0.01f), false);
			GetTimers().SetTimer(WindowCloseTimer, this, &UVaesenGA_MeleeAttack::CloseHitWindow, FMath::Max(Attack.HitWindowEnd, Attack.HitWindowStart + 0.02f), false);
		}
	}

	if (!Montage && IsLocallyControlled() && Character)
	{
		// No animation yet: still hear the swing (and the grunt on a heavy one).
		if (const UVaesenWeaponItem* Weapon = GetDrawnWeapon(ActorInfo); Weapon && Weapon->GetWeaponActor())
		{
			Weapon->GetWeaponActor()->PlayWeaponSound(bHeavy ? EVaesenWeaponSound::SwingHeavy : EVaesenWeaponSound::SwingLight);
		}
		if (bHeavy)
		{
			Character->PlayVocal(false);
		}
	}

	// Over at its recovery time (the follow-through plays out, or the next swing cuts it short), else at the end.
	const float SwingLength = Length > 0.0f && Attack.RecoveryTime > 0.0f ? FMath::Min(Length, Attack.RecoveryTime) : Length;
	const float Duration = (SwingLength > 0.0f ? SwingLength : Attack.Duration) + Attack.Cooldown;
	GetTimers().SetTimer(StepTimer, this, &UVaesenGA_MeleeAttack::FinishAttack, FMath::Max(Duration, 0.05f), false);
}

void UVaesenGA_MeleeAttack::OnHitWindowNotify(bool bOpen)
{
	if (bOpen)
	{
		OpenHitWindow();
	}
	else
	{
		CloseHitWindow();
	}
}

void UVaesenGA_MeleeAttack::OpenHitWindow()
{
	if (bWindowOpen || !HasAuthority(&CurrentActivationInfo))
	{
		return;
	}
	bWindowOpen = true;
	const UVaesenWeaponItem* Weapon = GetDrawnWeapon();
	if (AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr)
	{
		Actor->BeginHitWindow(Attack);
	}
	else
	{
		UnarmedStrike();
	}
}

void UVaesenGA_MeleeAttack::CloseHitWindow()
{
	if (!bWindowOpen)
	{
		return;
	}
	bWindowOpen = false;
	const UVaesenWeaponItem* Weapon = GetDrawnWeapon();
	if (AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr)
	{
		Actor->EndHitWindow();
	}
}

void UVaesenGA_MeleeAttack::UnarmedStrike()
{
	AVaesenBaseCharacter* Character = GetVaesenCharacter();
	if (!Character)
	{
		return;
	}
	const FVector Start = Character->GetActorLocation() + FVector(0.0, 0.0, 40.0);
	const FVector End = Start + Character->GetActorForwardVector() * Attack.Reach;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenUnarmed), false, Character);
	Params.bReturnPhysicalMaterial = true;
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(UnarmedRadius), Params);
	if (UVaesenCombatLibrary::IsCombatDebugEnabled())
	{
		DrawDebugCapsule(GetWorld(), (Start + End) * 0.5f, Attack.Reach * 0.5f, UnarmedRadius, FRotationMatrix::MakeFromZ(End - Start).ToQuat(), Hits.Num() ? FColor::Red : FColor::Green, false, 1.0f);
	}

	TSet<AActor*> Struck;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (Target && Target != Character && !Struck.Contains(Target))
		{
			Struck.Add(Target);
			UVaesenCombatLibrary::ApplyDamagePacket(Character, Target, Attack.Damage, Hit, nullptr, Attack.bCanBeBlocked, false, Attack.Knockback);
		}
	}
}

void UVaesenGA_MeleeAttack::FinishAttack()
{
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, false);
}

void UVaesenGA_MeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CloseHitWindow();
	if (AVaesenBaseCharacter* Character = GetVaesenCharacter(ActorInfo))
	{
		Character->OnMeleeHitWindow.Remove(HitWindowHandle);
	}
	HitWindowHandle.Reset();
	if (bWasCancelled)
	{
		// Staggered, blocked or sheathed mid-swing: the swing stops, and so does the combo.
		StopAbilityMontage();
		ComboStep = 0;
		LastSwingOverTime = -1.0;
	}
	else if (const UWorld* World = GetWorld())
	{
		LastSwingOverTime = World->GetTimeSeconds();
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --- Block ---

UVaesenGA_Block::UVaesenGA_Block()
{
	InputBinding = EVaesenAbilityInput::AimOrBlock;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Block, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Blocking);
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ VaesenTags::State_Attacking, VaesenTags::State_SwitchingWeapon, VaesenTags::State_Throwing })
	{
		ActivationBlockedTags.AddTag(Tag);
	}
}

bool UVaesenGA_Block::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	// Right mouse aims a gun instead.
	const UVaesenWeaponItem* Weapon = GetDrawnWeapon(ActorInfo);
	if (Weapon && Weapon->IsRanged())
	{
		return false;
	}
	UVaesenWeaponItem* BlockingItem = nullptr;
	return UVaesenCombatLibrary::FindBlockProfile(ActorInfo->AvatarActor.Get(), false, BlockingItem) != nullptr;
}

void UVaesenGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UVaesenWeaponItem* BlockingItem = nullptr;
	const FVaesenBlockProfile* Profile = UVaesenCombatLibrary::FindBlockProfile(ActorInfo->AvatarActor.Get(), false, BlockingItem);
	if (!Profile || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// The parry window: a hit landing within it is parried.
	if (Profile->ParryWindow > 0.0f)
	{
		ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(VaesenTags::State_ParryWindow);
		bParryWindowOpen = true;
		GetTimers().SetTimer(StepTimer, this, &UVaesenGA_Block::CloseParryWindow, Profile->ParryWindow, false);
	}

	StaminaPerSecond = Profile->StaminaPerSecond;
	if (HasAuthority(&ActivationInfo) && StaminaPerSecond > 0.0f)
	{
		GetTimers().SetTimer(RepeatTimer, this, &UVaesenGA_Block::DrainWhileHeld, 0.25f, true);
	}

	const UAnimMontage* BlockMontage = BlockingItem ? BlockingItem->GetMontage(&FVaesenWeaponMontages::Block)
		: UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).Montages.Block.Get();
	PlayAbilityMontage(const_cast<UAnimMontage*>(BlockMontage));
}

void UVaesenGA_Block::CloseParryWindow()
{
	if (bParryWindowOpen)
	{
		bParryWindowOpen = false;
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveLooseGameplayTag(VaesenTags::State_ParryWindow);
		}
	}
}

void UVaesenGA_Block::DrainWhileHeld()
{
	if (!DrainStamina(StaminaPerSecond * 0.25f))
	{
		// Out of stamina: the guard drops.
		EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
	}
}

void UVaesenGA_Block::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UVaesenGA_Block::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	CloseParryWindow();
	StopAbilityMontage();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// --- Dodge ---

UVaesenGA_Dodge::UVaesenGA_Dodge()
{
	InputBinding = EVaesenAbilityInput::Dodge;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Dodge }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Dodging);
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ VaesenTags::State_Dodging, VaesenTags::State_Attacking, VaesenTags::State_SwitchingWeapon,
		VaesenTags::State_Staggered, VaesenTags::State_Throwing, VaesenTags::State_Dead })
	{
		ActivationBlockedTags.AddTag(Tag);
	}
	// Dodging out of a guard or an aim drops it.
	CancelAbilitiesWithTag.AddTag(VaesenTags::Ability_Block);
	CancelAbilitiesWithTag.AddTag(VaesenTags::Ability_Aim);
}

UAnimMontage* UVaesenGA_Dodge::ChooseMontage(const AVaesenBaseCharacter* Character, const UVaesenWeaponItem* Weapon)
{
	if (!Character || !Weapon)
	{
		return nullptr;
	}
	// The way the player is steering (the server has the same from the client's moves); standing still steps back.
	const FVector Steer = Character->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	TObjectPtr<UAnimMontage> FVaesenWeaponMontages::* Which = &FVaesenWeaponMontages::DodgeBack;
	if (!Steer.IsNearlyZero())
	{
		const float Forward = FVector::DotProduct(Steer, Character->GetActorForwardVector());
		const float Right = FVector::DotProduct(Steer, Character->GetActorRightVector());
		Which = FMath::Abs(Forward) >= FMath::Abs(Right)
			? (Forward >= 0.0f ? &FVaesenWeaponMontages::DodgeForward : &FVaesenWeaponMontages::DodgeBack)
			: (Right >= 0.0f ? &FVaesenWeaponMontages::DodgeRight : &FVaesenWeaponMontages::DodgeLeft);
	}
	return Weapon->GetMontage(Which);
}

bool UVaesenGA_Dodge::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AVaesenBaseCharacter* Character = GetVaesenCharacter(ActorInfo);
	return Character && Character->GetCharacterMovement()->IsMovingOnGround() && ChooseMontage(Character, GetDrawnWeapon(ActorInfo)) != nullptr;
}

void UVaesenGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UAnimMontage* Montage = ChooseMontage(GetVaesenCharacter(ActorInfo), GetDrawnWeapon(ActorInfo));
	if (!Montage || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const float Length = PlayAbilityMontage(Montage);
	GetTimers().SetTimer(StepTimer, this, &UVaesenGA_Dodge::FinishDodge, FMath::Max(Length - Montage->BlendOut.GetBlendTime(), 0.1f), false);
}

void UVaesenGA_Dodge::FinishDodge()
{
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, false);
}
