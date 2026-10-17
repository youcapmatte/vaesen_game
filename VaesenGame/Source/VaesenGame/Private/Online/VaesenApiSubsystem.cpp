#include "Online/VaesenApiSubsystem.h"

#include "Core/VaesenSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"

#define LOCTEXT_NAMESPACE "VaesenApi"

DEFINE_LOG_CATEGORY(LogVaesenApi);

namespace VaesenFakeLogin
{
	/** Each step message stays up for StepSeconds +/- StepJitterSeconds. */
	constexpr float StepSeconds = 1.0f;
	constexpr float StepJitterSeconds = 0.3f;

	TArray<FText> GetSteps()
	{
		return {
			LOCTEXT("StepConnecting", "Connecting..."),
			LOCTEXT("StepAuthenticating", "Authenticating..."),
			LOCTEXT("StepVerifyingAccount", "Verifying account..."),
			LOCTEXT("StepCheckingSubscription", "Checking subscription..."),
			LOCTEXT("StepRetrievingRealms", "Retrieving realm list..."),
			LOCTEXT("StepRetrievingCharacters", "Retrieving character list..."),
			LOCTEXT("StepSuccess", "Success!"),
		};
	}
}

bool UVaesenApiSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer();
}

void UVaesenApiSubsystem::Login(const FString& UsernameOrEmail, const FString& Password, FProgressCallback OnProgress, FResultCallback OnComplete)
{
	CancelLogin();

	LoginProgress = MoveTemp(OnProgress);
	LoginComplete = MoveTemp(OnComplete);
	PendingAccountName = UsernameOrEmail.TrimStartAndEnd();

	if (PendingAccountName.IsEmpty() || Password.IsEmpty())
	{
		FinishLogin(false, TEXT("Enter your account name and password."));
		return;
	}

	if (!LoginProgress)
	{
		FinishLogin(true, FString());
		return;
	}

	PendingLoginSteps = VaesenFakeLogin::GetSteps();
	RunNextLoginStep();
}

void UVaesenApiSubsystem::CancelLogin()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(LoginStepTimer);
	}
	PendingLoginSteps.Reset();
	PendingAccountName.Reset();
	LoginProgress = nullptr;
	LoginComplete = nullptr;
}

void UVaesenApiSubsystem::RunNextLoginStep()
{
	if (PendingLoginSteps.IsEmpty())
	{
		FinishLogin(true, FString());
		return;
	}

	LoginProgress(PendingLoginSteps[0]);
	PendingLoginSteps.RemoveAt(0);

	const float Delay = VaesenFakeLogin::StepSeconds
		+ FMath::FRandRange(-VaesenFakeLogin::StepJitterSeconds, VaesenFakeLogin::StepJitterSeconds);
	GetGameInstance()->GetTimerManager().SetTimer(
		LoginStepTimer, FTimerDelegate::CreateUObject(this, &UVaesenApiSubsystem::RunNextLoginStep), Delay, false);
}

void UVaesenApiSubsystem::FinishLogin(bool bSuccess, const FString& Error)
{
	if (bSuccess)
	{
		Session = FVaesenPlayerSession();
		Session.AccountName = PendingAccountName;
		UE_LOG(LogVaesenApi, Log, TEXT("Logged in as account '%s' (fake login, no password check)"), *Session.AccountName);
	}

	// Reset before calling back: the callback may start another login or change level.
	FResultCallback Complete = MoveTemp(LoginComplete);
	CancelLogin();
	if (Complete)
	{
		Complete(bSuccess, Error);
	}
}

void UVaesenApiSubsystem::GetDevGuilds(FGuildsCallback OnComplete)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("GET"), TEXT("/api/v1/dev/guilds"));

	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			TArray<FVaesenDevGuild> Guilds;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty() && !FJsonObjectConverter::JsonArrayStringToUStruct(Response->GetContentAsString(), &Guilds))
			{
				Error = TEXT("Unexpected response from the API.");
			}
			OnComplete(Error.IsEmpty(), Guilds, Error);
		});

	Request->ProcessRequest();
}

void UVaesenApiSubsystem::SelectDevCharacter(const FString& DevPlayerId, FResultCallback OnComplete)
{
	if (!Session.IsLoggedIn())
	{
		OnComplete(false, TEXT("Log in first."));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(TEXT("POST"), TEXT("/api/v1/dev/login"));

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("playerId"), DevPlayerId);
	FString BodyString;
	FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&BodyString));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			FString Error = GetError(Req, Response, bConnected);
			FVaesenDevLoginResponse Login;
			if (Error.IsEmpty() && !FJsonObjectConverter::JsonObjectStringToUStruct(Response->GetContentAsString(), &Login))
			{
				Error = TEXT("Unexpected response from the API.");
			}

			if (Error.IsEmpty())
			{
				Session.AccessToken = Login.AccessToken;
				Session.PlayerId = Login.PlayerId;
				Session.PlayerName = Login.PlayerName;
				Session.GuildId = Login.GuildId;
				UE_LOG(LogVaesenApi, Log, TEXT("Playing as %s (%s)"), *Session.PlayerName, *Session.PlayerId);
			}

			OnComplete(Error.IsEmpty(), Error);
		});

	Request->ProcessRequest();
}

void UVaesenApiSubsystem::Logout()
{
	CancelLogin();
	Session = FVaesenPlayerSession();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UVaesenApiSubsystem::CreateRequest(const FString& Verb, const FString& Path) const
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	FString BaseUrl = UVaesenSettings::Get()->ApiBaseUrl;
	BaseUrl.RemoveFromEnd(TEXT("/"));

	Request->SetURL(BaseUrl + Path);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (Session.HasCharacter())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Session.AccessToken);
	}
	return Request;
}

FString UVaesenApiSubsystem::GetError(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		const FString Url = Request.IsValid() ? Request->GetURL() : FString();
		UE_LOG(LogVaesenApi, Warning, TEXT("Could not reach %s"), *Url);
		return TEXT("Could not reach the server. Is Vaesen.Api running?");
	}

	const int32 Code = Response->GetResponseCode();
	if (Code < 200 || Code >= 300)
	{
		UE_LOG(LogVaesenApi, Warning, TEXT("%s %s -> %d: %s"),
			*Request->GetVerb(), *Request->GetURL(), Code, *Response->GetContentAsString());
		return FString::Printf(TEXT("Request failed (%d)."), Code);
	}

	return FString();
}

#undef LOCTEXT_NAMESPACE
