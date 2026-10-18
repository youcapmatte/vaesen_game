#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "VaesenAmmoItem.generated.h"

/** Cartridges, powder and shot: what firearms (UVaesenWeaponItem::AmmoType) load. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenAmmoItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenAmmoItem();

	/** Added to the weapon's damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	int32 DamageBonus = 0;
};
