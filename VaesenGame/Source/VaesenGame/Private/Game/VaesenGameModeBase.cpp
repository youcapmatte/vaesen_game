#include "Game/VaesenGameModeBase.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Engine/GameInstance.h"
#include "Game/VaesenGameState.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Online/VaesenGameLiftSubsystem.h"
#include "Online/VaesenServerApiSubsystem.h"
#include "TimerManager.h"
#include "Player/VaesenPlayerController.h"
#include "Player/VaesenPlayerState.h"
#include "UI/VaesenHUD.h"

const TCHAR* AVaesenGameModeBase::PlayerSessionIdOption = TEXT("PlayerSessionId");
const TCHAR* AVaesenGameModeBase::CharacterIdOption = TEXT("CharacterId");

namespace
{
	/** Only exists on dedicated servers; null in the editor (PIE) and on clients, where every player is accepted. */
	UVaesenGameLiftSubsystem* GetGameLift(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UVaesenGameLiftSubsystem>() : nullptr;
	}

	/** Only exists on dedicated servers. */
	UVaesenServerApiSubsystem* GetServerApi(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UVaesenServerApiSubsystem>() : nullptr;
	}
}

AVaesenGameModeBase::AVaesenGameModeBase()
{
	GameStateClass = AVaesenGameState::StaticClass();
	PlayerStateClass = AVaesenPlayerState::StaticClass();
	PlayerControllerClass = AVaesenPlayerController::StaticClass();
	DefaultPawnClass = AVaesenPlayerCharacter::StaticClass();
	HUDClass = AVaesenHUD::StaticClass();
}

void AVaesenGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (GetServerApi(GetWorld()))
	{
		GetWorldTimerManager().SetTimer(HeartbeatTimer, this, &AVaesenGameModeBase::SendHeartbeat, HeartbeatSeconds, true);
	}
}

void AVaesenGameModeBase::SendHeartbeat()
{
	UVaesenServerApiSubsystem* ServerApi = GetServerApi(GetWorld());
	if (!ServerApi)
	{
		return;
	}

	TArray<FString> CharacterIds;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const AVaesenPlayerState* PlayerState = It->IsValid() ? (*It)->GetPlayerState<AVaesenPlayerState>() : nullptr;
		if (PlayerState && !PlayerState->GetCharacterId().IsEmpty())
		{
			CharacterIds.Add(PlayerState->GetCharacterId());
		}
	}
	ServerApi->SendHeartbeat(CharacterIds);
}

void AVaesenGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		return;
	}

	if (UVaesenGameLiftSubsystem* GameLift = GetGameLift(GetWorld()))
	{
		const FString PlayerSessionId = UGameplayStatics::ParseOption(Options, PlayerSessionIdOption);
		GameLift->AcceptPlayerSession(PlayerSessionId, ErrorMessage);
	}
}

FString AVaesenGameModeBase::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	if (AVaesenPlayerState* PlayerState = NewPlayerController ? NewPlayerController->GetPlayerState<AVaesenPlayerState>() : nullptr)
	{
		PlayerState->SetPlayerSessionId(UGameplayStatics::ParseOption(Options, PlayerSessionIdOption));
		PlayerState->SetCharacterId(UGameplayStatics::ParseOption(Options, CharacterIdOption));
	}

	return Error;
}

void AVaesenGameModeBase::Logout(AController* Exiting)
{
	if (const AVaesenPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AVaesenPlayerState>() : nullptr)
	{
		if (UVaesenGameLiftSubsystem* GameLift = GetGameLift(GetWorld()))
		{
			GameLift->RemovePlayerSession(PlayerState->GetPlayerSessionId());
		}

		// Lets the API bring the character back here if its player returns soon, and frees it for the guild meanwhile.
		if (UVaesenServerApiSubsystem* ServerApi = GetServerApi(GetWorld()))
		{
			ServerApi->ReportCharacterLeft(PlayerState->GetCharacterId());
		}
	}

	Super::Logout(Exiting);
}
