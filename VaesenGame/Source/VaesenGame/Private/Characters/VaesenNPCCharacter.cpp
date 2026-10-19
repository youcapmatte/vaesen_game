#include "Characters/VaesenNPCCharacter.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VaesenAttributeSet.h"
#include "AIController.h"

AVaesenNPCCharacter::AVaesenNPCCharacter()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Minimal: nobody predicts for an NPC, so only what everyone needs replicates.
	NPCAbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	NPCAbilitySystem->SetIsReplicated(true);
	NPCAbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	NPCAttributes = CreateDefaultSubobject<UVaesenAttributeSet>(TEXT("Attributes"));
}

void AVaesenNPCCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitAbilitySystem(NPCAbilitySystem, this);
}
