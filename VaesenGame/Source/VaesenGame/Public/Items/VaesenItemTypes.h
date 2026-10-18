#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VaesenItemTypes.generated.h"

/** Where an equippable item goes. Head..Feet are the modular body parts (armor replaces that part's mesh). */
UENUM(BlueprintType)
enum class EVaesenEquipmentSlot : uint8
{
	Head,
	Torso,
	Arms,
	Legs,
	Feet,
	MainHand,
	OffHand,
	Back,
	Talisman
};

/** Whether the slot is one of the body parts (what armor can go in). */
inline bool IsBodySlot(EVaesenEquipmentSlot Slot)
{
	return Slot <= EVaesenEquipmentSlot::Feet;
}

UENUM(BlueprintType)
enum class EVaesenWeaponType : uint8
{
	Melee,
	Firearm,
	Thrown
};

/** For sorting and the inventory; what it does comes from its effects. */
UENUM(BlueprintType)
enum class EVaesenConsumableType : uint8
{
	Food,
	Drink,
	Medicine,
	Poison,
	Other
};

/** Something a consumable does, e.g. Effect.Heal 2, or Effect.Poison 1 for 30 seconds. */
USTRUCT(BlueprintType)
struct FVaesenItemEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (Categories = "Effect"))
	FGameplayTag Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float Magnitude = 1.0f;

	/** 0 = happens at once; otherwise lasts this long. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0", Units = "s"))
	float Duration = 0.0f;
};
