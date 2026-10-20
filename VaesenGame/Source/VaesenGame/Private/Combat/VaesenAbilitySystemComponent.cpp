#include "Combat/VaesenAbilitySystemComponent.h"

#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"

UVaesenAbilitySystemComponent::UVaesenAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
}

void UVaesenAbilitySystemComponent::ReportDamageTaken(const FVaesenDamageReport& Report, UAbilitySystemComponent* Source)
{
	UE_LOG(LogVaesenCombat, Log, TEXT("%s took %s"), *GetNameSafe(GetAvatarActor()), *Report.ToString());
	OnDamageReported.Broadcast(Report);

	if (UVaesenAbilitySystemComponent* Attacker = Cast<UVaesenAbilitySystemComponent>(Source))
	{
		if (Attacker != this)
		{
			Attacker->ClientHitConfirmed(Report.Outcome);
		}
	}
}

void UVaesenAbilitySystemComponent::HandleOutOfHealth(AActor* DamageInstigator, const FGameplayEffectSpec& Spec)
{
	if (IsDead())
	{
		return;
	}

	// Replicated so every machine knows (and abilities blocked by State.Dead stop everywhere).
	AddLooseGameplayTag(VaesenTags::State_Dead, 1, EGameplayTagReplicationState::TagOnly);
	CancelAbilities();
	UE_LOG(LogVaesenCombat, Log, TEXT("%s is out of health (by %s)"), *GetNameSafe(GetAvatarActor()), *GetNameSafe(DamageInstigator));
	OnOutOfHealth.Broadcast(DamageInstigator);

	if (UVaesenAbilitySystemComponent* Attacker = Cast<UVaesenAbilitySystemComponent>(Spec.GetContext().GetInstigatorAbilitySystemComponent()))
	{
		if (Attacker != this)
		{
			Attacker->ClientHitConfirmed(EVaesenHitOutcome::Killed);
		}
	}
}

bool UVaesenAbilitySystemComponent::IsDead() const
{
	return HasMatchingGameplayTag(VaesenTags::State_Dead);
}

void UVaesenAbilitySystemComponent::ResetVitals()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}
	SetNumericAttributeBase(UVaesenAttributeSet::GetHealthAttribute(), GetNumericAttribute(UVaesenAttributeSet::GetMaxHealthAttribute()));
	SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), GetNumericAttribute(UVaesenAttributeSet::GetMaxStaminaAttribute()));
	SetLooseGameplayTagCount(VaesenTags::State_Dead, 0, EGameplayTagReplicationState::TagOnly);
}

void UVaesenAbilitySystemComponent::PressAbilityInput(EVaesenAbilityInput Input)
{
	if (Input != EVaesenAbilityInput::None)
	{
		AbilityLocalInputPressed(static_cast<int32>(Input));
	}
}

void UVaesenAbilitySystemComponent::ReleaseAbilityInput(EVaesenAbilityInput Input)
{
	if (Input != EVaesenAbilityInput::None)
	{
		AbilityLocalInputReleased(static_cast<int32>(Input));
	}
}

void UVaesenAbilitySystemComponent::ClientHitConfirmed_Implementation(EVaesenHitOutcome Outcome)
{
	OnHitConfirmed.Broadcast(Outcome);
}
