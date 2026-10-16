#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaesenFrontEndGameMode.generated.h"

/**
 * Game mode for the client-only front-end map (login / player select). Runs standalone, never on a dedicated server,
 * so it does not derive from AVaesenGameModeBase and has no player-session gating. Spawns no pawn.
 */
UCLASS()
class VAESENGAME_API AVaesenFrontEndGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenFrontEndGameMode();
};
