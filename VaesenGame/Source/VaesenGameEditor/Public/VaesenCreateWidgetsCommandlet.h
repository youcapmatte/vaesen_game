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
 * and adds whatever WBP_CharacterSelect is missing (guild name and buttons, a character panel, the three dialogs, the
 * throbber). The character panel replaces plain PlayerComboBox / PlayButton / StatusText controls with Vaesen ones of
 * the same names and removes the obsolete GuildComboBox; everything else there is left alone. Other existing
 * blueprints are never replaced.
 * Also creates the data-only player Blueprints (defaults only, no graph logic): BP_VaesenPlayerCharacter (UE
 * mannequin, ABP_Manny, template input actions), BP_VaesenPlayerController (IMC_Default) and BP_VaesenGameMode;
 * the camera zoom input; the modular body (Manny split into Head/Torso/Hands/Legs/Feet, set on the character); and the
 * interaction (outline material and interact key on the character, WBP_InteractionCard, BP_InteractableTest to try it on); BP_Pickup (one
 * pickup for every item) and test items; the inventory screen, I to open it and BP_LootChest;
 * a herb pouch and a herb to try pouches with; the equipment panel and test clothing (boots, gloves, cloak); and the
 * round minimap (M_Minimap, WBP_Minimap).
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
