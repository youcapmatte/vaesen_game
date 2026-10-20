#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "VaesenItemTypes.generated.h"

/**
 * Where an equippable item goes. The armor slots come first (like The Witcher 3, plus head and back):
 *   Head, Chest (torso and arms), Hands, Legs, Feet  - replace that modular body part's mesh
 *   Back                                              - a cloak, worn over the body
 *   FullBody                                          - one suit for the whole body; NPCs only
 * Then MainHand, OffHand (weapons, tools), Talisman (charms), Bag (satchel, backpack) and Sidearm (a second weapon,
 * carried in its own sheath while the main-hand one is in use).
 */
UENUM(BlueprintType)
enum class EVaesenEquipmentSlot : uint8
{
	Head,
	Chest,
	Hands,
	Legs,
	Feet,
	Back,
	FullBody UMETA(DisplayName = "Full Body (NPC only)"),
	MainHand,
	OffHand,
	Talisman,
	Bag,
	/** A second weapon carried alongside the main hand one (a pistol on the hip with a rifle on the back). */
	Sidearm
};

/** Whether the slot is for armor (clothes): Head through FullBody. */
inline bool IsArmorSlot(EVaesenEquipmentSlot Slot)
{
	return Slot <= EVaesenEquipmentSlot::FullBody;
}

/** How a piece of armor sits on its body part. */
UENUM(BlueprintType)
enum class EVaesenArmorFit : uint8
{
	/** Takes the body part's place (a closed plate helm, full gloves, boots that include the foot). */
	ReplacesBodyPart,
	/** Added on top; the body part stays (a hat, a hood, a scarf, an open helmet). */
	WornOver
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
