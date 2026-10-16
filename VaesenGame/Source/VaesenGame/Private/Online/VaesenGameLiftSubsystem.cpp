#include "Online/VaesenGameLiftSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"

#if VAESEN_WITH_GAMELIFT
#include "Async/Async.h"
#include "GameLiftServerSDK.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#endif

DEFINE_LOG_CATEGORY(LogVaesenGameLift);

#if VAESEN_WITH_GAMELIFT
namespace VaesenGameLift
{
	constexpr int32 DefaultPort = 7777;

	/** Game property keys set by Vaesen.Api when it creates a world's game session. */
	const TCHAR* const WorldIdProperty = TEXT("worldId");
	const TCHAR* const MapProperty = TEXT("map");

	FGameLiftServerSDKModule& Sdk()
	{
		return FModuleManager::LoadModuleChecked<FGameLiftServerSDKModule>(TEXT("GameLiftServerSDK"));
	}
}
#endif

bool UVaesenGameLiftSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && IsRunningDedicatedServer();
}

void UVaesenGameLiftSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if VAESEN_WITH_GAMELIFT
	if (FParse::Param(FCommandLine::Get(), TEXT("NoGameLift")))
	{
		Mode = EVaesenGameLiftMode::Local;
	}
	else
	{
		Mode = FParse::Param(FCommandLine::Get(), TEXT("GameLiftAnywhere"))
			? EVaesenGameLiftMode::Anywhere
			: EVaesenGameLiftMode::Managed;

		if (!InitGameLift())
		{
			// A server that GameLift can't manage is useless on a fleet; exit so the failure is visible.
			UE_LOG(LogVaesenGameLift, Error, TEXT("GameLift initialization failed; shutting down. Use -NoGameLift to run locally."));
			FPlatformMisc::RequestExit(false);
			return;
		}
		return;
	}
#endif

	Mode = EVaesenGameLiftMode::Local;
	UE_LOG(LogVaesenGameLift, Log, TEXT("Running without GameLift: every player session is accepted."));
}

void UVaesenGameLiftSubsystem::Deinitialize()
{
	EndProcess();
	Super::Deinitialize();
}

bool UVaesenGameLiftSubsystem::IsAcceptingPlayers() const
{
	return Mode == EVaesenGameLiftMode::Local || bGameSessionActive;
}

bool UVaesenGameLiftSubsystem::AcceptPlayerSession(const FString& PlayerSessionId, FString& OutError)
{
	if (Mode == EVaesenGameLiftMode::Local)
	{
		return true;
	}

#if VAESEN_WITH_GAMELIFT
	if (!bGameSessionActive)
	{
		OutError = TEXT("Server is not hosting a game session yet.");
		return false;
	}
	if (PlayerSessionId.IsEmpty())
	{
		OutError = TEXT("Missing PlayerSessionId. Join the world through the API.");
		return false;
	}

	const FGameLiftGenericOutcome Outcome = VaesenGameLift::Sdk().AcceptPlayerSession(PlayerSessionId);
	if (!Outcome.IsSuccess())
	{
		OutError = FString::Printf(TEXT("Player session rejected: %s"), *Outcome.GetError().m_errorMessage);
		UE_LOG(LogVaesenGameLift, Warning, TEXT("AcceptPlayerSession(%s) failed: %s"), *PlayerSessionId, *Outcome.GetError().m_errorMessage);
		return false;
	}

	UE_LOG(LogVaesenGameLift, Log, TEXT("Accepted player session %s"), *PlayerSessionId);
#endif
	return true;
}

void UVaesenGameLiftSubsystem::RemovePlayerSession(const FString& PlayerSessionId)
{
#if VAESEN_WITH_GAMELIFT
	if (Mode == EVaesenGameLiftMode::Local || !bGameSessionActive || PlayerSessionId.IsEmpty())
	{
		return;
	}

	const FGameLiftGenericOutcome Outcome = VaesenGameLift::Sdk().RemovePlayerSession(PlayerSessionId);
	if (!Outcome.IsSuccess())
	{
		UE_LOG(LogVaesenGameLift, Warning, TEXT("RemovePlayerSession(%s) failed: %s"), *PlayerSessionId, *Outcome.GetError().m_errorMessage);
	}
#endif
}

void UVaesenGameLiftSubsystem::EndProcess()
{
#if VAESEN_WITH_GAMELIFT
	if (Mode == EVaesenGameLiftMode::Local || !ProcessParameters.IsValid() || bProcessEnded)
	{
		return;
	}

	bProcessEnded = true;
	VaesenGameLift::Sdk().ProcessEnding();
	VaesenGameLift::Sdk().Destroy();
	UE_LOG(LogVaesenGameLift, Log, TEXT("Told GameLift the process is ending."));
#endif
}

#if VAESEN_WITH_GAMELIFT

bool UVaesenGameLiftSubsystem::InitGameLift()
{
	FGameLiftServerSDKModule& Sdk = VaesenGameLift::Sdk();

	FGameLiftGenericOutcome InitOutcome;
	if (Mode == EVaesenGameLiftMode::Anywhere)
	{
		const TCHAR* CommandLine = FCommandLine::Get();
		FServerParameters Params;
		FParse::Value(CommandLine, TEXT("GameLiftWebSocketUrl="), Params.m_webSocketUrl);
		FParse::Value(CommandLine, TEXT("GameLiftFleetId="), Params.m_fleetId);
		FParse::Value(CommandLine, TEXT("GameLiftHostId="), Params.m_hostId);
		FParse::Value(CommandLine, TEXT("GameLiftAuthToken="), Params.m_authToken);
		FParse::Value(CommandLine, TEXT("GameLiftRegion="), Params.m_awsRegion);
		Params.m_processId = FString::Printf(TEXT("%s-%u"), *Params.m_hostId, FPlatformProcess::GetCurrentProcessId());

		if (Params.m_webSocketUrl.IsEmpty() || Params.m_fleetId.IsEmpty() || Params.m_hostId.IsEmpty() || Params.m_authToken.IsEmpty())
		{
			UE_LOG(LogVaesenGameLift, Error,
				TEXT("-GameLiftAnywhere needs -GameLiftWebSocketUrl, -GameLiftFleetId, -GameLiftHostId and -GameLiftAuthToken (see Scripts/RunAnywhereServer.bat)."));
			return false;
		}

		UE_LOG(LogVaesenGameLift, Log, TEXT("Initializing GameLift Anywhere: fleet %s, host %s"), *Params.m_fleetId, *Params.m_hostId);
		InitOutcome = Sdk.InitSDK(Params);
	}
	else
	{
		UE_LOG(LogVaesenGameLift, Log, TEXT("Initializing GameLift (managed fleet)"));
		InitOutcome = Sdk.InitSDK();
	}

	if (!InitOutcome.IsSuccess())
	{
		UE_LOG(LogVaesenGameLift, Error, TEXT("InitSDK failed: %s"), *InitOutcome.GetError().m_errorMessage);
		return false;
	}

	ProcessParameters = MakeShared<FProcessParameters>();
	TWeakObjectPtr<UVaesenGameLiftSubsystem> WeakThis(this);

	// GameLift invokes these callbacks on its own thread; copy what is needed and hop to the game thread.
	ProcessParameters->OnStartGameSession.BindLambda([WeakThis](Aws::GameLift::Server::Model::GameSession GameSession)
	{
		const FString SessionId = UTF8_TO_TCHAR(GameSession.GetGameSessionId());
		FString WorldId;
		FString Map;

		int PropertyCount = 0;
		const Aws::GameLift::Server::Model::GameProperty* Properties = GameSession.GetGameProperties(PropertyCount);
		for (int Index = 0; Index < PropertyCount; ++Index)
		{
			const FString Key = UTF8_TO_TCHAR(Properties[Index].GetKey());
			const FString Value = UTF8_TO_TCHAR(Properties[Index].GetValue());
			if (Key == VaesenGameLift::WorldIdProperty)
			{
				WorldId = Value;
			}
			else if (Key == VaesenGameLift::MapProperty)
			{
				Map = Value;
			}
		}

		AsyncTask(ENamedThreads::GameThread, [WeakThis, SessionId, WorldId, Map]()
		{
			if (UVaesenGameLiftSubsystem* This = WeakThis.Get())
			{
				This->HandleStartGameSession(SessionId, WorldId, Map);
			}
		});
	});

	ProcessParameters->OnTerminate.BindLambda([WeakThis]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UVaesenGameLiftSubsystem* This = WeakThis.Get())
			{
				This->HandleTerminate();
			}
		});
	});

	ProcessParameters->OnHealthCheck.BindLambda([]() { return true; });

	int32 Port = VaesenGameLift::DefaultPort;
	FParse::Value(FCommandLine::Get(), TEXT("port="), Port);
	ProcessParameters->port = Port;

	// GameLift uploads these files when the process ends (managed fleets).
	ProcessParameters->logParameters.Add(
		FPaths::ConvertRelativePathToFull(FPaths::ProjectLogDir() / FString(FApp::GetProjectName()) + TEXT(".log")));

	const FGameLiftGenericOutcome ReadyOutcome = Sdk.ProcessReady(*ProcessParameters);
	if (!ReadyOutcome.IsSuccess())
	{
		UE_LOG(LogVaesenGameLift, Error, TEXT("ProcessReady failed: %s"), *ReadyOutcome.GetError().m_errorMessage);
		return false;
	}

	UE_LOG(LogVaesenGameLift, Log, TEXT("ProcessReady on port %d; waiting for a game session."), Port);
	return true;
}

void UVaesenGameLiftSubsystem::HandleStartGameSession(const FString& InGameSessionId, const FString& InWorldId, const FString& Map)
{
	GameSessionId = InGameSessionId;
	WorldId = InWorldId;
	UE_LOG(LogVaesenGameLift, Log, TEXT("Starting game session %s for world '%s' (map %s)"), *GameSessionId, *WorldId, *Map);

	UWorld* World = GetGameInstance()->GetWorld();
	if (Map.IsEmpty() || (World && World->GetOutermost()->GetName() == Map))
	{
		ActivateGameSession();
		return;
	}

	// Only report the session active (and let players in) once the world's map has loaded.
	PendingMap = Map;
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVaesenGameLiftSubsystem::HandlePostLoadMap);
	World->ServerTravel(Map, /*bAbsolute*/ true);
}

void UVaesenGameLiftSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetOutermost()->GetName() != PendingMap)
	{
		return;
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	PostLoadMapHandle.Reset();
	PendingMap.Reset();
	ActivateGameSession();
}

void UVaesenGameLiftSubsystem::ActivateGameSession()
{
	const FGameLiftGenericOutcome Outcome = VaesenGameLift::Sdk().ActivateGameSession();
	bGameSessionActive = Outcome.IsSuccess();
	if (bGameSessionActive)
	{
		UE_LOG(LogVaesenGameLift, Log, TEXT("Game session %s is active; accepting players."), *GameSessionId);
	}
	else
	{
		UE_LOG(LogVaesenGameLift, Error, TEXT("ActivateGameSession failed: %s"), *Outcome.GetError().m_errorMessage);
	}
}

void UVaesenGameLiftSubsystem::HandleTerminate()
{
	UE_LOG(LogVaesenGameLift, Log, TEXT("GameLift requested termination."));
	bGameSessionActive = false;
	EndProcess();
	FPlatformMisc::RequestExit(false);
}

#endif // VAESEN_WITH_GAMELIFT
