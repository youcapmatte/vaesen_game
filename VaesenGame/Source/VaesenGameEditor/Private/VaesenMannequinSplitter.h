#pragma once

#include "CoreMinimal.h"

/**
 * Splits the UE mannequin (SKM_Manny) into modular parts for clothing/armor-style characters:
 *   /Game/Characters/Mannequins/Meshes/Modular/SKM_Manny_Head, _Torso, _Arms, _Legs, _Feet
 * Every triangle goes to the part whose bones move it most (head+neck, spine+clavicles, upper arm..fingers,
 * pelvis+thighs+calves, feet+balls), and the open seams are closed without adding vertices, so a part hidden on its own
 * (e.g. the head in first person) leaves a closed surface instead of a hole. All parts share Manny's skeleton and have
 * a single LOD. Existing parts are left alone.
 */
namespace VaesenMannequinSplitter
{
	/** Creates the parts that don't exist yet. Returns false if any part failed. */
	bool SplitMannequin();

	extern const TCHAR* const PartsFolder;
}
