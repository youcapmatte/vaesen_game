#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * The gameplay tags combat uses from C++. They're registered natively (no ini needed); more tags under Damage.* can be
 * added in Project Settings > Gameplay Tags without code (no protection attribute = no protection against them).
 */
namespace VaesenTags
{
	// Damage types: an attack's damage packet is a list of (one of these, amount).
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Slashing);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Piercing);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Blunt);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Iron);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Silver);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Fire);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Salt);

	// States on the ability system component; abilities block and cancel each other through these.
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ParryWindow);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Staggered);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aiming);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Reloading);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Cycling);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_WeaponDrawn);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SwitchingWeapon);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Attacking);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Throwing);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_DrawingBow);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	// Ability tags, for cancelling groups of abilities.
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Attack);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Fire);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Melee);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Reload);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Block);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Throw);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Aim);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Holster);

	// SetByCaller data on GE_Damage and GE_StaminaCost.
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_StaminaCost);
	/** 1 = the hit ignores blocking (a vaesen's grab). */
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Unblockable);
	/** 1 = a projectile (only shields block it). */
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Projectile);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Knockback);

	// GameplayCues for hits, played on every machine.
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Slashing);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Piercing);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Blunt);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Blocked);
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Hit_Parried);

	/** Ammunition (a cartridge box pouch accepts it). */
	VAESENGAME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Item_Ammo);

	/** The hit cue for a damage type (GameplayCue.Hit.Slashing for Damage.Slashing); GameplayCue.Hit for others. */
	VAESENGAME_API FGameplayTag GetHitCueFor(const FGameplayTag& DamageType);
}
