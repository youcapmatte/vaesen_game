#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaesenFrontEndGameModeBase.generated.h"

/**
 * Base for the client-only menu maps (login, character select). These run standalone, never on a dedicated server,
 * so they don't derive from AVaesenGameModeBase and have no player-session gating.
 * Spawns a plain APawn (no movement); input is UI-only, set by AVaesenFrontEndPlayerControllerBase.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenFrontEndGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenFrontEndGameModeBase();
};
