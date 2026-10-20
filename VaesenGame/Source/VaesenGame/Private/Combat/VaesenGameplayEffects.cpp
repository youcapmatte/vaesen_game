#include "Combat/VaesenGameplayEffects.h"

#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenDamageExecution.h"
#include "Combat/VaesenGameplayTags.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UVaesenGE_Damage::UVaesenGE_Damage()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayEffectExecutionDefinition Execution;
	Execution.CalculationClass = UVaesenDamageExecution::StaticClass();
	Executions.Add(Execution);
}

UVaesenGE_StaminaCost::UVaesenGE_StaminaCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat Cost;
	Cost.DataTag = VaesenTags::Data_StaminaCost;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UVaesenAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);
	Modifiers.Add(Modifier);
}

UVaesenGE_Stagger::UVaesenGE_Stagger()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.8f));

	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	FInheritedTagContainer Granted;
	Granted.AddTag(VaesenTags::State_Staggered);
	TargetTags->SetAndApplyTargetTagChanges(Granted);
	GEComponents.Add(TargetTags);
}

UVaesenGE_StaminaRegen::UVaesenGE_StaminaRegen()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	Period = FScalableFloat(TickPeriod);
	bExecutePeriodicEffectOnApplication = false;

	// Stamina += StaminaRegenRate * period, each period.
	FAttributeBasedFloat RegenPerTick;
	RegenPerTick.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(UVaesenAttributeSet::GetStaminaRegenRateAttribute(), EGameplayEffectAttributeCaptureSource::Target, false);
	RegenPerTick.Coefficient = FScalableFloat(TickPeriod);

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = UVaesenAttributeSet::GetStaminaAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(RegenPerTick);
	Modifiers.Add(Modifier);

	// Paused (not removed) while blocking, drawing a bow or dead.
	UTargetTagRequirementsGameplayEffectComponent* Requirements = CreateDefaultSubobject<UTargetTagRequirementsGameplayEffectComponent>(TEXT("TagRequirements"));
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(VaesenTags::State_Blocking);
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(VaesenTags::State_DrawingBow);
	Requirements->OngoingTagRequirements.IgnoreTags.AddTag(VaesenTags::State_Dead);
	GEComponents.Add(Requirements);
}
