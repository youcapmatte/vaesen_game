#include "Abilities/VaesenGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Animation/AnimMontage.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenGameplayEffects.h"
#include "Combat/VaesenGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "TimerManager.h"

UVaesenGameplayAbility::UVaesenGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	StaminaCostEffect = UVaesenGE_StaminaCost::StaticClass();

	ActivationBlockedTags.AddTag(VaesenTags::State_Staggered);
	ActivationBlockedTags.AddTag(VaesenTags::State_Dead);
}

AVaesenBaseCharacter* UVaesenGameplayAbility::GetVaesenCharacter(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const FGameplayAbilityActorInfo* Info = ActorInfo ? ActorInfo : GetCurrentActorInfo();
	return Info ? Cast<AVaesenBaseCharacter>(Info->AvatarActor.Get()) : nullptr;
}

UVaesenEquipmentComponent* UVaesenGameplayAbility::GetEquipment(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const AVaesenBaseCharacter* Character = GetVaesenCharacter(ActorInfo);
	return Character ? Character->GetEquipment() : nullptr;
}

UVaesenWeaponItem* UVaesenGameplayAbility::GetDrawnWeapon(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const UVaesenEquipmentComponent* Equipment = GetEquipment(ActorInfo);
	return Equipment ? Equipment->GetDrawnWeapon() : nullptr;
}

UVaesenAbilitySystemComponent* UVaesenGameplayAbility::GetVaesenASC() const
{
	return Cast<UVaesenAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
}

FTimerManager& UVaesenGameplayAbility::GetTimers() const
{
	return GetWorld()->GetTimerManager();
}

bool UVaesenGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}
	const float Cost = GetStaminaCost(ActorInfo);
	const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (Cost <= 0.0f || !ASC)
	{
		return true;
	}
	// Some stamina left is enough to start; zero stamina stops everything that costs any.
	const float Stamina = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
	return Stamina >= FMath::Min(Cost, 1.0f) && Stamina > 0.0f;
}

void UVaesenGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
	const float Cost = GetStaminaCost(ActorInfo);
	if (Cost <= 0.0f || !StaminaCostEffect || !HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
	{
		return;
	}
	const FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, StaminaCostEffect, 1.0f);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(VaesenTags::Data_StaminaCost, -Cost);
		ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
	}
}

bool UVaesenGameplayAbility::DrainStamina(float Amount)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC || Amount <= 0.0f)
	{
		return true;
	}
	const float Stamina = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
	if (ASC->IsOwnerActorAuthoritative())
	{
		ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), FMath::Max(0.0f, Stamina - Amount));
	}
	return Stamina > Amount;
}

float UVaesenGameplayAbility::PlayAbilityMontage(UAnimMontage* Montage, float Rate, FName Section)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Montage || !ASC || !GetVaesenCharacter())
	{
		return 0.0f;
	}
	const float Length = ASC->PlayMontage(this, GetCurrentActivationInfo(), Montage, Rate, Section);
	if (Length > 0.0f)
	{
		PlayingMontage = Montage;
	}
	return Length > 0.0f ? Length / FMath::Max(Rate, 0.01f) : 0.0f;
}

void UVaesenGameplayAbility::StopAbilityMontage()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && PlayingMontage && ASC->GetCurrentMontage() == PlayingMontage)
	{
		ASC->CurrentMontageStop(0.2f);
	}
	PlayingMontage = nullptr;
}

void UVaesenGameplayAbility::GetAimViewPoint(FVector& OutOrigin, FVector& OutDirection) const
{
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	FRotator Rotation = FRotator::ZeroRotator;
	if (const APlayerController* Controller = Info ? Info->PlayerController.Get() : nullptr)
	{
		Controller->GetPlayerViewPoint(OutOrigin, Rotation);
	}
	else if (const AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr)
	{
		Avatar->GetActorEyesViewPoint(OutOrigin, Rotation);
	}
	OutDirection = Rotation.Vector();

	// In third person the camera is behind the character: start the aim level with it, so nothing between the camera
	// and the character can be hit (the same idea as the interaction trace).
	if (const AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr)
	{
		const float Along = FVector::DotProduct(Avatar->GetActorLocation() - OutOrigin, OutDirection);
		if (Along > 0.0f)
		{
			OutOrigin += OutDirection * Along;
		}
	}
}

void UVaesenGameplayAbility::SendAim(const FVector& Origin, const FVector& Direction, float Strength)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}
	if (ASC->IsOwnerActorAuthoritative())
	{
		OnAimReceived(Origin, Direction.GetSafeNormal(), Strength);
		return;
	}

	// The aim rides on a hit result: trace start = origin, trace end = along the direction, Time = strength.
	FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
	Data->HitResult.TraceStart = Origin;
	Data->HitResult.TraceEnd = Origin + Direction.GetSafeNormal() * 1000.0f;
	Data->HitResult.Time = FMath::Clamp(Strength, 0.0f, 1.0f);
	FGameplayAbilityTargetDataHandle Handle(Data);

	FScopedPredictionWindow Window(ASC);
	ASC->CallServerSetReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey(), Handle, FGameplayTag(), ASC->ScopedPredictionKey);
	// The local copy acts too (cosmetics already played; it doesn't deal damage).
	OnAimReceived(Origin, Direction.GetSafeNormal(), Strength);
}

void UVaesenGameplayAbility::SendAimCancelled()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && !ASC->IsOwnerActorAuthoritative())
	{
		ASC->ServerSetReplicatedTargetDataCancelled(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey(), ASC->ScopedPredictionKey);
	}
}

void UVaesenGameplayAbility::WaitForAim()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}
	const FGameplayAbilitySpecHandle Spec = GetCurrentAbilitySpecHandle();
	const FPredictionKey Key = GetCurrentActivationInfo().GetActivationPredictionKey();
	TargetDataHandle = ASC->AbilityTargetDataSetDelegate(Spec, Key).AddUObject(this, &UVaesenGameplayAbility::OnTargetDataReplicated);
	TargetCancelHandle = ASC->AbilityTargetDataCancelledDelegate(Spec, Key).AddUObject(this, &UVaesenGameplayAbility::OnTargetDataCancelled);
	ASC->CallReplicatedTargetDataDelegatesIfSet(Spec, Key);
}

void UVaesenGameplayAbility::OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		ASC->ConsumeClientReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey());
	}
	const FHitResult* Hit = Data.Num() > 0 ? Data.Get(0)->GetHitResult() : nullptr;
	if (!Hit)
	{
		OnAimCancelled();
		return;
	}
	OnAimReceived(Hit->TraceStart, (Hit->TraceEnd - Hit->TraceStart).GetSafeNormal(), FMath::Clamp(Hit->Time, 0.0f, 1.0f));
}

void UVaesenGameplayAbility::OnTargetDataCancelled()
{
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		ASC->ConsumeClientReplicatedTargetData(GetCurrentAbilitySpecHandle(), GetCurrentActivationInfo().GetActivationPredictionKey());
	}
	OnAimCancelled();
}

void UVaesenGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		const FPredictionKey Key = ActivationInfo.GetActivationPredictionKey();
		if (TargetDataHandle.IsValid())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, Key).Remove(TargetDataHandle);
		}
		if (TargetCancelHandle.IsValid())
		{
			ASC->AbilityTargetDataCancelledDelegate(Handle, Key).Remove(TargetCancelHandle);
		}
	}
	TargetDataHandle.Reset();
	TargetCancelHandle.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
