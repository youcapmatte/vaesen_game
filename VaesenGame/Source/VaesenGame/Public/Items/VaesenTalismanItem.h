#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenTalismanItem.generated.h"

/** A charm or protective object worn against vaesen: an iron nail, a silver cross, a pouch of salt. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenTalismanItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenTalismanItem();

	/** Which vaesen (or kinds of vaesen) it protects against. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talisman")
	FGameplayTagContainer WardsAgainst;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Talisman", meta = (ClampMin = "0"))
	int32 Protection = 1;
};
