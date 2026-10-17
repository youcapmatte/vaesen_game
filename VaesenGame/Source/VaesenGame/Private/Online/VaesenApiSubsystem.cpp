#include "Online/VaesenApiSubsystem.h"

#include "Core/VaesenSessionSubsystem.h"
#include "Core/VaesenSettings.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "VaesenApi"

DEFINE_LOG_CATEGORY(LogVaesenApi);

namespace VaesenApiEnvelope
{
	/**
	 * Vaesen.Api wraps every response as {"success": bool, "result": ..., "errorMessage": "...", "errorCode": "..."}.
	 * Returns "result" on success; otherwise null with OutError set to the API's message.
	 */
	TSharedPtr<FJsonValue> ParseResult(const FString& Body, FString& OutError)
	{
		TSharedPtr<FJsonObject> Envelope;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Envelope) || !Envelope.IsValid())
		{
			OutError = TEXT("Unexpected response from the API.");
			return nullptr;
		}

		bool bSuccess = false;
		if (!Envelope->TryGetBoolField(TEXT("success"), bSuccess) || !bSuccess)
		{
			FString Message;
			Envelope->TryGetStringField(TEXT("errorMessage"), Message);
			OutError = Message.IsEmpty() ? TEXT("Request failed.") : Message;
			return nullptr;
		}

		TSharedPtr<FJsonValue> Result = Envelope->TryGetField(TEXT("result"));
		if (!Result.IsValid())
		{
			OutError = TEXT("Unexpected response from the API.");
		}
		return Result;
	}

	/** Reads the envelope's "result" object into a USTRUCT. */
	template <typename TStruct>
	bool ReadObject(const FString& Body, TStruct& Out, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Result = ParseResult(Body, OutError);
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (OutError.IsEmpty() && (!Result->TryGetObject(Object) || !FJsonObjectConverter::JsonObjectToUStruct((*Object).ToSharedRef(), &Out)))
		{
			OutError = TEXT("Unexpected response from the API.");
		}
		return OutError.IsEmpty();
	}

	/** Reads the envelope's "result" array into USTRUCTs. */
	template <typename TStruct>
	bool ReadArray(const FString& Body, TArray<TStruct>& Out, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Result = ParseResult(Body, OutError);
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (OutError.IsEmpty() && (!Result->TryGetArray(Items) || !FJsonObjectConverter::JsonArrayToUStruct(*Items, &Out)))
		{
			OutError = TEXT("Unexpected response from the API.");
		}
		return OutError.IsEmpty();
	}

	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Json));
		return Json;
	}
}

namespace VaesenFakeLogin
{
	/** Each step message stays up for StepSeconds +/- StepJitterSeconds. */
	constexpr float StepSeconds = 1.0f;
	constexpr float StepJitterSeconds = 0.3f;

	/** Index of the step that really looks the account up in the API. */
	constexpr int32 VerifyAccountStep = 2;

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
		// Nobody is watching (e.g. -DevPlayer): just look the account up.
		VerifyAccount([this]() { FinishLogin(true, FString()); });
		return;
	}

	PendingLoginSteps = VaesenFakeLogin::GetSteps();
	VerifyAccountAtRemainingSteps = PendingLoginSteps.Num() - VaesenFakeLogin::VerifyAccountStep;
	RunNextLoginStep();
}

void UVaesenApiSubsystem::VerifyAccount(TFunction<void()> OnFound)
{
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CreateRequest(
		TEXT("GET"), TEXT("/api/accounts/by-username/") + FGenericPlatformHttp::UrlEncode(PendingAccountName));

	const int32 Attempt = LoginAttempt;
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Attempt, OnFound = MoveTemp(OnFound)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			if (Attempt != LoginAttempt)
			{
				return; // Cancelled or superseded while the request was in flight.
			}

			FString Error = GetError(Req, Response, bConnected);
			FVaesenAccount Account;
			if (Error.IsEmpty())
			{
				const TSharedPtr<FJsonValue> Result = VaesenApiEnvelope::ParseResult(Response->GetContentAsString(), Error);
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (Error.IsEmpty() && (!Result->TryGetObject(Object) || !FJsonObjectConverter::JsonObjectToUStruct((*Object).ToSharedRef(), &Account)))
				{
					Error = TEXT("Unexpected response from the API.");
				}
			}

			if (!Error.IsEmpty())
			{
				FinishLogin(false, Error);
				return;
			}

			PendingAccount = Account;
			OnFound();
		});

	Request->ProcessRequest();
}

void UVaesenApiSubsystem::CancelLogin()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(LoginStepTimer);
	}
	++LoginAttempt;
	PendingLoginSteps.Reset();
	PendingAccountName.Reset();
	PendingAccount = FVaesenAccount();
	VerifyAccountAtRemainingSteps = INDEX_NONE;
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

	const bool bVerifyNow = PendingLoginSteps.Num() == VerifyAccountAtRemainingSteps;
	LoginProgress(PendingLoginSteps[0]);
	PendingLoginSteps.RemoveAt(0);

	if (bVerifyNow)
	{
		// The real step: wait for the API before moving on.
		VerifyAccountAtRemainingSteps = INDEX_NONE;
		VerifyAccount([this]() { ScheduleNextLoginStep(); });
		return;
	}

	ScheduleNextLoginStep();
}

void UVaesenApiSubsystem::ScheduleNextLoginStep()
{
	const float Delay = VaesenFakeLogin::StepSeconds
		+ FMath::FRandRange(-VaesenFakeLogin::StepJitterSeconds, VaesenFakeLogin::StepJitterSeconds);
	GetGameInstance()->GetTimerManager().SetTimer(
		LoginStepTimer, FTimerDelegate::CreateUObject(this, &UVaesenApiSubsystem::RunNextLoginStep), Delay, false);
}

void UVaesenApiSubsystem::FinishLogin(bool bSuccess, const FString& Error)
{
	if (bSuccess)
	{
		GetSession()->SetAccount(PendingAccount);
		UE_LOG(LogVaesenApi, Log, TEXT("Logged in as account '%s' (%s); password not checked yet"), *PendingAccount.Username, *PendingAccount.Id);
	}

	// Reset before calling back: the callback may start another login or change level.
	FResultCallback Complete = MoveTemp(LoginComplete);
	CancelLogin();
	if (Complete)
	{
		Complete(bSuccess, Error);
	}
}

void UVaesenApiSubsystem::GetMyGuilds(FGuildsCallback OnComplete)
{
	const FString AccountId = GetSession()->GetAccount().Id;
	if (AccountId.IsEmpty())
	{
		OnComplete(false, {}, TEXT("Log in first."));
		return;
	}

	const auto Request = CreateRequest(TEXT("GET"), TEXT("/api/guilds/by-account/") + FGenericPlatformHttp::UrlEncode(AccountId));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			TArray<FVaesenGuild> Guilds;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty())
			{
				VaesenApiEnvelope::ReadArray(Response->GetContentAsString(), Guilds, Error);
			}
			OnComplete(Error.IsEmpty(), Guilds, Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::CreateGuild(const FString& Name, FGuildCallback OnComplete)
{
	const FString AccountId = GetSession()->GetAccount().Id;
	if (AccountId.IsEmpty())
	{
		OnComplete(false, FVaesenGuild(), TEXT("Log in first."));
		return;
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), Name);
	Body->SetStringField(TEXT("createdByAccountId"), AccountId);

	const auto Request = CreateRequest(TEXT("POST"), TEXT("/api/guilds"));
	Request->SetContentAsString(VaesenApiEnvelope::ToJson(Body));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			FVaesenGuild Guild;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty())
			{
				VaesenApiEnvelope::ReadObject(Response->GetContentAsString(), Guild, Error);
			}
			OnComplete(Error.IsEmpty(), Guild, Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::GetGuildCharacters(const FString& GuildId, FCharactersCallback OnComplete)
{
	const auto Request = CreateRequest(TEXT("GET"), FString::Printf(TEXT("/api/guilds/%s/characters"), *FGenericPlatformHttp::UrlEncode(GuildId)));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			TArray<FVaesenCharacter> Characters;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty())
			{
				VaesenApiEnvelope::ReadArray(Response->GetContentAsString(), Characters, Error);
			}
			OnComplete(Error.IsEmpty(), Characters, Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::GetCharacterClasses(FStringsCallback OnComplete)
{
	const auto Request = CreateRequest(TEXT("GET"), TEXT("/api/characters/classes"));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			TArray<FString> Classes;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty())
			{
				const TSharedPtr<FJsonValue> Result = VaesenApiEnvelope::ParseResult(Response->GetContentAsString(), Error);
				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (Error.IsEmpty() && Result->TryGetArray(Items))
				{
					for (const TSharedPtr<FJsonValue>& Item : *Items)
					{
						Classes.Add(Item->AsString());
					}
				}
			}
			OnComplete(Error.IsEmpty(), Classes, Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::IsCharacterNameTaken(const FString& Name, FNameTakenCallback OnComplete)
{
	const auto Request = CreateRequest(TEXT("GET"), TEXT("/api/characters/by-name/") + FGenericPlatformHttp::UrlEncode(Name));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			// 404 means nobody has the name: that's the answer, not an error.
			if (bConnected && Response.IsValid() && Response->GetResponseCode() == EHttpResponseCodes::NotFound)
			{
				OnComplete(true, false, FString());
				return;
			}

			const FString Error = GetError(Req, Response, bConnected);
			OnComplete(Error.IsEmpty(), Error.IsEmpty(), Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::CreateCharacter(const FString& GuildId, const FString& FirstName, const FString& LastName, const FString& Class, FCharacterCallback OnComplete)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("firstName"), FirstName);
	Body->SetStringField(TEXT("lastName"), LastName);
	if (!Class.IsEmpty())
	{
		Body->SetStringField(TEXT("class"), Class);
	}
	Body->SetStringField(TEXT("createdByAccountId"), GetSession()->GetAccount().Id);

	const auto Request = CreateRequest(TEXT("POST"), FString::Printf(TEXT("/api/guilds/%s/characters"), *FGenericPlatformHttp::UrlEncode(GuildId)));
	Request->SetContentAsString(VaesenApiEnvelope::ToJson(Body));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			FVaesenCharacter Character;
			FString Error = GetError(Req, Response, bConnected);
			if (Error.IsEmpty())
			{
				VaesenApiEnvelope::ReadObject(Response->GetContentAsString(), Character, Error);
			}
			OnComplete(Error.IsEmpty(), Character, Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::PlayCharacter(const FVaesenCharacter& Character, FResultCallback OnComplete)
{
	const FString AccountId = GetSession()->GetAccount().Id;
	if (AccountId.IsEmpty())
	{
		OnComplete(false, TEXT("Log in first."));
		return;
	}

	// 1. Become the character's player in its guild (whoever played it before stops).
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("accountId"), AccountId);
	const auto Request = CreateRequest(TEXT("PUT"), FString::Printf(TEXT("/api/guilds/%s/characters/%s/player"),
		*FGenericPlatformHttp::UrlEncode(Character.GuildId), *FGenericPlatformHttp::UrlEncode(Character.Id)));
	Request->SetContentAsString(VaesenApiEnvelope::ToJson(Body));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Character, OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected) mutable
		{
			const FString Error = GetError(Req, Response, bConnected);
			if (!Error.IsEmpty())
			{
				OnComplete(false, Error);
				return;
			}

			// 2. Get the token for playing it.
			RequestCharacterToken(Character, MoveTemp(OnComplete));
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::RequestCharacterToken(const FVaesenCharacter& Character, FResultCallback OnComplete)
{
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("accountId"), GetSession()->GetAccount().Id);
	Body->SetStringField(TEXT("characterId"), Character.Id);

	const auto Request = CreateRequest(TEXT("POST"), TEXT("/api/dev/login"));
	Request->SetContentAsString(VaesenApiEnvelope::ToJson(Body));
	Request->OnProcessRequestComplete().BindWeakLambda(this,
		[this, Character, OnComplete = MoveTemp(OnComplete)](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bConnected)
		{
			FString Error = GetError(Req, Response, bConnected);
			FVaesenDevLoginResponse Login;
			if (Error.IsEmpty())
			{
				VaesenApiEnvelope::ReadObject(Response->GetContentAsString(), Login, Error);
			}

			if (Error.IsEmpty())
			{
				FVaesenCharacterRef Selected;
				Selected.Id = Character.Id;
				Selected.Name = Character.Name;
				GetSession()->SetSelectedCharacter(Selected, Login.AccessToken);
				UE_LOG(LogVaesenApi, Log, TEXT("Playing as %s (%s)"), *Character.Name, *Character.Id);
			}

			OnComplete(Error.IsEmpty(), Error);
		});
	Request->ProcessRequest();
}

void UVaesenApiSubsystem::Logout()
{
	CancelLogin();
	GetSession()->Logout();
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
	const FString& AccessToken = GetSession()->GetAccessToken();
	if (!AccessToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + AccessToken);
	}
	return Request;
}

UVaesenSessionSubsystem* UVaesenApiSubsystem::GetSession() const
{
	// Both live on the same GameInstance and are client-only, so the session always exists here.
	return GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>();
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

		// Prefer the API's own message ("Username is taken.") over the bare status code.
		FString Message;
		VaesenApiEnvelope::ParseResult(Response->GetContentAsString(), Message);
		return Message.IsEmpty() || Message == TEXT("Unexpected response from the API.")
			? FString::Printf(TEXT("Request failed (%d)."), Code)
			: Message;
	}

	return FString();
}

#undef LOCTEXT_NAMESPACE
