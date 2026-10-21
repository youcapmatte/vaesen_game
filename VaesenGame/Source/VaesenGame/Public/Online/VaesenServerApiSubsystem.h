#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Appearance/VaesenAppearance.h"
#include "VaesenServerApiSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogVaesenServerApi, Log, All);

/**
 * What a dedicated game server tells Vaesen.Api about the characters in its world (api/sessions/server/...):
 * when one leaves, and a regular heartbeat with the ones still here so their players' claims don't time out; and what a
 * joining character looks like. Reports are fire-and-forget: failures are logged, never block the game. Dedicated
 * servers only.
 */
UCLASS()
class VAESENGAME_API UVaesenServerApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** The character left this world (logged out or lost the connection). */
	void ReportCharacterLeft(const FString& CharacterId);

	/** A joining character's appearance (unset if it has none, or the API couldn't be reached). */
	void GetCharacterAppearance(const FString& CharacterId, TFunction<void(const FVaesenAppearance& Appearance)> OnComplete);

	/** The characters in this world right now. */
	void SendHeartbeat(const TArray<FString>& CharacterIds);

private:
	void Post(const FString& Path, const FString& Body);
};
