#include "FrontEnd/VaesenLoginPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/VaesenMusicSubsystem.h"
#include "Core/VaesenSessionSubsystem.h"
#include "Core/VaesenSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/VaesenApiSubsystem.h"
#include "Sound/SoundBase.h"
#include "UI/Loading/VaesenLoadingScreenSubsystem.h"

void AVaesenLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		if (UVaesenMusicSubsystem* Music = GetGameInstance()->GetSubsystem<UVaesenMusicSubsystem>())
		{
			Music->PlayMusic(UVaesenSettings::Get()->LoginMusic.LoadSynchronous());
		}
		TryDevAutoLogin();
	}
}

TSubclassOf<UUserWidget> AVaesenLoginPlayerController::GetDefaultScreenWidgetClass() const
{
	return UVaesenSettings::Get()->LoginScreenWidget.LoadSynchronous();
}

void AVaesenLoginPlayerController::TryDevAutoLogin()
{
	FString DevPlayerId;
	if (!FParse::Value(FCommandLine::Get(), TEXT("DevPlayer="), DevPlayerId) || DevPlayerId.IsEmpty())
	{
		return;
	}

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	const UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>();
	if (!Api || !Session || Session->IsLoggedIn())
	{
		// Already logged in: we came back to the login screen on purpose (e.g. logout), so don't loop.
		return;
	}

	UE_LOG(LogVaesenApi, Log, TEXT("-DevPlayer=%s: skipping the login screen"), *DevPlayerId);
	// No progress callback: skip the fake login steps so restarts stay fast.
	Api->Login(DevPlayerId, TEXT("dev"), nullptr, [WeakThis = TWeakObjectPtr<AVaesenLoginPlayerController>(this)](bool bSuccess, const FString& Error)
	{
		AVaesenLoginPlayerController* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (bSuccess)
		{
			This->GetGameInstance()->GetSubsystem<UVaesenLoadingScreenSubsystem>()->TravelToMap(UVaesenSettings::Get()->CharacterSelectMap, /*bShowLoadingScreen*/ false);
		}
		else
		{
			UE_LOG(LogVaesenApi, Warning, TEXT("-DevPlayer auto-login failed (is there an account with that username?): %s"), *Error);
		}
	});
}
