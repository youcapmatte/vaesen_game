#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VaesenSoundSetupCommandlet.generated.h"

/**
 * Sets up weapon sounds from the sound packs in the project (run with the editor closed):
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenSoundSetup            make what's missing
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenSoundSetup -rebuild   make the cues and sets again
 *
 * "Distance Gun Sounds: WeaponVerse1" (OctaveCore) in /Game/WeaponVerse1: each gun's shot recorded at 5, 25, 100 and
 * 300 m, bolts, draws, drops, bullet cracks. Into /Game/Vaesen/Audio/Weapons it builds Sound Cues (the shot blended by
 * the listener's distance, the bolt timed to the hand working it, random takes) and a Sound Set per weapon, and gives
 * the weapons their sets. The pack itself stays out of git (installed from Fab); packs that aren't there are skipped.
 */
UCLASS()
class UVaesenSoundSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVaesenSoundSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
