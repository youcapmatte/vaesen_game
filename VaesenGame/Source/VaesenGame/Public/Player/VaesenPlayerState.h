#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "VaesenPlayerState.generated.h"

UCLASS()
class VAESENGAME_API AVaesenPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	/** GameLift player session this player joined with. Server-only; not replicated. */
	const FString& GetPlayerSessionId() const { return PlayerSessionId; }
	void SetPlayerSessionId(const FString& InPlayerSessionId) { PlayerSessionId = InPlayerSessionId; }

	/** The character this player plays ("character-{guid}", the "CharacterId" URL option). Server-only; not replicated. */
	const FString& GetCharacterId() const { return CharacterId; }
	void SetCharacterId(const FString& InCharacterId) { CharacterId = InCharacterId; }

protected:
	/** Carry the player session across seamless travel within the same server. */
	virtual void CopyProperties(APlayerState* PlayerState) override;

private:
	FString PlayerSessionId;
	FString CharacterId;
};
