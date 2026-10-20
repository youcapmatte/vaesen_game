#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "VaesenGameplayEffects.generated.h"

/**
 * The combat GameplayEffects, set up in C++ so they work without assets. The commandlet makes Blueprint children
 * (GE_Damage, GE_StaminaCost, GE_Stagger, GE_StaminaRegen) for tuning; abilities default to these classes.
 */

/**
 * Damage: one SetByCaller magnitude per damage type (Damage.Slashing 12, Damage.Piercing 4...). UVaesenDamageExecution
 * reduces each by the target's protection and block, and writes the total to IncomingDamage.
 */
UCLASS()
class VAESENGAME_API UVaesenGE_Damage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVaesenGE_Damage();
};

/** An ability's stamina cost: Stamina + SetByCaller Data.StaminaCost (pass it negative). */
UCLASS()
class VAESENGAME_API UVaesenGE_StaminaCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVaesenGE_StaminaCost();
};

/** Staggered (parried): State.Staggered for a moment, which stops attacks and blocks new ones. */
UCLASS()
class VAESENGAME_API UVaesenGE_Stagger : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVaesenGE_Stagger();
};

/** Always on: gives back StaminaRegenRate stamina per second, except while blocking or drawing a bow. */
UCLASS()
class VAESENGAME_API UVaesenGE_StaminaRegen : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UVaesenGE_StaminaRegen();

	/** How often it ticks. */
	static constexpr float TickPeriod = 0.1f;
};
