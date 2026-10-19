#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "VaesenGameplayAbility.generated.h"

/**
 * Base for Vaesen's abilities (actions with costs, cooldowns and rules: a skill check, using a talisman, a vaesen's
 * power). One instance per character, predicted on the player's machine and confirmed by the server. Make Blueprint
 * subclasses (GA_...) and give them to characters through their Startup Abilities.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGameplayAbility();
};
