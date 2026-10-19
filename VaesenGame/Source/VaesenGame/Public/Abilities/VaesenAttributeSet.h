#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "VaesenAttributeSet.generated.h"

/** Getter, setter and initter for an attribute (the usual GAS helpers). */
#define VAESEN_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * A character's vitals: Health (physical harm), Stamina (running, fighting, effort) and Mental (Vaesen's mental
 * harm). Each has a maximum, and the current value is kept between 0 and it. Changed through Gameplay Effects (a
 * consumable healing, poison wearing down, fear shaking the mind); replicated to everyone.
 */
UCLASS()
class VAESENGAME_API UVaesenAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UVaesenAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Stamina)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxStamina)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Mental)
	FGameplayAttributeData Mental;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Mental)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxMental)
	FGameplayAttributeData MaxMental;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxMental)

protected:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Stamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Mental(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMental(const FGameplayAttributeData& OldValue);

private:
	/** Keeps a current value between 0 and its maximum, and maximums positive. */
	void Clamp(const FGameplayAttribute& Attribute, float& NewValue) const;
};
