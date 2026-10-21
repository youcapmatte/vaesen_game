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

/**
 * How a weapon sits in the hands, per weapon (tune it with the Weapon Fit rig in L_WeaponFit). The animation gives the body; this
 * places the weapon, and the hands follow it.
 *
 * In the hand: InHand moves it from the hand socket (hand_r_weapon) in every pose. The off hand goes to the mesh's
 * LeftGrip socket (+ LeftHandOffset), turned as the socket is.
 *
 * Shoulder aim (two-handed guns while aiming, the Shoulder Aim anim node): the weapon is laid along the aim (its Sight
 * socket's line) with the sight line at the eye (AimSightOffset) and the butt (Stock socket) in the shoulder pocket (the
 * skeleton's ShoulderPocket socket, + ShoulderOffset). The body meets the stock: the shoulder lifts by ShoulderShare of
 * the gap and the head bows onto the stock by the rest. Both hands are solved onto the weapon. Without a Stock socket
 * the sight just goes in front of the eye.
 */
USTRUCT(BlueprintType)
struct FVaesenWeaponHold
{
	GENERATED_BODY()

	/** From the hand socket, in every pose (forward is the barrel, X). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold")
	FTransform InHand;

	/** Moves the left hand from the LeftGrip socket, in the weapon's space (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold")
	FVector LeftHandOffset = FVector::ZeroVector;

	/** Lay it against the shoulder while aiming (off: the aim animation holds it as it is). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim")
	bool bShoulderAim = true;

	/**
	 * Where the Sight socket is while aiming, from the eye (the skeleton's Eye_R socket) along the aim: X how far in front
	 * of the eye, Y right, Z up (cm). The sight line goes through the eye with Y and Z at zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	FVector AimSightOffset = FVector(14.0, 0.0, 0.0);

	/** Moves the Stock from the shoulder pocket, along the aim (X forward, Y right, Z up, cm); normally zero. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	FVector ShoulderOffset = FVector::ZeroVector;

	/**
	 * How the gap between the eye line and the shoulder is shared: the shoulder lifts to the stock by this much of it,
	 * the head bows (and leans) onto the stock by the rest. 0 = all head, 1 = all shoulder.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim", ClampMin = "0", ClampMax = "1"))
	float ShoulderShare = 0.4f;

	/** Turns it while aiming (Roll cants it). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	FRotator AimRotation = FRotator::ZeroRotator;

	/** How fast it comes up to the shoulder and goes down again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim", ClampMin = "1"))
	float AimBlendSpeed = 12.0f;

	/** Each shot while aimed: pushed back this far (cm) and the muzzle up (degrees), easing back. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	float RecoilBack = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	float RecoilPitch = 4.0f;

	/** Each shot while aimed: the upper body rocks back this far (degrees), the shoulder taking the stock with it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold|Aim", meta = (EditCondition = "bShoulderAim"))
	float RecoilBodyPitch = 4.0f;
};

/**
 * A bolt worked by hand, as shares of the cycle time: the hand reaches the bolt knob, lifts it, pulls it back (the
 * spent case flies out at the end of the pull), pushes it forward, turns it down and goes back to the grip. The anim
 * (FAnimNode_VaesenShoulderAim) and GA_CycleAction (the case) use the same moments.
 */
namespace VaesenBolt
{
	constexpr float Reached = 0.15f;
	constexpr float Lifted = 0.27f;
	constexpr float Back = 0.5f;
	constexpr float Forward = 0.72f;
	constexpr float Down = 0.84f;
}
