#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenGameplayTags.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenPouchItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "VaesenCombatTestTypes.generated.h"

/** Items for the combat automation tests only (the real ones are Blueprints). Hidden from the editor's pickers. */

UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestCartridge : public UVaesenAmmoItem
{
	GENERATED_BODY()
};

/** Another kind of ammo with the Item.Ammo tag, to test accepting by tag. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestBullet : public UVaesenAmmoItem
{
	GENERATED_BODY()
};

UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestRevolver : public UVaesenRangedWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestRevolver()
	{
		Category = EVaesenWeaponCategory::Revolver;
		Slot = EVaesenEquipmentSlot::Sidearm;
		MagazineSize = 6;
		AcceptedAmmo = UVaesenTestCartridge::StaticClass();
		ReloadStyle = EVaesenReloadStyle::Cylinder;
		CasingEjection = EVaesenCasingEjection::OnReload;
	}
};

/** Takes any ammo by tag. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestRifle : public UVaesenRangedWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestRifle()
	{
		Category = EVaesenWeaponCategory::Rifle;
		Slot = EVaesenEquipmentSlot::MainHand;
		bTwoHanded = true;
		MagazineSize = 5;
		AcceptedAmmoTag = VaesenTags::Item_Ammo;
		Action = EVaesenWeaponAction::BoltAction;
	}
};

UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestSword : public UVaesenMeleeWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestSword()
	{
		Category = EVaesenWeaponCategory::OneHandedSword;
		Slot = EVaesenEquipmentSlot::MainHand;
		Throwing.bCanBeThrown = true;
	}
};

/** A cartridge box: holds Item.Ammo. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestCartridgeBox : public UVaesenPouchItem
{
	GENERATED_BODY()

public:
	UVaesenTestCartridgeBox()
	{
		PouchSlots = 4;
		AcceptedItemTags.AddTag(VaesenTags::Item_Ammo);
	}
};
