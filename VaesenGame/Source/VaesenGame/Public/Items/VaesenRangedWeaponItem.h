#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenWeaponItem.h"
#include "VaesenRangedWeaponItem.generated.h"

class UVaesenAmmoItem;
class UVaesenInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenAmmoChanged);

/**
 * Guns, bows and crossbows. The ammo defines what flies (damage, projectile, whether it sticks); the weapon defines how
 * it's launched: shot type, action, reload style, casings, timings, spread. Everything is set in Class Defaults.
 *
 * Period guidance: Krag-Jørgensen and other bolt actions eject on cycle; rolling blocks on reload (breech opened);
 * revolvers on reload; muzzle-loaders never.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenRangedWeaponItem : public UVaesenWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenRangedWeaponItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsRanged() const override { return true; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	EVaesenShotType ShotType = EVaesenShotType::InstantHit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	EVaesenCasingEjection CasingEjection = EVaesenCasingEjection::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	EVaesenReloadStyle ReloadStyle = EVaesenReloadStyle::Magazine;

	/** Whether a cycle step (bolt, lever) follows each shot; Draw for bows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged")
	EVaesenWeaponAction Action = EVaesenWeaponAction::SingleShot;

	/** The ammo it loads. Or, with AcceptedAmmoTag, any ammo with that tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ammo")
	TSubclassOf<UVaesenAmmoItem> AcceptedAmmo;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ammo", meta = (Categories = "Item"))
	FGameplayTag AcceptedAmmoTag;

	/** Rounds loaded at once (1 for single shots and muzzle-loaders). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ammo", meta = (ClampMin = "1", ClampMax = "50"))
	int32 MagazineSize = 1;

	/** Fastest it can fire again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float FireInterval = 0.5f;

	/** Magazine and muzzle-loading: the whole reload. SingleRound and Cylinder: each round. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float ReloadTime = 2.0f;

	/** Working the bolt or lever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Timing", meta = (ClampMin = "0.05", Units = "s", EditCondition = "Action == EVaesenWeaponAction::BoltAction || Action == EVaesenWeaponAction::LeverAction", EditConditionHides))
	float CycleTime = 0.8f;

	/** Projectiles leave the muzzle this fast (bows: at full draw). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ballistics", meta = (ClampMin = "100", Units = "CentimetersPerSecond", EditCondition = "ShotType == EVaesenShotType::Projectile", EditConditionHides))
	float LaunchSpeed = 30000.0f;

	/** Scales the ammo's damage packet (a long barrel hits harder). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ballistics", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.0f;

	/** Full damage up to here; it falls to half at MaxRange. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ballistics", meta = (ClampMin = "0", Units = "cm"))
	float EffectiveRange = 3000.0f;

	/** Instant hits reach this far. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Ballistics", meta = (ClampMin = "0", Units = "cm"))
	float MaxRange = 15000.0f;

	/** Cone half-angle when firing from the hip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Spread", meta = (ClampMin = "0", ClampMax = "30", Units = "Degrees"))
	float SpreadHip = 4.0f;

	/** Cone half-angle while aiming (right mouse). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Spread", meta = (ClampMin = "0", ClampMax = "30", Units = "Degrees"))
	float SpreadAimed = 0.5f;

	/** Added at full running speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Spread", meta = (ClampMin = "0", ClampMax = "30", Units = "Degrees"))
	float SpreadMoving = 3.0f;

	/** Field of view while aiming. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Spread", meta = (ClampMin = "10", ClampMax = "120", Units = "Degrees"))
	float AimFieldOfView = 60.0f;

	/** Bows: time from nocked to full draw. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Bow", meta = (ClampMin = "0.05", Units = "s", EditCondition = "Category == EVaesenWeaponCategory::Bow || Category == EVaesenWeaponCategory::Crossbow", EditConditionHides))
	float DrawTime = 1.0f;

	/** Bows and crossbows: damage and launch speed multiplier at full draw (an early release is weaker). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Bow", meta = (ClampMin = "0.1", EditCondition = "Category == EVaesenWeaponCategory::Bow || Category == EVaesenWeaponCategory::Crossbow", EditConditionHides))
	float MaxDrawStrength = 1.0f;

	/** Bows: taking an arrow and nocking it, before the draw starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Bow", meta = (ClampMin = "0", Units = "s", EditCondition = "Category == EVaesenWeaponCategory::Bow", EditConditionHides))
	float NockTime = 0.35f;

	/** Bows: weakest release that still shoots (share of a full draw). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Bow", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "Category == EVaesenWeaponCategory::Bow", EditConditionHides))
	float MinDrawToShoot = 0.15f;

	/** Bows: stamina per second while drawn and held. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Bow", meta = (ClampMin = "0", EditCondition = "Category == EVaesenWeaponCategory::Bow", EditConditionHides))
	float DrawStaminaPerSecond = 6.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Effects")
	FVaesenFireEffects Effects;

	/** The spent case thrown out (when CasingEjection isn't None). Empty = a small grey-box cylinder. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Effects", meta = (EditCondition = "CasingEjection != EVaesenCasingEjection::None", EditConditionHides))
	TObjectPtr<UStaticMesh> CasingMesh;

	/** Chance (0..1) a shot fails: a dull click, or a flash in the pan. Off by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Wear", meta = (ClampMin = "0", ClampMax = "1"))
	float MisfireChance = 0.0f;

	/** Durability lost per shot, so an old rifle can fail. Off by default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ranged|Wear", meta = (ClampMin = "0"))
	float WearPerShot = 0.0f;

	// --- State (replicated with the item, kept when it goes back into the inventory) ---

	UFUNCTION(BlueprintPure, Category = "Ranged")
	int32 GetLoadedRounds() const { return LoadedRounds; }

	/** A spent case is in the chamber: the bolt or lever must be worked before the next shot. */
	UFUNCTION(BlueprintPure, Category = "Ranged")
	bool NeedsCycle() const { return bNeedsCycle; }

	/** What's loaded (its damage is used). Null if nothing is. */
	UFUNCTION(BlueprintPure, Category = "Ranged")
	TSubclassOf<UVaesenAmmoItem> GetLoadedAmmo() const { return LoadedAmmo; }

	/** Bows shoot straight from the inventory (an arrow is nocked per shot) instead of loading rounds. */
	UFUNCTION(BlueprintPure, Category = "Ranged")
	bool IsBow() const { return Action == EVaesenWeaponAction::Draw; }

	/** Whether this action needs a bolt or lever worked between shots. */
	bool HasCycleStep() const { return Action == EVaesenWeaponAction::BoltAction || Action == EVaesenWeaponAction::LeverAction; }

	/** Reloading can stop between rounds. */
	bool CanInterruptReload() const { return ReloadStyle == EVaesenReloadStyle::SingleRound || ReloadStyle == EVaesenReloadStyle::Cylinder; }

	/** The fire montage: the aimed one while aiming (if there is one), else the plain one. */
	UAnimMontage* GetFireMontage(bool bAiming) const
	{
		UAnimMontage* Aimed = bAiming ? GetMontage(&FVaesenWeaponMontages::FireAimed) : nullptr;
		return Aimed ? Aimed : GetMontage(&FVaesenWeaponMontages::Fire);
	}

	/** The reload montage: from empty when nothing is loaded (if there is one), else the plain one. */
	UAnimMontage* GetReloadMontage() const
	{
		UAnimMontage* Empty = LoadedRounds <= 0 ? GetMontage(&FVaesenWeaponMontages::ReloadEmpty) : nullptr;
		return Empty ? Empty : GetMontage(&FVaesenWeaponMontages::Reload);
	}

	/** Whether it can load this kind of ammo. */
	bool Accepts(TSubclassOf<UVaesenItem> AmmoClass) const;

	/** The ammo it would load from Inventory (what's loaded first, then AcceptedAmmo, then any with the tag). Null if none there. */
	TSubclassOf<UVaesenAmmoItem> FindAmmoIn(const UVaesenInventoryComponent* Inventory) const;

	/** How many of the ammo it loads the inventory holds (pouches included), for the HUD. */
	UFUNCTION(BlueprintPure, Category = "Ranged")
	int32 CountAmmoIn(const UVaesenInventoryComponent* Inventory) const;

	// --- Server ---

	void SetLoadedRounds(int32 Rounds, TSubclassOf<UVaesenAmmoItem> Ammo);
	void SetNeedsCycle(bool bInNeedsCycle);

	/** Loaded rounds or chamber changed (on every machine). */
	UPROPERTY(BlueprintAssignable, Category = "Ranged")
	FVaesenAmmoChanged OnAmmoChanged;

protected:
	virtual TSubclassOf<AVaesenWeaponActor> GetDefaultActorClass() const override;

private:
	UFUNCTION()
	void OnRep_Ammo();

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 LoadedRounds = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	bool bNeedsCycle = false;

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	TSubclassOf<UVaesenAmmoItem> LoadedAmmo;
};
