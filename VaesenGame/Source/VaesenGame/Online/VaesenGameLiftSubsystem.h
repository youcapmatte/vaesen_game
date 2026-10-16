#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaesenGameLiftSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVaesenGameLift, Log, All);

UENUM()
enum class EVaesenGameLiftMode : uint8
{
	/** No GameLift: the server was started with -NoGameLift or built without the SDK. Every player session is accepted. */
	Local,
	/** GameLift Anywhere: this PC is a registered compute; connection parameters come from the command line. */
	Anywhere,
	/** GameLift managed fleet: the SDK reads its parameters from the host. */
	Managed,
};

/**
 * Owns the GameLift Server SDK for the whole lifetime of a dedicated server process.
 *
 * Lives on the GameInstance (not GameMode/GameState) because the process outlives every map and game mode:
 * InitSDK/ProcessReady run once at startup, and OnStartGameSession loads the world's map.
 * Game modes only ask this subsystem to validate and release player sessions (see AVaesenGameModeBase).
 *
 * Command line:
 *   -NoGameLift                         Run without GameLift (local development).
 *   -GameLiftAnywhere                   Connect as an Anywhere compute, with:
 *     -GameLiftWebSocketUrl=wss://...   from RegisterCompute (GameLiftServiceSdkEndpoint)
 *     -GameLiftFleetId=fleet-...
 *     -GameLiftHostId=<compute name>
 *     -GameLiftAuthToken=...            from GetComputeAuthToken
 *     -GameLiftRegion=eu-central-1      (optional)
 *   -port=7777                          Port reported to GameLift and listened on.
 */
UCLASS()
class VAESENGAME_API UVaesenGameLiftSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	EVaesenGameLiftMode GetMode() const { return Mode; }

	/** True once GameLift has activated a game session and players may join. Always true in Local mode. */
	bool IsAcceptingPlayers() const;

	/** Id of the world this process hosts (game property "worldId"), empty until a game session starts. */
	const FString& GetWorldId() const { return WorldId; }

	/** Called from PreLogin. Reserves the player's slot with GameLift; fills OutError when the player must be rejected. */
	bool AcceptPlayerSession(const FString& PlayerSessionId, FString& OutError);

	/** Called from Logout so GameLift frees the player's slot. */
	void RemovePlayerSession(const FString& PlayerSessionId);

private:
	EVaesenGameLiftMode Mode = EVaesenGameLiftMode::Local;
	bool bGameSessionActive = false;
	FString GameSessionId;
	FString WorldId;
	FString PendingMap;
	FDelegateHandle PostLoadMapHandle;

	/** Kept alive for the process: the SDK calls back through the delegates stored here. Only set when built with GameLift. */
	TSharedPtr<struct FProcessParameters> ProcessParameters;

	bool bProcessEnded = false;

	// Implemented only in server builds (VAESEN_WITH_GAMELIFT).
	bool InitGameLift();
	void HandleStartGameSession(const FString& InGameSessionId, const FString& InWorldId, const FString& Map);
	void HandleTerminate();
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void ActivateGameSession();
	void EndProcess();
};
