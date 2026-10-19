#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "VaesenPlayerState.generated.h"

class UAbilitySystemComponent;
class UVaesenAttributeSet;

/**
 * A player in the world. Holds the player's ability system (Gameplay Ability System) and vitals, so they survive the
 * character being replaced (death, respawn); the character connects to them when possessed.
 */
UCLASS()
class VAESENGAME_API AVaesenPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AVaesenPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category = "Abilities")
	UVaesenAttributeSet* GetAttributes() const { return Attributes; }

	/** GameLift player session this player joined with. Server-only; not replicated. */
	const FString& GetPlayerSessionId() const { return PlayerSessionId; }
	void SetPlayerSessionId(const FString& InPlayerSessionId) { PlayerSessionId = InPlayerSessionId; }

	/** The character this player plays ("character-{guid}", the "CharacterId" URL option). Server-only; not replicated. */
	const FString& GetCharacterId() const { return CharacterId; }
	void SetCharacterId(const FString& InCharacterId) { CharacterId = InCharacterId; }

protected:
	/** Carry the player session across seamless travel within the same server. */
	virtual void CopyProperties(APlayerState* PlayerState) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> AbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UVaesenAttributeSet> Attributes;

private:
	FString PlayerSessionId;
	FString CharacterId;
};
