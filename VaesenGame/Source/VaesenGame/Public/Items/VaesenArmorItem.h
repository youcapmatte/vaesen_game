#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenArmorItem.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Clothes and armor: worn on a body part (Slot = Head, Torso, Arms, Legs or Feet) and replacing that part's mesh
 * with ArmorMesh while equipped. ArmorMesh must use the mannequin skeleton (it follows the body's animation) and
 * cover the whole part, like the split SKM_Manny parts do.
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

	/** Replaces the body part's mesh while worn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor")
	TObjectPtr<USkeletalMesh> ArmorMesh;

	/** How much harm it takes off (0 for ordinary clothes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor", meta = (ClampMin = "0"))
	int32 Protection = 0;

	/** The character's mesh component for this item's body part. */
	USkeletalMeshComponent* GetBodyPart(const AVaesenPlayerCharacter* Character) const;

protected:
	virtual void OnEquipped_Implementation(AVaesenPlayerCharacter* Character) override;
	virtual void OnUnequipped_Implementation(AVaesenPlayerCharacter* Character) override;

private:
	/** The body part's mesh before this was put on. */
	TWeakObjectPtr<USkeletalMesh> PreviousMesh;
};
