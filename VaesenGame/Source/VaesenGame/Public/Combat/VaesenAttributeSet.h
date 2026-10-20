#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "VaesenAttributeSet.generated.h"

#define VAESEN_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Health, stamina and protection, for every character (and the training dummies).
 *
 * Damage arrives in the meta attribute IncomingDamage (written by UVaesenDamageExecution after protection and block);
 * PostGameplayEffectExecute takes it off Health. Protection per damage type (ProtectionSlashing...) is what armor will
 * raise later through infinite GameplayEffects; negative protection is a weakness. Damage types without a protection
 * attribute (Damage.Iron, Damage.Silver...) aren't reduced.
 */
UCLASS()
class VAESENGAME_API UVaesenAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UVaesenAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** The protection attribute against a damage type (Slashing, Piercing, Blunt), or an invalid one if there's none. */
	static FGameplayAttribute GetProtectionAttributeFor(const FGameplayTag& DamageType);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Attributes")
	FGameplayAttributeData Stamina;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Attributes")
	FGameplayAttributeData MaxStamina;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxStamina)

	/** Stamina regained per second (paused while blocking or drawing a bow). */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StaminaRegenRate, Category = "Attributes")
	FGameplayAttributeData StaminaRegenRate;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, StaminaRegenRate)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProtectionSlashing, Category = "Attributes|Protection")
	FGameplayAttributeData ProtectionSlashing;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, ProtectionSlashing)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProtectionPiercing, Category = "Attributes|Protection")
	FGameplayAttributeData ProtectionPiercing;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, ProtectionPiercing)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ProtectionBlunt, Category = "Attributes|Protection")
	FGameplayAttributeData ProtectionBlunt;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, ProtectionBlunt)

	/** Meta attribute (server only, not replicated): damage about to be taken off Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, IncomingDamage)

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_StaminaRegenRate(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ProtectionSlashing(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ProtectionPiercing(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_ProtectionBlunt(const FGameplayAttributeData& OldValue);
};
