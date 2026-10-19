#include "Player/VaesenPlayerState.h"

#include "AbilitySystemComponent.h"
#include "Abilities/VaesenAttributeSet.h"

AVaesenPlayerState::AVaesenPlayerState()
{
	// Mixed: the owning player gets full effect information (for prediction and UI), others just what they need.
	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	Attributes = CreateDefaultSubobject<UVaesenAttributeSet>(TEXT("Attributes"));

	// Player states update rarely by default; abilities and vitals need it more often.
	SetNetUpdateFrequency(100.0f);
}

void AVaesenPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AVaesenPlayerState* VaesenPlayerState = Cast<AVaesenPlayerState>(PlayerState))
	{
		VaesenPlayerState->PlayerSessionId = PlayerSessionId;
		VaesenPlayerState->CharacterId = CharacterId;
	}
}
