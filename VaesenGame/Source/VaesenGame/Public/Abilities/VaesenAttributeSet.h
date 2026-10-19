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
 * A character's vitals, like Vaesen's two kinds of harm: Health (physical) and Mental. Each has a maximum, and the
 * current value is kept between 0 and it. Changed through Gameplay Effects (a consumable healing, poison wearing
 * down, fear shaking the mind); replicated to everyone.
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

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Vitals", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;
	VAESEN_ATTRIBUTE_ACCESSORS(UVaesenAttributeSet, MaxHealth)

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
	void OnRep_Mental(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxMental(const FGameplayAttributeData& OldValue);
};
