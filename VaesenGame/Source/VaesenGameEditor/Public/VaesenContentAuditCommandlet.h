#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VaesenContentAuditCommandlet.generated.h"

/**
 * Keeps third-party packs out of git: finds what the project uses of each pack, and moves just that (with what it
 * needs inside the pack: meshes' materials, materials' textures, cues' waves) into /Game/Vaesen/ThirdParty/<Pack>/...,
 * so the packs themselves can be ignored (reinstalled from Fab when the setup tools need them). Run with the editor
 * closed:
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenContentAudit          list what's used of each pack, and its size
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenContentAudit -move    move it into /Game/Vaesen/ThirdParty
 *
 * "Used" means referenced (hard or soft) from outside the pack: our content, maps, the template, and the paths the
 * code names (VaesenContentAudit::CodePaths). Packs: -packs=A,B (folders under /Game); default every known pack.
 */
UCLASS()
class UVaesenContentAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVaesenContentAuditCommandlet();

	virtual int32 Main(const FString& Params) override;
};
