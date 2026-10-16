#pragma once

#include "CoreMinimal.h"
#include "VaesenApiTypes.generated.h"

/** A player from the API's development seed data (GET /api/v1/dev/guilds). */
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

/** The signed-in player on this client. Filled by a successful login. */
USTRUCT(BlueprintType)
struct FVaesenPlayerSession
{
	GENERATED_BODY()

	/** Bearer token sent to Vaesen.Api. */
	UPROPERTY()
	FString AccessToken;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString GuildId;

	bool IsValid() const { return !AccessToken.IsEmpty(); }
};

/** Response of POST /api/v1/dev/login. */
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
