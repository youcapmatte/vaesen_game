#pragma once

#include "CoreMinimal.h"
#include "VaesenApiTypes.generated.h"

/** A player from the API's development seed data (GET /api/dev/guilds). */
USTRUCT(BlueprintType)
struct FVaesenDevPlayer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Dev")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Dev")
	FString Name;
};

/** A guild and its players from the API's development seed data. */
USTRUCT(BlueprintType)
struct FVaesenDevGuild
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Dev")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Dev")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Dev")
	TArray<FVaesenDevPlayer> Players;
};

/** The account's subscription, as returned by the API. */
USTRUCT(BlueprintType)
struct FVaesenAccountSubscription
{
	GENERATED_BODY()

	/** None, Trialing, Active, PastDue or Canceled. */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FString Status;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FString Plan;
};

/** A player account (GET /api/accounts/...). */
USTRUCT(BlueprintType)
struct FVaesenAccount
{
	GENERATED_BODY()

	/** "account-{guid}". */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FString Username;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FString Email;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	FVaesenAccountSubscription Subscription;
};

/**
 * Who is signed in on this client: the account (after the login screen) and, once one is picked on
 * the character select screen, the character and its API token.
 */
USTRUCT(BlueprintType)
struct FVaesenPlayerSession
{
	GENERATED_BODY()

	/** The account's username, as stored in the API (the login lookup is case-insensitive). */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString AccountName;

	/** "account-{guid}" of the signed-in account. */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString AccountId;

	/** Bearer token sent to Vaesen.Api; set when a character is selected. */
	UPROPERTY()
	FString AccessToken;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString GuildId;

	bool IsLoggedIn() const { return !AccountName.IsEmpty(); }
	bool HasCharacter() const { return !AccessToken.IsEmpty(); }
};

/** Response of POST /api/dev/login. */
USTRUCT()
struct FVaesenDevLoginResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FString AccessToken;

	UPROPERTY()
	int32 ExpiresIn = 0;

	UPROPERTY()
	FString PlayerId;

	UPROPERTY()
	FString PlayerName;

	UPROPERTY()
	FString GuildId;
};
