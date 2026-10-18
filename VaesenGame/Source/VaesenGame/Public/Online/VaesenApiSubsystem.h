#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Online/VaesenApiTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"
#include "VaesenApiSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVaesenApi, Log, All);

class UVaesenSessionSubsystem;

/**
 * Client-side gateway to Vaesen.Api (base URL from UVaesenSettings): the HTTP calls and nothing else.
 * Results that describe who is playing (account, chosen character, token) go into UVaesenSessionSubsystem,
 * and every request carries the session's token. Not created on dedicated servers.
 */
UCLASS()
class VAESENGAME_API UVaesenApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FGuildsCallback = TFunction<void(bool bSuccess, const TArray<FVaesenGuild>& Guilds, const FString& Error)>;
	using FGuildCallback = TFunction<void(bool bSuccess, const FVaesenGuild& Guild, const FString& Error)>;
	using FCharactersCallback = TFunction<void(bool bSuccess, const TArray<FVaesenCharacter>& Characters, const FString& Error)>;
	using FRosterCallback = TFunction<void(bool bSuccess, const TArray<FVaesenRosterEntry>& Roster, const FString& Error)>;
	using FCharacterCallback = TFunction<void(bool bSuccess, const FVaesenCharacter& Character, const FString& Error)>;
	using FStringsCallback = TFunction<void(bool bSuccess, const TArray<FString>& Values, const FString& Error)>;
	using FNameTakenCallback = TFunction<void(bool bSuccess, bool bTaken, const FString& Error)>;
	using FResultCallback = TFunction<void(bool bSuccess, const FString& Error)>;
	using FEnterWorldCallback = TFunction<void(bool bSuccess, const FVaesenEnterWorldResponse& Response, const FString& Error)>;
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

	/** The signed-in account's guilds (GET /api/guilds/by-account/{accountId}). */
	void GetMyGuilds(FGuildsCallback OnComplete);

	/** Creates a guild with the signed-in account as its first member and guild master (POST /api/guilds). */
	void CreateGuild(const FString& Name, FGuildCallback OnComplete);

	/** A guild's characters (GET /api/guilds/{guildId}/characters). */
	void GetGuildCharacters(const FString& GuildId, FCharactersCallback OnComplete);

	/** A guild's characters with who is playing each one (GET /api/guilds/{guildId}/roster). */
	void GetGuildRoster(const FString& GuildId, FRosterCallback OnComplete);

	/** The classes a character can have (GET /api/characters/classes). */
	void GetCharacterClasses(FStringsCallback OnComplete);

	/** Whether a character with this name exists (GET /api/characters/by-name/{name}). */
	void IsCharacterNameTaken(const FString& Name, FNameTakenCallback OnComplete);

	/** Creates a character in the guild, made by the signed-in account (POST /api/guilds/{guildId}/characters). */
	void CreateCharacter(const FString& GuildId, const FString& FirstName, const FString& LastName, const FString& Class, FCharacterCallback OnComplete);

	/**
	 * Plays a character: assigns it to the signed-in account in its guild, then gets the account's play token for it
	 * (development login for now). On success the session holds the character and token.
	 */
	void PlayCharacter(const FVaesenCharacter& Character, FResultCallback OnComplete);

	/**
	 * Asks where the character (just claimed with PlayCharacter) enters the world: back where it just was, else its
	 * guild's hub (POST /api/sessions/enter). Status "starting" means ask again shortly.
	 */
	void EnterWorld(const FString& CharacterId, FEnterWorldCallback OnComplete);

	/** Cancels a pending login and clears the session. */
	void Logout();

private:
	UVaesenSessionSubsystem* GetSession() const;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Verb, const FString& Path) const;

	/** Gets the dev play token for a character the signed-in account is assigned to. */
	void RequestCharacterToken(const FVaesenCharacter& Character, FResultCallback OnComplete);

	/** Turns a failed or non-2xx response into a readable error; empty when the request succeeded. */
	static FString GetError(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	void RunNextLoginStep();
	void ScheduleNextLoginStep();
	void FinishLogin(bool bSuccess, const FString& Error);

	/** Looks up the pending username in the API; on success stores the account and calls OnFound. */
	void VerifyAccount(TFunction<void()> OnFound);

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
