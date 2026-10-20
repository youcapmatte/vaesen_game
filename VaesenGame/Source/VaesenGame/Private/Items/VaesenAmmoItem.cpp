#include "Items/VaesenAmmoItem.h"

#include "Combat/VaesenGameplayTags.h"

UVaesenAmmoItem::UVaesenAmmoItem()
{
	bStackable = true;
	MaxStackSize = 10;
	ItemTags.AddTag(VaesenTags::Item_Ammo);
	Damage.Entries.Emplace(VaesenTags::Damage_Piercing, 20.0f);
}

FVaesenDamagePacket UVaesenAmmoItem::GetDamage() const
{
	FVaesenDamagePacket Result = Damage;
	if (DamageBonus != 0 && Result.Entries.Num() > 0)
	{
		Result.Entries[0].Amount = FMath::Max(0.0f, Result.Entries[0].Amount + DamageBonus);
	}
	return Result;
}
