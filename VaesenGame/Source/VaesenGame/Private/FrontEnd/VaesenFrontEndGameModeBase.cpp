#include "FrontEnd/VaesenFrontEndGameModeBase.h"

#include "GameFramework/Pawn.h"

AVaesenFrontEndGameModeBase::AVaesenFrontEndGameModeBase()
{
	// A bare pawn: something to possess and view from, with no movement or collision.
	DefaultPawnClass = APawn::StaticClass();
}
