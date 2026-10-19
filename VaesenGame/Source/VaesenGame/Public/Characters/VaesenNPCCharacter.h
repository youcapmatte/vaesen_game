#pragma once

#include "CoreMinimal.h"
#include "Characters/VaesenBaseCharacter.h"
#include "VaesenNPCCharacter.generated.h"

class UAbilitySystemComponent;
class UVaesenAttributeSet;

/**
 * Character driven by AI on the server. Possessed automatically whether placed in the level or spawned. Has its own
 * ability system and vitals (on the character, since NPCs have no player state).
 */
UCLASS()
class VAESENGAME_API AVaesenNPCCharacter : public AVaesenBaseCharacter
{
	GENERATED_BODY()

public:
	AVaesenNPCCharacter();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAbilitySystemComponent> NPCAbilitySystem;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UVaesenAttributeSet> NPCAttributes;
};
