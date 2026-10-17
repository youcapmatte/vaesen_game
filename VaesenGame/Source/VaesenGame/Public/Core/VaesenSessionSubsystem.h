#pragma once

#include "CoreMinimal.h"
#include "Online/VaesenApiTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaesenSessionSubsystem.generated.h"

/** A guild the player has chosen (character select). */
USTRUCT(BlueprintType)
struct FVaesenGuildRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString Name;

	bool IsSet() const { return !Id.IsEmpty(); }
};

/** The character the player is playing as. */
USTRUCT(BlueprintType)
struct FVaesenCharacterRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString Id;

	UPROPERTY(BlueprintReadOnly, Category = "Vaesen|Session")
	FString Name;

	bool IsSet() const { return !Id.IsEmpty(); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenSessionChangedSignature);

/**
 * Who is playing on this client: the signed-in account and what they picked (guild, character), plus the API token.
 * Lives on the GameInstance, so it survives every level change, including travel to a world server; cleared on
 * logout or when the game closes. Not created on dedicated servers.
 *
 * Read it anywhere: GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>(), or in Blueprints
 * "Get Vaesen Session Subsystem". UVaesenApiSubsystem fills it in as the player logs in and picks a character.
 */
UCLASS()
class VAESENGAME_API UVaesenSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	bool IsLoggedIn() const { return !Account.Id.IsEmpty(); }

	/** True once a character is picked and the API has issued a token for it. */
	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	bool HasCharacter() const { return SelectedCharacter.IsSet() && !AccessToken.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	const FVaesenAccount& GetAccount() const { return Account; }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	const FVaesenGuildRef& GetSelectedGuild() const { return SelectedGuild; }

	UFUNCTION(BlueprintPure, Category = "Vaesen|Session")
	const FVaesenCharacterRef& GetSelectedCharacter() const { return SelectedCharacter; }

	/** Forget everything: account, selections and token. */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Session")
	void Logout();

	/** Broadcast whenever anything in the session changes (login, selection, logout). */
	UPROPERTY(BlueprintAssignable, Category = "Vaesen|Session")
	FVaesenSessionChangedSignature OnSessionChanged;

	/** A new login: replaces the account and clears earlier selections and token. */
	void SetAccount(const FVaesenAccount& InAccount);

	void SetSelectedGuild(const FVaesenGuildRef& Guild);

	/** The character to play, with the API token issued for it. */
	void SetSelectedCharacter(const FVaesenCharacterRef& Character, const FString& InAccessToken);

	/** Bearer token for Vaesen.Api; empty until a character is picked. Not exposed to Blueprints. */
	const FString& GetAccessToken() const { return AccessToken; }

private:
	UPROPERTY()
	FVaesenAccount Account;

	UPROPERTY()
	FVaesenGuildRef SelectedGuild;

	UPROPERTY()
	FVaesenCharacterRef SelectedCharacter;

	FString AccessToken;
};
