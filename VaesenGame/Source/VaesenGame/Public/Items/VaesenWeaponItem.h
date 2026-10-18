#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenWeaponItem.generated.h"

class UVaesenAmmoItem;

/** Something to fight with: knives, axes, revolvers, rifles, thrown things. Held in the main hand. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenWeaponItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenWeaponItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EVaesenWeaponType WeaponType = EVaesenWeaponType::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0"))
	int32 Damage = 1;

	/** Reach for melee, effective range otherwise. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0", Units = "cm"))
	float Range = 150.0f;

	/** Takes both hands (the off hand stays empty). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bTwoHanded = false;

	/** What it fires. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (EditCondition = "WeaponType == EVaesenWeaponType::Firearm", EditConditionHides))
	TSubclassOf<UVaesenAmmoItem> AmmoType;

	/** Shots before reloading. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1", EditCondition = "WeaponType == EVaesenWeaponType::Firearm", EditConditionHides))
	int32 MagazineSize = 1;
};
