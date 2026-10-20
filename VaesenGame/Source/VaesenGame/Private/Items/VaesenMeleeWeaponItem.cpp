#include "Items/VaesenMeleeWeaponItem.h"

UVaesenMeleeWeaponItem::UVaesenMeleeWeaponItem()
{
	Category = EVaesenWeaponCategory::OneHandedSword;
	SheathSocket = TEXT("Sheath_HipLeft");
}

const FVaesenMeleeAttack* UVaesenMeleeWeaponItem::GetAttack(bool bHeavy) const
{
	if (bOverrideAttacks)
	{
		return bHeavy ? (bHasHeavyAttack ? &HeavyAttack : nullptr) : &LightAttack;
	}
	const FVaesenCategoryDefaults& Defaults = GetCategoryDefaults();
	return bHeavy ? (Defaults.bHasHeavyAttack ? &Defaults.HeavyAttack : nullptr) : &Defaults.LightAttack;
}

const FVaesenBlockProfile& UVaesenMeleeWeaponItem::GetBlockProfile() const
{
	return bOverrideBlockProfile ? BlockProfile : Super::GetBlockProfile();
}
