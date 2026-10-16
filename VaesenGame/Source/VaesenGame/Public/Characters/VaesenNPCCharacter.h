#pragma once

#include "CoreMinimal.h"
#include "Characters/VaesenBaseCharacter.h"
#include "VaesenNPCCharacter.generated.h"

/** Character driven by AI on the server. Possessed automatically whether placed in the level or spawned. */
UCLASS()
class VAESENGAME_API AVaesenNPCCharacter : public AVaesenBaseCharacter
{
	GENERATED_BODY()

public:
	AVaesenNPCCharacter();
};
