#pragma once

#include "CoreMinimal.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Engine/DataAsset.h"
#include "Weapons/VaesenWeaponTypes.h"
#include "VaesenWeaponData.generated.h"

class UAnimInstance;
class UAnimSequenceBase;
class USoundAttenuation;
class USoundConcurrency;
class UPhysicalMaterial;

/**
 * A weapon's sounds (DA_WeaponSounds): one per moment, each a Sound Cue or MetaSound with its own variation (pitch,
 * volume, several takes). A weapon without a set of its own uses its category's; an empty slot is silent.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenWeaponSoundSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TMap<EVaesenWeaponSound, TObjectPtr<USoundBase>> Sounds;

	/** For Fire, Echo and the distant tail: gunshots carry far. Empty = the sound's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundAttenuation> GunshotAttenuation;

	/** For everything else (swings, reloads, casings): short range. Empty = the sound's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundAttenuation> ShortRangeAttenuation;

	/** Caps how many play at once (crowds of casings or swings). Empty = no cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<USoundConcurrency> Concurrency;

	USoundBase* GetSound(EVaesenWeaponSound Slot) const
	{
		const TObjectPtr<USoundBase>* Found = Sounds.Find(Slot);
		return Found ? Found->Get() : nullptr;
	}

	/** Whether this slot is heard from far away. */
	static bool IsGunshot(EVaesenWeaponSound Slot)
	{
		return Slot == EVaesenWeaponSound::Fire || Slot == EVaesenWeaponSound::FireDistantTail || Slot == EVaesenWeaponSound::GunshotEcho;
	}
};

/** One row of the impact table: what a hit of a damage type on a surface sounds and looks like. */
USTRUCT(BlueprintType)
struct FVaesenImpactEntry
{
	GENERATED_BODY()

	/** Empty = any damage type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (Categories = "Damage"))
	FGameplayTag DamageType;

	/** Default = any surface. Surfaces come from physical materials (Flesh, Wood, Stone, Metal, Earth, Water, Cloth). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UFXSystemAsset> Effect;

	/** Bullets only: chance (0..1) of a ricochet sound off this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0", ClampMax = "1"))
	float RicochetChance = 0.0f;
};

/**
 * The impact table (DA_ImpactEffects): sounds and effects for hits, by damage type and the physical material hit.
 * The most specific row wins: type and surface, then surface only, then type only, then the any/any row.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenImpactEffects : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (TitleProperty = "DamageType"))
	TArray<FVaesenImpactEntry> Entries;

	/** Ricochet sound for bullets glancing off (used with RicochetChance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> RicochetSound;

	/** The best row for this hit, or null. */
	const FVaesenImpactEntry* FindEntry(const FGameplayTag& DamageType, EPhysicalSurface Surface) const;
};

/**
 * A category's weapons held in both hands (a sword or an axe that can be: UVaesenWeaponItem::bCanBeTwoHanded). Each
 * empty entry falls back to the one-handed one, so a category only needs what's different.
 */
USTRUCT(BlueprintType)
struct FVaesenTwoHandedGrip
{
	GENERATED_BODY()

	/** The stance while drawn in both hands. Empty = the one-handed stance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed")
	TSubclassOf<UAnimInstance> AnimLayer;

	/** Motion matching (later): locomotion in both hands. Empty = the one-handed database. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (AllowedClasses = "/Script/PoseSearch.PoseSearchDatabase,/Script/Chooser.ChooserTable"))
	TSoftObjectPtr<UObject> LocomotionDatabase;

	/** Montages in both hands (attacks, block, draw...). Each empty one = the one-handed montage. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed")
	FVaesenWeaponMontages Montages;

	/** Its own attacks in both hands; otherwise the one-handed attacks, scaled below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed")
	bool bOverrideAttacks = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "bOverrideAttacks"))
	FVaesenMeleeAttack LightAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "bOverrideAttacks"))
	FVaesenMeleeAttack HeavyAttack;

	/** The one-handed attacks in both hands: harder, and heavier on stamina. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "!bOverrideAttacks", ClampMin = "0"))
	float DamageScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "!bOverrideAttacks", ClampMin = "0"))
	float StaminaScale = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "!bOverrideAttacks", ClampMin = "0"))
	float KnockbackScale = 1.25f;

	/** Blocking in both hands. Off = the one-handed block. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed")
	bool bOverrideBlockProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Two-Handed", meta = (EditCondition = "bOverrideBlockProfile"))
	FVaesenBlockProfile BlockProfile;
};

/** What a weapon category uses unless the weapon overrides it. */
USTRUCT(BlueprintType)
struct FVaesenCategoryDefaults
{
	GENERATED_BODY()

	/** Linked anim layer (Lyra style) while drawn. Empty = no change. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	TSubclassOf<UAnimInstance> AnimLayer;

	/**
	 * Motion matching (later): the pose search database (or chooser) for locomotion with this category drawn. Read by
	 * the anim instance (UVaesenAnimInstance::LocomotionDatabase); empty = the character's default locomotion.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category", meta = (AllowedClasses = "/Script/PoseSearch.PoseSearchDatabase,/Script/Chooser.ChooserTable"))
	TSoftObjectPtr<UObject> LocomotionDatabase;

	/**
	 * How the fingers close round this category's grips: a pose (frame 0 of it) laid over the finger bones only, of each
	 * hand that holds the weapon. Empty = the fingers as the animation has them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	TObjectPtr<UAnimSequenceBase> GripPose;

	/**
	 * How this category's weapons sit in the hand, from the hand socket (its animation pack holds them so: a sword's
	 * blade angled as the pack's sword is). A weapon's own Hold.InHand is applied on top.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FTransform InHand;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FVaesenWeaponMontages Montages;

	/** Melee: the default light attack (also a ranged weapon's butt stroke, a shield's bash). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FVaesenMeleeAttack LightAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	bool bHasHeavyAttack = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category", meta = (EditCondition = "bHasHeavyAttack"))
	FVaesenMeleeAttack HeavyAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FVaesenBlockProfile BlockProfile;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	TObjectPtr<UVaesenWeaponSoundSet> Sounds;

	/** Held in both hands (weapons that can be: bCanBeTwoHanded), when the off hand is free. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FVaesenTwoHandedGrip TwoHanded;
};

/** A place on the body a weapon is carried (a sheath socket on the mannequin) and how it's drawn from there. */
USTRUCT(BlueprintType)
struct FVaesenSheathLocation
{
	GENERATED_BODY()

	/** Shared by every weapon carried here; the hand IK (curve HandIK_Sheath) puts the hand on each weapon's Grip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath")
	TObjectPtr<UAnimMontage> SheathMontage;

	/** Where the draw montage's hand grabs, in the sheath socket's space. A weapon's Grip far from it gets a warning. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath")
	FVector ReferenceGrabPoint = FVector::ZeroVector;

	/** Without a montage (or its AN_WeaponGrab / AN_WeaponRelease notify): when the weapon changes hands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath|Timing Fallback", meta = (ClampMin = "0", Units = "s"))
	float GrabTime = 0.25f;

	/** Without a montage: how long drawing or sheathing takes in all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath|Timing Fallback", meta = (ClampMin = "0", Units = "s"))
	float Duration = 0.5f;

	/** Bone used when the skeleton has no socket of this name yet, and the offset from it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath|Fallback")
	FName FallbackBone = TEXT("spine_03");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheath|Fallback")
	FTransform FallbackOffset;
};

/**
 * DA_WeaponCategoryDefaults (Project Settings > Vaesen > Combat): per weapon category its anim layer, montages, attacks,
 * block profile and sounds, and per sheath socket its draw and sheath montages. Weapons only set what differs.
 *
 * Without the asset everything still works: these class defaults hold grey-box values (attacks, timings, block
 * profiles) for every category.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenWeaponCategoryDefaults : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UVaesenWeaponCategoryDefaults();

	/** The asset from Project Settings, or these built-in defaults. Never null. */
	static const UVaesenWeaponCategoryDefaults* Get();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Categories")
	TMap<EVaesenWeaponCategory, FVaesenCategoryDefaults> Categories;

	/** Keyed by socket name on the character (Sheath_BackUpper...). Add more here and to the skeleton; no code needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheathing")
	TMap<FName, FVaesenSheathLocation> SheathLocations;

	/** Warn in the editor when a sheathed weapon's Grip is further than this from its location's grab point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sheathing", meta = (ClampMin = "0", Units = "cm"))
	float GripWarningDistance = 25.0f;

	/** The hand socket weapons are held in, and the off hand's (shields). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Holding")
	FName MainHandSocket = TEXT("hand_r_weapon");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Holding")
	FName OffHandSocket = TEXT("hand_l_weapon");

	/** Used everywhere nothing else is set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sounds")
	TObjectPtr<UVaesenImpactEffects> ImpactEffects;

	const FVaesenCategoryDefaults& GetCategory(EVaesenWeaponCategory Category) const;
	const FVaesenSheathLocation* FindSheathLocation(FName Socket) const { return SheathLocations.Find(Socket); }

	/** The sheath sockets, for dropdowns. */
	TArray<FName> GetSheathSocketNames() const;

private:
	/** Fills the built-in grey-box values. */
	void SetBuiltInDefaults();
};
