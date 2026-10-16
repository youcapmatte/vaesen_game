#include "FrontEnd/VaesenLoginPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Core/VaesenSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Online/VaesenApiSubsystem.h"

void AVaesenLoginPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
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
	if (!Api || Api->IsLoggedIn())
	{
		// Already logged in: we came back to the login screen on purpose (e.g. logout), so don't loop.
		return;
	}

	UE_LOG(LogVaesenApi, Log, TEXT("-DevPlayer=%s: logging in automatically"), *DevPlayerId);
	Api->DevLogin(DevPlayerId, [WeakThis = TWeakObjectPtr<AVaesenLoginPlayerController>(this)](bool bSuccess, const FString& Error)
	{
		AVaesenLoginPlayerController* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (bSuccess)
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(This, UVaesenSettings::Get()->CharacterSelectMap);
		}
		else
		{
			UE_LOG(LogVaesenApi, Warning, TEXT("-DevPlayer auto-login failed: %s"), *Error);
		}
	});
}
