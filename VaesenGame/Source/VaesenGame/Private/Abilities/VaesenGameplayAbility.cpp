#include "Abilities/VaesenGameplayAbility.h"

UVaesenGameplayAbility::UVaesenGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
