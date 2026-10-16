#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Online/VaesenApiTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
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
	using FLoginCallback = TFunction<void(bool bSuccess, const FString& Error)>;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Development only: guilds and players to log in as (GET /api/v1/dev/guilds). */
	void GetDevGuilds(FGuildsCallback OnComplete);

	/** Development only: log in as a seed player without a password (POST /api/v1/dev/login). */
	void DevLogin(const FString& DevPlayerId, FLoginCallback OnComplete);

	void Logout();

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	bool IsLoggedIn() const { return Session.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	const FVaesenPlayerSession& GetSession() const { return Session; }

private:
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateRequest(const FString& Verb, const FString& Path) const;

	/** Turns a failed or non-2xx response into a readable error; empty when the request succeeded. */
	static FString GetError(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	FVaesenPlayerSession Session;
};
