#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenDamageTypes.h"
#include "Items/VaesenItem.h"
#include "VaesenAmmoItem.generated.h"

/**
 * Cartridges, powder and ball, arrows, bolts: what ranged weapons load (UVaesenRangedWeaponItem::AcceptedAmmo). The
 * ammo defines what flies: its damage packet (bullets and arrows Piercing; a blunderbuss load Piercing + Blunt), its
 * projectile, and whether it sticks and can be picked up again (arrows). Tagged Item.Ammo, so cartridge boxes take it.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenAmmoItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenAmmoItem();

	/** What one round deals (before the weapon's multiplier, the bow's draw strength and range falloff). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	FVaesenDamagePacket Damage;

	/** Added to the first entry of Damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	int32 DamageBonus = 0;

	/** Shown in flight for projectile weapons. Empty = a small grey-box sphere. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Projectile")
	TObjectPtr<UStaticMesh> ProjectileMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Projectile")
	FVector ProjectileScale = FVector(0.05);

	/** 1 = normal gravity; lower flies flatter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Projectile", meta = (ClampMin = "0"))
	float GravityScale = 1.0f;

	/** Stays stuck in what it hits (arrows, bolts). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Projectile")
	bool bSticksInSurfaces = false;

	/** Once stuck, can be picked up again (it becomes a pickup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo|Projectile", meta = (EditCondition = "bSticksInSurfaces"))
	bool bRecoverable = false;

	/** The packet with DamageBonus added. */
	FVaesenDamagePacket GetDamage() const;
};
