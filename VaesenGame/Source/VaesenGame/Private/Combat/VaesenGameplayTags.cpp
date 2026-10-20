#include "Combat/VaesenGameplayTags.h"

namespace VaesenTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "Damage", "Damage types");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Slashing, "Damage.Slashing", "Blades swung: swords, axes, a dagger's slash");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Piercing, "Damage.Piercing", "Points and bullets: thrusts, arrows, shot");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Blunt, "Damage.Blunt", "Clubs, hammers, fists, stones");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Iron, "Damage.Iron", "Cold iron (reserved for vaesen)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Silver, "Damage.Silver", "Silver (reserved for vaesen)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Fire, "Damage.Fire", "Fire (reserved for vaesen)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Salt, "Damage.Salt", "Salt (reserved for vaesen)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Holding block");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ParryWindow, "State.ParryWindow", "Just pressed block: a hit now is parried");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Briefly can't act (parried)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Aiming a ranged weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Reloading, "State.Reloading", "Reloading");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Cycling, "State.Cycling", "Working a bolt or lever");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_WeaponDrawn, "State.WeaponDrawn", "A weapon is in the hand");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SwitchingWeapon, "State.SwitchingWeapon", "Drawing or sheathing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "Swinging or firing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Throwing, "State.Throwing", "Aiming a throw");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_DrawingBow, "State.DrawingBow", "Nocking, drawing or holding a bow");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Out of health");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon, "Ability.Weapon", "Uses the drawn weapon (cancelled when it's sheathed)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Attack, "Ability.Attack", "An attack (cancelled by stagger)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Fire, "Ability.Fire", "Firing a ranged weapon");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Melee, "Ability.Melee", "A melee attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Reload, "Ability.Reload", "Reloading or cycling");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Block, "Ability.Block", "Blocking");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Throw, "Ability.Throw", "Throwing");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Aim, "Ability.Aim", "Aiming");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Holster, "Ability.Holster", "Drawing and sheathing");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StaminaCost, "Data.StaminaCost", "SetByCaller: stamina an ability costs");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Unblockable, "Data.Unblockable", "SetByCaller: 1 = can't be blocked");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Projectile, "Data.Projectile", "SetByCaller: 1 = a projectile");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Knockback, "Data.Knockback", "SetByCaller: knockback strength");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit, "GameplayCue.Hit", "Something was hit");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Slashing, "GameplayCue.Hit.Slashing", "Hit by a blade");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Piercing, "GameplayCue.Hit.Piercing", "Hit by a point or bullet");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Blunt, "GameplayCue.Hit.Blunt", "Hit by something blunt");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Blocked, "GameplayCue.Hit.Blocked", "A hit absorbed by a block");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit_Parried, "GameplayCue.Hit.Parried", "A hit parried");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Ammo, "Item.Ammo", "Ammunition: cartridges, powder and ball, arrows, bolts");

	FGameplayTag GetHitCueFor(const FGameplayTag& DamageType)
	{
		if (DamageType.MatchesTag(Damage_Slashing)) { return GameplayCue_Hit_Slashing; }
		if (DamageType.MatchesTag(Damage_Piercing)) { return GameplayCue_Hit_Piercing; }
		if (DamageType.MatchesTag(Damage_Blunt))    { return GameplayCue_Hit_Blunt; }
		return GameplayCue_Hit;
	}
}
