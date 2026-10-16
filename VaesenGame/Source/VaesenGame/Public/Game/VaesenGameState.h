#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "VaesenGameState.generated.h"

/** Replicated state of the world a server hosts, visible to every connected client. */
UCLASS()
class VAESENGAME_API AVaesenGameState : public AGameStateBase
{
	GENERATED_BODY()
};
