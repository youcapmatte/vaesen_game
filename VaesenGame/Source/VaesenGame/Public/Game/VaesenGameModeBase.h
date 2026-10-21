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
 * On dedicated servers it also tells the API which characters are here (UVaesenServerApiSubsystem): who left, and a
 * heartbeat every HeartbeatSeconds that keeps their players' claims alive.
 */
UCLASS()
class VAESENGAME_API AVaesenGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AVaesenGameModeBase();

	/** URL option the client passes when connecting: <host>:<port>?PlayerSessionId=<id> */
	static const TCHAR* PlayerSessionIdOption;

	/** URL option naming the character the player plays: ...&CharacterId=character-{guid} */
	static const TCHAR* CharacterIdOption;

	virtual void BeginPlay() override;

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
	virtual void Logout(AController* Exiting) override;

protected:
	/** How often the server reports its characters to the API. Well under the API's claim timeout (30 min). */
	UPROPERTY(EditDefaultsOnly, Category = "Vaesen", meta = (ClampMin = "10", Units = "s"))
	float HeartbeatSeconds = 300.0f;

private:
	/** Asks the API for the joining player's character's look and puts it on its player state (and character). */
	void LoadCharacterAppearance(class AVaesenPlayerState* PlayerState);

	void SendHeartbeat();

	FTimerHandle HeartbeatTimer;
};
