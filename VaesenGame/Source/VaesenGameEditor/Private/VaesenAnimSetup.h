#pragma once

#include "CoreMinimal.h"

/** Weapon animation setup from the animation packs (see UVaesenAnimSetupCommandlet). */
namespace VaesenAnimSetup
{
	/** Where the rifle pack's animations go once retargeted to the UE5 Manny. */
	extern const TCHAR* RifleFolder;

	/** Retargets the rifle pack to Manny, if it's in the project and not done yet. Returns whether the retargeted set exists. */
	bool RetargetRiflePack();

	/** Logs what the setup needs to know: blend space axes, lengths, the pack's sockets, hand and head positions in key poses. */
	void AnalyzeRifle();

	/** Montages, the rifle locomotion anim Blueprint, the character anim Blueprint, sockets and category defaults. */
	void BuildRifle();
}
