#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "VaesenGameModeBase.generated.h"

/**
 * Base class for every Vaesen game mode.
 *
 * Gates joining on a valid GameLift player session (the "PlayerSessionId" URL option the API hands to clients)
 * and releases it on logout. All GameLift work is delegated to UVaesenGameLiftSubsystem, so derived game modes
 * never deal with GameLift directly.
 */
UCLASS()
class VAESENGAME_API AVaesenGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenGameModeBase();

	/** URL option the client passes when connecting: <host>:<port>?PlayerSessionId=<id> */
	static const TCHAR* PlayerSessionIdOption;

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void Logout(AController* Exiting) override;
};
