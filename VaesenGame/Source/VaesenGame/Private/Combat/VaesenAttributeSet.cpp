#include "Combat/VaesenAttributeSet.h"

#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UVaesenAttributeSet::UVaesenAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitStaminaRegenRate(15.0f);
	InitProtectionSlashing(0.0f);
	InitProtectionPiercing(0.0f);
	InitProtectionBlunt(0.0f);
	InitIncomingDamage(0.0f);
}

void UVaesenAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, StaminaRegenRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, ProtectionSlashing, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, ProtectionPiercing, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, ProtectionBlunt, COND_None, REPNOTIFY_Always);
}

FGameplayAttribute UVaesenAttributeSet::GetProtectionAttributeFor(const FGameplayTag& DamageType)
{
	if (DamageType.MatchesTag(VaesenTags::Damage_Slashing)) { return GetProtectionSlashingAttribute(); }
	if (DamageType.MatchesTag(VaesenTags::Damage_Piercing)) { return GetProtectionPiercingAttribute(); }
	if (DamageType.MatchesTag(VaesenTags::Damage_Blunt))    { return GetProtectionBluntAttribute(); }
	return FGameplayAttribute();
}

void UVaesenAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UVaesenAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = GetIncomingDamage();
		SetIncomingDamage(0.0f);
		if (Damage <= 0.0f)
		{
			return;
		}

		const float OldHealth = GetHealth();
		SetHealth(FMath::Clamp(OldHealth - Damage, 0.0f, GetMaxHealth()));
		if (OldHealth > 0.0f && GetHealth() <= 0.0f)
		{
			if (UVaesenAbilitySystemComponent* ASC = Cast<UVaesenAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
			{
				ASC->HandleOutOfHealth(Data.EffectSpec.GetContext().GetOriginalInstigator(), Data.EffectSpec);
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
}

void UVaesenAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, Health, OldValue); }
void UVaesenAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, MaxHealth, OldValue); }
void UVaesenAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, Stamina, OldValue); }
void UVaesenAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, MaxStamina, OldValue); }
void UVaesenAttributeSet::OnRep_StaminaRegenRate(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, StaminaRegenRate, OldValue); }
void UVaesenAttributeSet::OnRep_ProtectionSlashing(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, ProtectionSlashing, OldValue); }
void UVaesenAttributeSet::OnRep_ProtectionPiercing(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, ProtectionPiercing, OldValue); }
void UVaesenAttributeSet::OnRep_ProtectionBlunt(const FGameplayAttributeData& OldValue) { GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, ProtectionBlunt, OldValue); }
