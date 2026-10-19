#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenArmorItem.generated.h"

class USkeletalMesh;

/**
 * Clothes and armor, worn in one armor slot and only there: Head, Chest (torso and arms), Hands, Legs or Feet; Back (a
 * cloak, always worn over the body); FullBody (replaces the whole body, NPCs only; players dress piece by piece).
 * On a body part, Fit says whether ArmorMesh replaces the part (a plate helm) or is worn over it (a hat). The
 * character's equipment component applies the look.
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

	/** What it looks like worn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor")
	TObjectPtr<USkeletalMesh> ArmorMesh;

	/** Whether ArmorMesh takes the body part's place or is worn over it. Back is always worn over; Full Body always replaces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor")
	EVaesenArmorFit Fit = EVaesenArmorFit::ReplacesBodyPart;

	/** Whether the body part stays visible under it. */
	UFUNCTION(BlueprintPure, Category = "Armor")
	bool IsWornOver() const;

	/** How much harm it takes off (0 for ordinary clothes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor", meta = (ClampMin = "0"))
	int32 Protection = 0;
};
