#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenArmorItem.generated.h"

class USkeletalMesh;

/**
 * Clothes and armor, worn in one armor slot and only there: Head, Chest (torso and arms), Hands, Legs or Feet replace
 * that body part's mesh with ArmorMesh; Back (a cloak) is worn over the body; FullBody replaces the whole body and is
 * for NPCs only (players dress piece by piece). The character's equipment component applies the look.
 *
 * ArmorMesh must use the mannequin skeleton (it follows the body's animation) and, for the body parts, cover the
 * whole part like the split SKM_Manny parts do.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenArmorItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenArmorItem();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** What it looks like worn: replaces the body part's mesh (or, on the back, is worn over the body). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor")
	TObjectPtr<USkeletalMesh> ArmorMesh;

	/** How much harm it takes off (0 for ordinary clothes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor", meta = (ClampMin = "0"))
	int32 Protection = 0;
};
