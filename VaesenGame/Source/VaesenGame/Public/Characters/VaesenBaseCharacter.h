#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VaesenBaseCharacter.generated.h"

/**
 * Shared base for every character in the world, player-controlled or AI.
 * Holds what all characters have in common (capsule, movement); no camera or input.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenBaseCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AVaesenBaseCharacter();
};
