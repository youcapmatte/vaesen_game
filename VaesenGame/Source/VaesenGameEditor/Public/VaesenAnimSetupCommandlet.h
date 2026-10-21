#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VaesenAnimSetupCommandlet.generated.h"

/**
 * Sets up weapon animation from the animation packs in the project (run with the editor closed):
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenAnimSetup            retarget (once) and build everything
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenAnimSetup -analyze   retarget (once) and log poses, axes, sockets
 *
 * Rifle: "Shooter Rifle Animations" (PeaceSells) in /Game/RifleAnims, made on the UE4 mannequin. It's retargeted to
 * the UE5 Manny with auto-generated IK rigs and retargeter (the same as the editor's Retarget Animations window does)
 * into /Game/Vaesen/Animations/Rifle. Packs that aren't in the project are skipped. See VaesenAnimSetup.h.
 */
UCLASS()
class UVaesenAnimSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVaesenAnimSetupCommandlet();

	virtual int32 Main(const FString& Params) override;
};
