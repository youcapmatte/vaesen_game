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

/**
 * Who is signed in on this client: the account (after the login screen) and, once one is picked on
 * the character select screen, the character and its API token.
 */
USTRUCT(BlueprintType)
struct FVaesenPlayerSession
{
	GENERATED_BODY()

	/** Username or email entered on the login screen. */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString AccountName;

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
