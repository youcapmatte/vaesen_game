#include "Abilities/VaesenRangedAbilities.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "Engine/World.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "TimerManager.h"
#include "Weapons/VaesenRangedWeaponActor.h"

namespace VaesenRangedAbilities
{
	/** Montage play rate so it lasts Seconds. */
	float RateToFit(const UAnimMontage* Montage, float Seconds)
	{
		return Montage && Seconds > 0.0f ? FMath::Max(Montage->GetPlayLength() / Seconds, 0.05f) : 1.0f;
	}

	UVaesenInventoryComponent* GetInventory(const FGameplayAbilityActorInfo* ActorInfo)
	{
		const AVaesenPlayerCharacter* Player = ActorInfo ? Cast<AVaesenPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
		return Player ? Player->GetInventory() : nullptr;
	}
}

// --- Base ---

UVaesenRangedAbility::UVaesenRangedAbility()
{
	ActivationBlockedTags.AddTag(VaesenTags::State_SwitchingWeapon);
	ActivationBlockedTags.AddTag(VaesenTags::State_Throwing);
}

UVaesenRangedWeaponItem* UVaesenRangedAbility::GetRangedItem(const FGameplayAbilityActorInfo* ActorInfo) const
{
	return Cast<UVaesenRangedWeaponItem>(GetDrawnWeapon(ActorInfo));
}

bool UVaesenRangedAbility::IsAiming() const
{
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	return ASC && (ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming) || ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow));
}

AVaesenRangedWeaponActor* UVaesenRangedAbility::GetRangedActor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	return Item ? Cast<AVaesenRangedWeaponActor>(Item->GetWeaponActor()) : nullptr;
}

bool UVaesenRangedAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	return Item && GetRangedActor(ActorInfo) && Item->IsBow() == WantsBow();
}

// --- GA_FireWeapon ---

UVaesenGA_FireWeapon::UVaesenGA_FireWeapon()
{
	InputBinding = EVaesenAbilityInput::Attack;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Fire, VaesenTags::Ability_Attack, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Attacking);
	ActivationBlockedTags.AddTag(VaesenTags::State_Cycling);
	ActivationBlockedTags.AddTag(VaesenTags::State_Attacking);
	// Firing stops a reload between rounds (a muzzle-loader's reload can't be cancelled).
	CancelAbilitiesWithTag.AddTag(VaesenTags::Ability_Reload);
	CycleAbility = UVaesenGA_CycleAction::StaticClass();
}

bool UVaesenGA_FireWeapon::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	return !(Item && !Item->CanInterruptReload() && ASC && ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
}

void UVaesenGA_FireWeapon::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	const EVaesenFireBlock Block = Actor ? Actor->GetFireBlock() : EVaesenFireBlock::NoWeapon;
	if (Block != EVaesenFireBlock::None)
	{
		if (IsLocallyControlled() && Actor && (Block == EVaesenFireBlock::Empty || Block == EVaesenFireBlock::NeedsCycle || Block == EVaesenFireBlock::Broken))
		{
			Actor->PlayDryFire(Block);
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!IsLocallyControlled())
	{
		// The server's copy for a remote player: wait for their aim.
		WaitForAim();
		return;
	}

	FVector Origin, Direction;
	GetAimViewPoint(Origin, Direction);
	const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const bool bAiming = ASC && ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming);
	FRandomStream Random(FMath::Rand());
	Direction = AVaesenRangedWeaponActor::ApplySpread(Direction, Actor->GetCurrentSpread(bAiming), Random);

	if (!HasAuthority(&ActivationInfo))
	{
		// Prediction: the shot is seen and heard at once; the server decides what it hits.
		UAnimMontage* Montage = Item->GetFireMontage(bAiming);
		PlayAbilityMontage(Montage);
		Actor->PlayFireCosmetics(Montage);
	}
	SendAim(Origin, Direction, 1.0f);
}

void UVaesenGA_FireWeapon::OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	if (HasAuthority(&CurrentActivationInfo))
	{
		AVaesenRangedWeaponActor* Actor = GetRangedActor();
		const bool bFired = Actor && Actor->ServerFire(Origin, Direction, 1.0f);
		if (bFired && IsLocallyControlled())
		{
			// This machine is the server and the shooter (listen server, standalone): it plays the shot itself.
			UAnimMontage* Montage = GetRangedItem()->GetFireMontage(IsAiming());
			PlayAbilityMontage(Montage);
			Actor->PlayFireCosmetics(Montage);
		}
		else if (bFired)
		{
			// A remote shooter: the server plays the montage too, so the ability system replicates it to everyone else
			// (the shooter already predicted it). No cosmetics on the server itself.
			PlayAbilityMontage(GetRangedItem()->GetFireMontage(IsAiming()));
		}
		StartCycleIfNeeded();
	}
	EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, false);
}

void UVaesenGA_FireWeapon::OnAimCancelled()
{
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
}

void UVaesenGA_FireWeapon::StartCycleIfNeeded()
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Item || !ASC || !CycleAbility || !Item->NeedsCycle() || (Item->GetLoadedRounds() <= 0 && !UVaesenCombatLibrary::IsFreeFire()))
	{
		return;
	}
	// Once this ability has ended; the server starting a predicted ability tells the player's machine to run it too.
	// The granted one may be a Blueprint of CycleAbility (GA_CycleAction), so look for anything of that class.
	const TSubclassOf<UGameplayAbility> Cycle = CycleAbility;
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(ASC, [ASC, Cycle]()
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA(Cycle))
			{
				ASC->TryActivateAbility(Spec.Handle);
				return;
			}
		}
	}));
}

// --- GA_Reload ---

UVaesenGA_Reload::UVaesenGA_Reload()
{
	InputBinding = EVaesenAbilityInput::Reload;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Reload, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Reloading);
	ActivationBlockedTags.AddTag(VaesenTags::State_Attacking);
	ActivationBlockedTags.AddTag(VaesenTags::State_Cycling);
	ActivationBlockedTags.AddTag(VaesenTags::State_Reloading);
	BlockAbilitiesWithTag.AddTag(VaesenTags::Ability_Melee);
}

bool UVaesenGA_Reload::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	const AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	// Nothing to do (full, or a bolt to work first), or nothing to load: no reload, no error.
	return Item && Actor && !Actor->NeedsCycle()
		&& Item->GetLoadedRounds() < Item->MagazineSize
		&& Item->CountAmmoIn(VaesenRangedAbilities::GetInventory(ActorInfo)) > 0;
}

void UVaesenGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	bPerRound = Item->CanInterruptReload();
	// Loading round by round (a revolver's cylinder): opened first, closed when done (each round its own sound).
	if (bPerRound && IsLocallyControlled())
	{
		Actor->PlayWeaponSound(EVaesenWeaponSound::ReloadOpen);
	}
	SetCanBeCanceled(Item->ReloadStyle != EVaesenReloadStyle::MuzzleLoading);

	// Spent casings out first (revolver emptied, breech opened).
	if (Item->CasingEjection == EVaesenCasingEjection::OnReload)
	{
		if (IsLocallyControlled())
		{
			Actor->EjectCasingsUnlessNotified(EVaesenCasingMoment::Reload, Item->GetReloadMontage(), 0);
		}
		if (HasAuthority(&ActivationInfo))
		{
			Actor->MulticastActionCosmetics(EVaesenCasingMoment::Reload, 0);
		}
	}
	StartRound();
}

void UVaesenGA_Reload::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Round by round: the cylinder closed again, done or stopped (to fire).
	AVaesenRangedWeaponActor* Actor = bPerRound && IsLocallyControlled() ? GetRangedActor(ActorInfo) : nullptr;
	if (Actor)
	{
		Actor->PlayWeaponSound(EVaesenWeaponSound::ReloadClose);
	}
	bPerRound = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UVaesenGA_Reload::StartRound()
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item)
	{
		EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
		return;
	}
	UAnimMontage* Montage = Item->GetReloadMontage();
	PlayAbilityMontage(Montage, VaesenRangedAbilities::RateToFit(Montage, Item->ReloadTime));
	if (!Montage && IsLocallyControlled())
	{
		if (AVaesenRangedWeaponActor* Actor = GetRangedActor())
		{
			Actor->PlayWeaponSound(Item->ReloadStyle == EVaesenReloadStyle::MuzzleLoading ? EVaesenWeaponSound::Ramrod : EVaesenWeaponSound::ReloadInsert);
		}
	}
	GetTimers().SetTimer(StepTimer, this, &UVaesenGA_Reload::FinishRound, Item->ReloadTime, false);
}

void UVaesenGA_Reload::FinishRound()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	AVaesenRangedWeaponActor* Actor = GetRangedActor();
	if (!Item || !Actor)
	{
		EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	if (HasAuthority(&CurrentActivationInfo))
	{
		const int32 Loaded = Actor->ServerLoadRounds(bPerRound ? 1 : Item->MagazineSize);
		const bool bMore = bPerRound && Loaded > 0 && Item->GetLoadedRounds() < Item->MagazineSize
			&& Item->CountAmmoIn(VaesenRangedAbilities::GetInventory(ActorInfo)) > 0;
		if (!bMore)
		{
			// The server ends it; the player's machine follows.
			EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, false);
			return;
		}
		StartRound();
		return;
	}

	// The player's machine: keep going round by round until the server says it's done.
	if (bPerRound)
	{
		StartRound();
	}
}

// --- GA_CycleAction ---

UVaesenGA_CycleAction::UVaesenGA_CycleAction()
{
	InputBinding = EVaesenAbilityInput::Reload;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Reload, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Cycling);
	ActivationBlockedTags.AddTag(VaesenTags::State_Cycling);
	ActivationBlockedTags.AddTag(VaesenTags::State_Reloading);
}

bool UVaesenGA_CycleAction::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	const AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	return Item && Actor && Item->HasCycleStep() && Actor->NeedsCycle();
}

void UVaesenGA_CycleAction::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	UAnimMontage* Montage = Item->GetMontage(&FVaesenWeaponMontages::Cycle);
	PlayAbilityMontage(Montage, VaesenRangedAbilities::RateToFit(Montage, Item->CycleTime));
	if (!Montage && IsLocallyControlled())
	{
		GetRangedActor(ActorInfo)->PlayWeaponSound(EVaesenWeaponSound::Cycle);
	}
	GetTimers().SetTimer(StepTimer, this, &UVaesenGA_CycleAction::FinishCycle, Item->CycleTime, false);
	GetTimers().SetTimer(RepeatTimer, this, &UVaesenGA_CycleAction::EjectCase, Item->CycleTime * VaesenBolt::Back, false);
}

void UVaesenGA_CycleAction::EjectCase()
{
	// When the bolt is fully back (the hand works it there: VaesenBolt), on the shooter's machine at once and on the
	// others through the server.
	AVaesenRangedWeaponActor* Actor = GetRangedActor();
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Actor || !Item)
	{
		return;
	}
	if (IsLocallyControlled())
	{
		Actor->EjectCasingsUnlessNotified(EVaesenCasingMoment::Cycle, Item->GetMontage(&FVaesenWeaponMontages::Cycle), 1);
	}
	if (HasAuthority(&CurrentActivationInfo))
	{
		Actor->MulticastActionCosmetics(EVaesenCasingMoment::Cycle, 1);
	}
}

void UVaesenGA_CycleAction::FinishCycle()
{
	AVaesenRangedWeaponActor* Actor = GetRangedActor();
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (Actor && Item)
	{
		Actor->ClearPredictedCycle();
		if (HasAuthority(&CurrentActivationInfo))
		{
			Actor->ServerFinishCycle();
		}
	}
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, false);
}

// --- GA_Aim ---

UVaesenGA_Aim::UVaesenGA_Aim()
{
	InputBinding = EVaesenAbilityInput::AimOrBlock;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Aim, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Aiming);
}

bool UVaesenGA_Aim::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Any ranged weapon, bows included (skips the bow check of the base).
	return UVaesenGameplayAbility::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& GetRangedItem(ActorInfo) != nullptr;
}

void UVaesenGA_Aim::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UVaesenGA_Aim::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// --- GA_DrawBow ---

UVaesenGA_DrawBow::UVaesenGA_DrawBow()
{
	InputBinding = EVaesenAbilityInput::Attack;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Fire, VaesenTags::Ability_Attack, VaesenTags::Ability_Weapon }));
	ActivationOwnedTags.AddTag(VaesenTags::State_DrawingBow);
	ActivationOwnedTags.AddTag(VaesenTags::State_Attacking);
	ActivationBlockedTags.AddTag(VaesenTags::State_Attacking);
}

bool UVaesenGA_DrawBow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	const AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	return Actor && Actor->GetFireBlock() == EVaesenFireBlock::None;
}

float UVaesenGA_DrawBow::GetDrawFraction() const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item || !GetWorld())
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - DrawStartTime) / FMath::Max(Item->DrawTime, 0.01f)), 0.0f, 1.0f);
}

void UVaesenGA_DrawBow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	bReleased = false;

	// Nock, then draw: the strength starts building once the arrow is on the string.
	DrawStartTime = GetWorld()->GetTimeSeconds() + Item->NockTime;
	UAnimMontage* Montage = Item->GetReloadMontage();
	PlayAbilityMontage(Montage, VaesenRangedAbilities::RateToFit(Montage, Item->NockTime + Item->DrawTime));
	if (IsLocallyControlled())
	{
		Actor->PlayWeaponSound(EVaesenWeaponSound::BowNock);
	}
	GetTimers().SetTimer(RepeatTimer, this, &UVaesenGA_DrawBow::TickDraw, 0.1f, true);

	if (!IsLocallyControlled())
	{
		WaitForAim();
	}
}

void UVaesenGA_DrawBow::TickDraw()
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	AVaesenRangedWeaponActor* Actor = GetRangedActor();
	if (!Item || !Actor)
	{
		EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
		return;
	}
	const float Fraction = GetDrawFraction();
	if (HasAuthority(&CurrentActivationInfo))
	{
		Actor->ServerSetDrawAlpha(Fraction);
		// Holding a drawn bow is tiring; out of stamina, the arrow is let down.
		if (Fraction > 0.0f && !DrainStamina(Item->DrawStaminaPerSecond * 0.1f))
		{
			Actor->ServerSetDrawAlpha(0.0f);
			EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
		}
	}
}

void UVaesenGA_DrawBow::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (bReleased || !IsLocallyControlled())
	{
		return;
	}
	bReleased = true;

	const UVaesenRangedWeaponItem* Item = GetRangedItem(ActorInfo);
	AVaesenRangedWeaponActor* Actor = GetRangedActor(ActorInfo);
	const float Fraction = GetDrawFraction();
	UE_LOG(LogVaesenCombat, Display, TEXT("%s releases the bow at %.0f%% draw (%s)"), *GetNameSafe(GetAvatarActorFromActorInfo()), Fraction * 100.0f,
		Item && Actor && Fraction >= Item->MinDrawToShoot ? TEXT("shoots") : TEXT("too little: lets it down"));
	if (!Item || !Actor || Fraction < Item->MinDrawToShoot)
	{
		// Let down without a shot.
		SendAimCancelled();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	FVector Origin, Direction;
	GetAimViewPoint(Origin, Direction);
	FRandomStream Random(FMath::Rand());
	Direction = AVaesenRangedWeaponActor::ApplySpread(Direction, Actor->GetCurrentSpread(true), Random);
	Actor->PlayWeaponSound(EVaesenWeaponSound::BowRelease);
	if (!HasAuthority(&ActivationInfo))
	{
		UAnimMontage* Montage = Item->GetMontage(&FVaesenWeaponMontages::Fire);
		PlayAbilityMontage(Montage);
		Actor->PlayFireCosmetics(Montage);
	}
	SendAim(Origin, Direction, Fraction);
}

void UVaesenGA_DrawBow::OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength)
{
	if (HasAuthority(&CurrentActivationInfo))
	{
		AVaesenRangedWeaponActor* Actor = GetRangedActor();
		// No stronger than the server's own clock allows (a little slack for lag).
		const float Checked = FMath::Min(Strength, GetDrawFraction() + 0.15f);
		const bool bFired = Actor && Actor->ServerFire(Origin, Direction, Checked);
		if (Actor)
		{
			Actor->ServerSetDrawAlpha(0.0f);
		}
		if (bFired && IsLocallyControlled())
		{
			UAnimMontage* Montage = GetRangedItem()->GetFireMontage(IsAiming());
			PlayAbilityMontage(Montage);
			Actor->PlayFireCosmetics(Montage);
		}
		else if (bFired)
		{
			// A remote shooter: the server plays the montage too, so the ability system replicates it to everyone else
			// (the shooter already predicted it). No cosmetics on the server itself.
			PlayAbilityMontage(GetRangedItem()->GetFireMontage(IsAiming()));
		}
	}
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, false);
}

void UVaesenGA_DrawBow::OnAimCancelled()
{
	if (AVaesenRangedWeaponActor* Actor = GetRangedActor())
	{
		Actor->ServerSetDrawAlpha(0.0f);
	}
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
}
