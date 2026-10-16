#include "Characters/VaesenNPCCharacter.h"

#include "AIController.h"

AVaesenNPCCharacter::AVaesenNPCCharacter()
{
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
