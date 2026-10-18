#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "VaesenIngredientItem.generated.h"

/** Herbs, roots, minerals and other materials for remedies and crafting. Kinds are told apart by ItemTags. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenIngredientItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenIngredientItem();
};
