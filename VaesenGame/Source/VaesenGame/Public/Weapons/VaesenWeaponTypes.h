#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenDamageTypes.h"
#include "VaesenWeaponTypes.generated.h"

class UAnimMontage;
class UFXSystemAsset;
class USoundBase;

/** Picks a weapon's default animation layer, montages, attacks and sounds (DA_WeaponCategoryDefaults). */
UENUM(BlueprintType)
enum class EVaesenWeaponCategory : uint8
{
	Unarmed,
	Dagger,
	OneHandedSword,
	OneHandedAxe,
	Club,
	Warhammer,
	Improvised,
	Shield,
	Pistol,
	Revolver,
	Rifle,
	Bow,
	Crossbow
};
ENUM_RANGE_BY_FIRST_AND_LAST(EVaesenWeaponCategory, EVaesenWeaponCategory::Unarmed, EVaesenWeaponCategory::Crossbow);

/** How a shot reaches its target. */
UENUM(BlueprintType)
enum class EVaesenShotType : uint8
{
	/** Hitscan: hits at once along the aim. */
	InstantHit,
	/** A projectile with travel time and drop. */
	Projectile
};

/** When spent casings leave the weapon. */
UENUM(BlueprintType)
enum class EVaesenCasingEjection : uint8
{
	None,
	/** Every shot (self-loading). */
	OnFire,
	/** When the bolt or lever is worked (Krag-Jørgensen, Winchester). */
	OnCycle,
	/** When reloading: revolver emptied, breech opened (rolling block). */
	OnReload
};

/** How rounds go in. */
UENUM(BlueprintType)
enum class EVaesenReloadStyle : uint8
{
	/** One at a time; can be interrupted between rounds. */
	SingleRound,
	/** All at once (a clip, a magazine). */
	Magazine,
	/** Revolver: spent casings out, then one round at a time; can be interrupted between rounds. */
	Cylinder,
	/** Powder, ball and ramrod: one long reload that can't be interrupted. */
	MuzzleLoading
};

/** What happens between shots. */
UENUM(BlueprintType)
enum class EVaesenWeaponAction : uint8
{
	/** Fire, then reload (or fire again if more is loaded). */
	SingleShot,
	/** Work the bolt after each shot before the next. */
	BoltAction,
	/** Work the lever after each shot before the next. */
	LeverAction,
	/** Double action: fire again straight away. */
	Revolver,
	/** Bows: nock, draw, hold, release. */
	Draw
};

/** How a thrown item flies. */
UENUM(BlueprintType)
enum class EVaesenThrowStyle : uint8
{
	/** Underarm arc (a stone). */
	Lob,
	/** Overarm, end over end (a club, a sword). */
	Overhand,
	/** Fast flat spin (a hatchet). */
	Spinning,
	/** Point first (a knife, a spear). */
	PointFirst
};

/** The moment of a montage that may eject casings (AN_EjectCasing only ejects if it matches the weapon's setting). */
UENUM(BlueprintType)
enum class EVaesenCasingMoment : uint8
{
	Fire,
	Cycle,
	Reload
};

/** A weapon sound (DA_WeaponSounds), played from notifies and replicated events; empty slots are silent. */
UENUM(BlueprintType)
enum class EVaesenWeaponSound : uint8
{
	Draw,
	Sheath,
	SwingLight,
	SwingHeavy,
	Fire,
	FireDistantTail,
	DryFire,
	ReloadOpen,
	ReloadInsert,
	ReloadClose,
	Ramrod,
	Cylinder,
	Cycle,
	CasingBounce,
	BowNock,
	BowDraw,
	BowRelease,
	Flyby,
	Throw,
	Block,
	Parry,
	Break,
	NearBreak,
	HammerCock,
	Misfire,
	Ricochet,
	ArrowStick,
	ArrowPull,
	DropLand,
	GearRattle,
	GunshotEcho
};
ENUM_RANGE_BY_FIRST_AND_LAST(EVaesenWeaponSound, EVaesenWeaponSound::Draw, EVaesenWeaponSound::GunshotEcho);

/** The weapon montages that can be overridden per weapon (empty = the category's from DA_WeaponCategoryDefaults). */
USTRUCT(BlueprintType)
struct FVaesenWeaponMontages
{
	GENERATED_BODY()

	/** Melee attack (when an attack has none of its own). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Fire;

	/** Firing while aiming (right mouse). Empty = Fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> FireAimed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Reload;

	/** Reloading an empty weapon (chamber a round as well). Empty = Reload. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ReloadEmpty;

	/** Working the bolt or lever. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Cycle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Equip;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Unequip;

	/** Hold-to-aim throw and release. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Throw;

	/** Raising the block. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Block;

	/** Knocked off balance by a parry. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> Stagger;
};

/**
 * One melee attack, as data. The hit window comes from ANS_MeleeHitWindow in the montage; without a montage (or
 * without that notify) HitWindowStart/HitWindowEnd and Duration time it instead, so it can be tested before animations exist.
 */
USTRUCT(BlueprintType)
struct FVaesenMeleeAttack
{
	GENERATED_BODY()

	/** Empty = the weapon's (or category's) attack montage; empty there too = timed by the fallback below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage;

	/** Section of the montage to play (None = from the start). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName MontageSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FVaesenDamagePacket Damage;

	/** How far it reaches when the weapon has no TraceStart/TraceEnd sockets (and for unarmed). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", Units = "cm"))
	float Reach = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float StaminaCost = 10.0f;

	/** After the attack ends, before another can start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", Units = "s"))
	float Cooldown = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float Knockback = 0.0f;

	/** Off for hits a block can't stop (a vaesen's grab). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bCanBeBlocked = true;

	/** Durability the weapon loses per hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0"))
	float DurabilityCostPerHit = 0.5f;

	/** Without a hit window notify: when the blade becomes live, from the start. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Timing Fallback", meta = (ClampMin = "0", Units = "s"))
	float HitWindowStart = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Timing Fallback", meta = (ClampMin = "0", Units = "s"))
	float HitWindowEnd = 0.45f;

	/** Without a montage: how long the whole attack takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack|Timing Fallback", meta = (ClampMin = "0.05", Units = "s"))
	float Duration = 0.7f;
};

/** A ranged weapon's visual effects; every one is optional. */
USTRUCT(BlueprintType)
struct FVaesenFireEffects
{
	GENERATED_BODY()

	/** At the Muzzle socket for a moment (Niagara or Cascade). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UFXSystemAsset> MuzzleFlash;

	/** Left hanging in the air at the muzzle (heavy and lingering for black powder). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UFXSystemAsset> MuzzleSmoke;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects", meta = (ClampMin = "0"))
	float SmokeScale = 1.0f;

	/** Attached to projectiles in flight (an arrow's trail), or drawn from muzzle to hit for instant hits (a tracer). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UFXSystemAsset> TracerOrTrail;
};

/** Everything about throwing an item; any item can be made throwable (UVaesenItem::Throwing). */
USTRUCT(BlueprintType)
struct FVaesenThrowSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing")
	bool bCanBeThrown = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown"))
	EVaesenThrowStyle Style = EVaesenThrowStyle::Lob;

	/** What it deals on a hit: a stone Blunt, a thrown sword Slashing or (point first) Piercing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown"))
	FVaesenDamagePacket Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown", ClampMin = "100", Units = "CentimetersPerSecond"))
	float Speed = 1400.0f;

	/** Turns per second in the air (end over end or flat, by style). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown", ClampMin = "0"))
	float SpinRate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown", ClampMin = "0"))
	float StaminaCost = 8.0f;

	/** Shatters on the first hit (a bottle) instead of landing as a pickup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown"))
	bool bBreaksOnImpact = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown && bBreaksOnImpact"))
	TObjectPtr<UFXSystemAsset> BreakEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Throwing", meta = (EditCondition = "bCanBeThrown && bBreaksOnImpact"))
	TObjectPtr<USoundBase> BreakSound;
};
