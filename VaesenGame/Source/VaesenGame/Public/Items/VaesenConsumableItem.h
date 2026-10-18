#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenConsumableItem.generated.h"

/**
 * Used up when used: food, drink, medicine, remedies, poison. What it does is its Effects; the inventory takes one
 * from the stack after Use. Override Use in a Blueprint for anything the effects can't say.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenConsumableItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenConsumableItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	EVaesenConsumableType ConsumableType = EVaesenConsumableType::Food;

	/** What happens to whoever consumes it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	TArray<FVaesenItemEffect> Effects;

	/** Off: it can be used without running out (e.g. a never-ending flask). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable")
	bool bConsumedOnUse = true;
};
