#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "VaesenGameInstance.generated.h"

/**
 * Project game instance (set in DefaultEngine.ini). Lives for the whole process on clients and servers.
 * Keep it thin: per-feature state belongs in GameInstance subsystems (e.g. UVaesenGameLiftSubsystem).
 */
UCLASS()
class VAESENGAME_API UVaesenGameInstance : public UGameInstance
{
	GENERATED_BODY()
};
