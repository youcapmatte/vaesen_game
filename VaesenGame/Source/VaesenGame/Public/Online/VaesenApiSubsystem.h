#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Online/VaesenApiTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "VaesenApiSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVaesenApi, Log, All);

/**
 * Client-side gateway to Vaesen.Api (base URL from UVaesenSettings). Holds the signed-in player's session
 * and attaches its token to every request. Lives on the GameInstance, so the session survives map changes.
 * Not created on dedicated servers.
 */
UCLASS()
class VAESENGAME_API UVaesenApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FGuildsCallback = TFunction<void(bool bSuccess, const TArray<FVaesenDevGuild>& Guilds, const FString& Error)>;
	using FResultCallback = TFunction<void(bool bSuccess, const FString& Error)>;
	using FProgressCallback = TFunction<void(const FText& Message)>;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * Account login from the login screen. OnProgress receives one status message per step ("Authenticating...");
	 * each replaces the previous one. Pass no OnProgress to skip the step messages (e.g. -DevPlayer).
	 *
	 * PARTLY FAKE FOR NOW: the account is looked up by username in the API ("Verifying account..."; unknown
	 * usernames fail), but the password isn't checked and the other steps only show a message for a moment.
	 * Will be replaced by Auth0; callers don't need to change.
	 */
	void Login(const FString& UsernameOrEmail, const FString& Password, FProgressCallback OnProgress, FResultCallback OnComplete);

	/** Stops a login in progress. Its callbacks are not called. */
	void CancelLogin();

	/** Development only: guilds and their characters to choose from (GET /api/dev/guilds). */
	void GetDevGuilds(FGuildsCallback OnComplete);

	/** Development only: play as a seed character; gets the API token for it (POST /api/dev/login). Requires Login first. */
	void SelectDevCharacter(const FString& DevPlayerId, FResultCallback OnComplete);

	void Logout();

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	bool IsLoggedIn() const { return Session.IsLoggedIn(); }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	bool HasCharacter() const { return Session.HasCharacter(); }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	const FVaesenPlayerSession& GetSession() const { return Session; }

private:
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Verb, const FString& Path) const;

	/** Turns a failed or non-2xx response into a readable error; empty when the request succeeded. */
	static FString GetError(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	void RunNextLoginStep();
	void ScheduleNextLoginStep();
	void FinishLogin(bool bSuccess, const FString& Error);

	/** Looks up the pending username in the API; on success stores the account and calls OnFound. */
	void VerifyAccount(TFunction<void()> OnFound);

	FVaesenPlayerSession Session;

	/** Login in progress. */
	FString PendingAccountName;
	FVaesenAccount PendingAccount;
	TArray<FText> PendingLoginSteps;
	/** Remaining step count at which the account lookup runs; INDEX_NONE once done. */
	int32 VerifyAccountAtRemainingSteps = INDEX_NONE;
	/** Incremented by every login/cancel, so responses of an abandoned login are ignored. */
	int32 LoginAttempt = 0;
	FProgressCallback LoginProgress;
	FResultCallback LoginComplete;
	FTimerHandle LoginStepTimer;
};
