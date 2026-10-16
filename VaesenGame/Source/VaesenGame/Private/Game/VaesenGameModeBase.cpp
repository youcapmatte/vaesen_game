#include "Game/VaesenGameModeBase.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Engine/GameInstance.h"
#include "Game/VaesenGameState.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Online/VaesenGameLiftSubsystem.h"
#include "Player/VaesenPlayerController.h"
#include "Player/VaesenPlayerState.h"

const TCHAR* AVaesenGameModeBase::PlayerSessionIdOption = TEXT("PlayerSessionId");

namespace
{
	/** Only exists on dedicated servers; null in the editor (PIE) and on clients, where every player is accepted. */
	UVaesenGameLiftSubsystem* GetGameLift(const UWorld* World)
	{
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UVaesenGameLiftSubsystem>() : nullptr;
	}
}

AVaesenGameModeBase::AVaesenGameModeBase()
{
	GameStateClass = AVaesenGameState::StaticClass();
	PlayerStateClass = AVaesenPlayerState::StaticClass();
	PlayerControllerClass = AVaesenPlayerController::StaticClass();
	DefaultPawnClass = AVaesenPlayerCharacter::StaticClass();
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
	}

	return Error;
}

void AVaesenGameModeBase::Logout(AController* Exiting)
{
	if (UVaesenGameLiftSubsystem* GameLift = GetGameLift(GetWorld()))
	{
		if (const AVaesenPlayerState* PlayerState = Exiting ? Exiting->GetPlayerState<AVaesenPlayerState>() : nullptr)
		{
			GameLift->RemovePlayerSession(PlayerState->GetPlayerSessionId());
		}
	}

	Super::Logout(Exiting);
}
