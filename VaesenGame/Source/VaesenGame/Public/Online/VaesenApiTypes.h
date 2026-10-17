#pragma once

#include "CoreMinimal.h"
#include "VaesenApiTypes.generated.h"

/** Who plays one of a guild's characters right now. */
USTRUCT(BlueprintType)
struct FVaesenCharacterAssignment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString CharacterId;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString AccountId;
};

/** A guild (GET /api/guilds/...). */
USTRUCT(BlueprintType)
struct FVaesenGuild
{
	GENERATED_BODY()

	/** "guild-{guid}". */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString Name;

	/** The account that leads the guild (its creator). */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString GuildMasterAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	TArray<FString> MemberAccountIds;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	TArray<FString> CharacterIds;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	TArray<FVaesenCharacterAssignment> CharacterAssignments;
};

/** A character owned by a guild (GET /api/guilds/{id}/characters, /api/characters/...). */
USTRUCT(BlueprintType)
struct FVaesenCharacter
{
	GENERATED_BODY()

	/** "character-{guid}". */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString GuildId;

	/** "{FirstName} {LastName}"; unique. */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString FirstName;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString LastName;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	FString Class;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Character")
	int32 HealthPoints = 0;
};

/** One of a guild's characters and who is playing it (GET /api/guilds/{id}/roster). */
USTRUCT(BlueprintType)
struct FVaesenRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FVaesenCharacter Character;

	/** The account playing it; empty when the character is free. */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString PlayerAccountId;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Guild")
	FString PlayerUsername;

	bool IsFree() const { return PlayerAccountId.IsEmpty(); }
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

	/** Ids of the guilds this account is a member of ("guild-{guid}"). */
	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Account")
	TArray<FString> GuildIds;
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

	UPROPERTY()
	FString AccountId;

	UPROPERTY()
	FString CharacterId;
};
