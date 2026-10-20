#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "VaesenDamageExecution.generated.h"

/**
 * GE_Damage's execution (server). For each damage type in the spec (SetByCaller Damage.*):
 *   1. reduce it by the target's matching protection attribute (min zero);
 *   2. if the target is blocking, the hit came from its front and can be blocked (shields only for projectiles):
 *      reduce what's left by the blocking weapon's block profile, and wear that weapon by the amount absorbed;
 *   3. if the target pressed block within its parry window: negate it all, no wear, and stagger the attacker.
 * The total goes to IncomingDamage. The result is reported to the target (debug numbers, log) and the attacker
 * (hit marker), and a hit GameplayCue plays on every machine.
 */
UCLASS()
class VAESENGAME_API UVaesenDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UVaesenDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
