#include "Abilities/VaesenAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UVaesenAttributeSet::UVaesenAttributeSet()
{
	InitMaxHealth(100.0f);
	InitHealth(100.0f);
	InitMaxStamina(100.0f);
	InitStamina(100.0f);
	InitMaxMental(100.0f);
	InitMental(100.0f);
}

void UVaesenAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, Mental, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UVaesenAttributeSet, MaxMental, COND_None, REPNOTIFY_Always);
}

void UVaesenAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UVaesenAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UVaesenAttributeSet::Clamp(const FGameplayAttribute& Attribute, float& NewValue) const
{
	// Current values stay within 0 and their maximum; maximums stay positive.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
	else if (Attribute == GetMentalAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMental());
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxStaminaAttribute() || Attribute == GetMaxMentalAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.0f);
	}
}

void UVaesenAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Instant effects change the base value, which PreAttributeChange doesn't see: clamp it here too.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
	else if (Data.EvaluatedData.Attribute == GetMentalAttribute())
	{
		SetMental(FMath::Clamp(GetMental(), 0.0f, GetMaxMental()));
	}
}

void UVaesenAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, Health, OldValue);
}

void UVaesenAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, MaxHealth, OldValue);
}

void UVaesenAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, Stamina, OldValue);
}

void UVaesenAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, MaxStamina, OldValue);
}

void UVaesenAttributeSet::OnRep_Mental(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, Mental, OldValue);
}

void UVaesenAttributeSet::OnRep_MaxMental(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UVaesenAttributeSet, MaxMental, OldValue);
}
