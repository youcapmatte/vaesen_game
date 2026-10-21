#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;

/**
 * Splits the UE mannequin (SKM_Manny) into modular parts for clothing/armor-style characters:
 *   /Game/Characters/Mannequins/Meshes/Modular/SKM_Manny_Head, _Torso (with the arms), _Hands, _Legs, _Feet
 * Every triangle goes to the part whose bones move it most (head+neck, spine+clavicles, upper arm..fingers,
 * pelvis+thighs+calves, feet+balls), and the open seams are closed without adding vertices, so a part hidden on its own
 * (e.g. the head in first person) leaves a closed surface instead of a hole. All parts share Manny's skeleton and have
 * a single LOD. Existing parts are left alone.
 */
namespace VaesenMannequinSplitter
{
	/** Creates the parts that don't exist yet. Returns false if any part failed. */
	bool SplitMannequin();

	/**
	 * The same split for any skeletal mesh with UE5 mannequin bone names (a MetaHuman body too): parts named
	 * <Prefix>_Head/_Torso/_Hands/_Legs/_Feet in OutFolder. Without bKeepHead there's no head part and the neck stays
	 * with the torso (a MetaHuman's head is its own face mesh). Existing parts are left alone.
	 */
	bool SplitSkeletalMesh(USkeletalMesh* Source, const FString& OutFolder, const FString& Prefix, bool bKeepHead);

	extern const TCHAR* const PartsFolder;
}
