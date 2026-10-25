#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VaesenDamageTypes.generated.h"

/** One part of a damage packet: e.g. Damage.Slashing 12. */
USTRUCT(BlueprintType)
struct FVaesenDamageEntry
{
	GENERATED_BODY()

	FVaesenDamageEntry() = default;
	FVaesenDamageEntry(const FGameplayTag& InType, float InAmount) : DamageType(InType), Amount(InAmount) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (Categories = "Damage"))
	FGameplayTag DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float Amount = 0.0f;
};

/**
 * What one attack deals: a list of (damage type, amount). Most attacks have one entry (a sword swing: Slashing 12),
 * some two (an iron blade: Slashing 10 + Iron 4). Damage types live on the attack, not on the weapon.
 */
USTRUCT(BlueprintType)
struct FVaesenDamagePacket
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (TitleProperty = "DamageType"))
	TArray<FVaesenDamageEntry> Entries;

	float GetTotal() const
	{
		float Total = 0.0f;
		for (const FVaesenDamageEntry& Entry : Entries)
		{
			Total += Entry.Amount;
		}
		return Total;
	}

	bool IsEmpty() const { return GetTotal() <= 0.0f; }

	/** Every amount times Scale (a weapon's multiplier, a bow's draw strength). */
	FVaesenDamagePacket Scaled(float Scale) const
	{
		FVaesenDamagePacket Result = *this;
		for (FVaesenDamageEntry& Entry : Result.Entries)
		{
			Entry.Amount *= Scale;
		}
		return Result;
	}

	/** Adds Amount to the entry of that type (a new entry if there's none). */
	void Add(const FGameplayTag& DamageType, float Amount)
	{
		if (FVaesenDamageEntry* Existing = Entries.FindByPredicate([&DamageType](const FVaesenDamageEntry& Each) { return Each.DamageType == DamageType; }))
		{
			Existing->Amount += Amount;
		}
		else
		{
			Entries.Emplace(DamageType, Amount);
		}
	}

	FString ToString() const
	{
		TArray<FString> Parts;
		for (const FVaesenDamageEntry& Entry : Entries)
		{
			Parts.Add(FString::Printf(TEXT("%s %.1f"), *Entry.DamageType.ToString(), Entry.Amount));
		}
		return Parts.Num() ? FString::Join(Parts, TEXT(", ")) : TEXT("(none)");
	}
};

/**
 * How well something blocks, per damage type: the share of each hit it absorbs (0 = none, 1 = all). A sword blocks
 * Slashing well and Blunt poorly; a shield blocks all three well. Missing types aren't reduced.
 */
USTRUCT(BlueprintType)
struct FVaesenBlockProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (Categories = "Damage", ClampMin = "0", ClampMax = "1"))
	TMap<FGameplayTag, float> Reduction;

	/** Only shields stop bullets and arrows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block")
	bool bCanBlockProjectiles = false;

	/** Pressing block this soon before a hit lands parries it (daggers and swords good, clubs poor, shields medium). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0", Units = "s"))
	float ParryWindow = 0.2f;

	/** Stamina used per second while block is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0"))
	float StaminaPerSecond = 5.0f;

	/** How hard blocking wears it: the absorbed damage times this. Guns and bows aren't made for it (4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0"))
	float WearScale = 1.0f;

	/** A parry wears it too, by this share of the hit (times WearScale): 0 for blades; guns and bows suffer it anyway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0", ClampMax = "1"))
	float ParryWearShare = 0.0f;

	/** How wide the front is: hits from within this angle of where the blocker faces can be blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Block", meta = (ClampMin = "0", ClampMax = "180", Units = "Degrees"))
	float BlockHalfAngle = 70.0f;

	float GetReduction(const FGameplayTag& DamageType) const
	{
		// The most specific listed type that matches wins (Damage.Slashing covers Damage.Slashing.Axe).
		const float* Best = nullptr;
		int32 BestDepth = -1;
		for (const TPair<FGameplayTag, float>& Pair : Reduction)
		{
			if (DamageType.MatchesTag(Pair.Key))
			{
				const int32 Depth = Pair.Key.GetGameplayTagParents().Num();
				if (Depth > BestDepth)
				{
					Best = &Pair.Value;
					BestDepth = Depth;
				}
			}
		}
		return Best ? FMath::Clamp(*Best, 0.0f, 1.0f) : 0.0f;
	}
};

/** How a hit turned out, for hit markers, sounds and the debug numbers. */
UENUM(BlueprintType)
enum class EVaesenHitOutcome : uint8
{
	Hit,
	/** Reduced by a block. */
	Blocked,
	/** Negated by a parry; the attacker is staggered. */
	Parried,
	/** Took the target's last health. */
	Killed
};

/** One damage type in a resolved hit: before and after protection and block. */
USTRUCT(BlueprintType)
struct FVaesenDamageLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FGameplayTag DamageType;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Raw = 0.0f;

	/** The target's protection against this type (negative = a weakness). */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Protection = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float AfterProtection = 0.0f;

	/** Taken by the block (and off the blocking weapon's durability). */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Absorbed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float Final = 0.0f;
};

/** A resolved hit: what was dealt, how it was reduced, and what it came to. */
USTRUCT(BlueprintType)
struct FVaesenDamageReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	TArray<FVaesenDamageLine> Lines;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	EVaesenHitOutcome Outcome = EVaesenHitOutcome::Hit;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float TotalFinal = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float TotalAbsorbed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	FVector_NetQuantize Location = FVector::ZeroVector;

	FString ToString() const;
};

/**
 * The damage rules, as plain functions (used by the damage execution, and by the automation tests):
 *   - each entry is reduced by the target's matching protection, to no less than zero (negative protection adds);
 *   - when blocked, what's left is reduced by the block profile's share for that type, and the absorbed part wears
 *     the blocking weapon;
 *   - a parry negates everything.
 */
namespace VaesenDamageMath
{
	/** Resolves Packet against protections (GetProtection: damage type -> protection) and an optional block. */
	VAESENGAME_API FVaesenDamageReport Resolve(const FVaesenDamagePacket& Packet, TFunctionRef<float(const FGameplayTag&)> GetProtection,
		const FVaesenBlockProfile* Block = nullptr, bool bParried = false);

	/** Whether something at Source is within HalfAngleDegrees of where Blocker (at BlockerLocation) faces. */
	VAESENGAME_API bool IsInFront(const FVector& BlockerLocation, const FVector& BlockerForward, const FVector& Source, float HalfAngleDegrees);

	/** Durability left after a wear of Amount (never below zero). */
	inline float ApplyWear(float Durability, float Amount) { return FMath::Max(0.0f, Durability - FMath::Max(0.0f, Amount)); }
}
