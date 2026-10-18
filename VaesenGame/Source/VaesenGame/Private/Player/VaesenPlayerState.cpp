#include "Player/VaesenPlayerState.h"

void AVaesenPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (AVaesenPlayerState* VaesenPlayerState = Cast<AVaesenPlayerState>(PlayerState))
	{
		VaesenPlayerState->PlayerSessionId = PlayerSessionId;
		VaesenPlayerState->CharacterId = CharacterId;
	}
}
