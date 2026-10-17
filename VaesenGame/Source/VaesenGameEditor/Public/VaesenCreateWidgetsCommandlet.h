#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VaesenCreateWidgetsCommandlet.generated.h"

/**
 * Creates the Vaesen Widget Blueprints with every control the C++ classes bind, named and laid out, using the
 * Vaesen controls (so they follow the UI style):
 *   /Game/Vaesen/UI/Guild/WBP_PickGuild, WBP_CreateGuild
 *   /Game/Vaesen/UI/CharacterSelect/WBP_CreateCharacter
 *   /Game/Vaesen/UI/Loading/WBP_LoadingScreen
 * and adds whatever WBP_CharacterSelect is missing (guild name and buttons, the three dialogs, the throbber) without
 * touching what's already there. Existing blueprints are never replaced.
 *
 * Run with the editor closed:
 *   UnrealEditor-Cmd.exe VaesenGame.uproject -run=VaesenCreateWidgets
 */
UCLASS()
class UVaesenCreateWidgetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVaesenCreateWidgetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
