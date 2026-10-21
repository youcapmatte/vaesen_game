#pragma once

#include "CoreMinimal.h"

/**
 * The combat test setup, built by VaesenCreateWidgetsCommandlet. Only what's missing is created, so running it again is
 * safe:
 *   - grey-box weapon meshes with their sockets (Grip, LeftGrip, Sight, Stock, Muzzle, Eject, TraceStart, TraceEnd) and the
 *     mannequin's weapon and sheath sockets (hand_r_weapon, hand_l_weapon, Sheath_*);
 *   - physical materials per surface, DA_WeaponCategoryDefaults, DA_ImpactEffects, DA_WeaponSounds_Default;
 *   - GE_Damage, GE_StaminaCost, GE_Stagger, GE_StaminaRegen and the GA_ Blueprints (granted by the player);
 *   - combat input (attack, heavy, aim/block, reload, throw, holster, swap) in IMC_Default;
 *   - test weapons, ammo, a cartridge box, throwables, the training dummies, and the map L_CombatTest.
 */
namespace VaesenCombatAssets
{
	void SetupCombat();
}
