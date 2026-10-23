#include "Items/VaesenMeleeWeaponItem.h"

UVaesenMeleeWeaponItem::UVaesenMeleeWeaponItem()
{
	Category = EVaesenWeaponCategory::OneHandedSword;
	SheathSocket = TEXT("Sheath_HipLeft");
}

bool UVaesenMeleeWeaponItem::GetAttack(bool bHeavy, FVaesenMeleeAttack& OutAttack) const
{
	const FVaesenCategoryDefaults& Defaults = GetCategoryDefaults();
	const FVaesenMeleeAttack* Found = nullptr;
	if (bOverrideAttacks)
	{
		Found = bHeavy ? (bHasHeavyAttack ? &HeavyAttack : nullptr) : &LightAttack;
	}
	else
	{
		Found = bHeavy ? (Defaults.bHasHeavyAttack ? &Defaults.HeavyAttack : nullptr) : &Defaults.LightAttack;
	}
	if (!Found)
	{
		return false;
	}
	OutAttack = *Found;

	// In both hands: the category's own two-handed attack, or this one hitting harder.
	if (IsInTwoHandedGrip())
	{
		const FVaesenTwoHandedGrip& TwoHanded = Defaults.TwoHanded;
		if (TwoHanded.bOverrideAttacks)
		{
			OutAttack = bHeavy ? TwoHanded.HeavyAttack : TwoHanded.LightAttack;
		}
		else
		{
			OutAttack.Damage = OutAttack.Damage.Scaled(TwoHanded.DamageScale);
			OutAttack.StaminaCost *= TwoHanded.StaminaScale;
			OutAttack.Knockback *= TwoHanded.KnockbackScale;
			// The two-handed attack montage (when the category has one) rather than the one-handed one.
			if (TwoHanded.Montages.Attack)
			{
				OutAttack.Montage = nullptr;
			}
		}
	}
	return true;
}

const FVaesenBlockProfile& UVaesenMeleeWeaponItem::GetBlockProfile() const
{
	if (IsInTwoHandedGrip() && GetCategoryDefaults().TwoHanded.bOverrideBlockProfile)
	{
		return GetCategoryDefaults().TwoHanded.BlockProfile;
	}
	return bOverrideBlockProfile ? BlockProfile : Super::GetBlockProfile();
}
