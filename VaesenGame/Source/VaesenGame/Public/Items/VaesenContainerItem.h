#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenContainerItem.generated.h"

/** A satchel, bag or backpack: worn on the back, it lets the character carry more. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenContainerItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenContainerItem();

	/** Extra inventory slots while worn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "0"))
	int32 ExtraSlots = 6;

	/** Extra weight the character can carry while worn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Container", meta = (ClampMin = "0", Units = "kg"))
	float ExtraCarryWeight = 10.0f;
};
