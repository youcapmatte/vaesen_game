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
		ReloadTime = 0.2f;
		SpreadHip = 0.0f;
		SpreadMoving = 0.0f;
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

/** Arrows: stick and can be picked up again. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestArrow : public UVaesenAmmoItem
{
	GENERATED_BODY()

public:
	UVaesenTestArrow()
	{
		bSticksInSurfaces = true;
		bRecoverable = true;
	}
};

UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestBow : public UVaesenRangedWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestBow()
	{
		Category = EVaesenWeaponCategory::Bow;
		Slot = EVaesenEquipmentSlot::MainHand;
		bTwoHanded = true;
		Action = EVaesenWeaponAction::Draw;
		ShotType = EVaesenShotType::Projectile;
		LaunchSpeed = 6000.0f;
		AcceptedAmmo = UVaesenTestArrow::StaticClass();
		SpreadHip = 0.0f;
		SpreadAimed = 0.0f;
		SpreadMoving = 0.0f;
	}
};

/** A throwable stone (stacks). */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestStone : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenTestStone()
	{
		bStackable = true;
		MaxStackSize = 10;
		Throwing.bCanBeThrown = true;
		Throwing.Style = EVaesenThrowStyle::Lob;
		Throwing.Speed = 1300.0f;
		Throwing.Damage.Entries.Emplace(VaesenTags::Damage_Blunt, 6.0f);
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

/** A sword that can be held in both hands (the off hand free) or one. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestVersatileSword : public UVaesenMeleeWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestVersatileSword()
	{
		Category = EVaesenWeaponCategory::OneHandedSword;
		Slot = EVaesenEquipmentSlot::MainHand;
		bCanBeTwoHanded = true;
	}
};

/** A buckler for the off hand. */
UCLASS(NotBlueprintable, HideDropdown, meta = (HiddenNode))
class UVaesenTestBuckler : public UVaesenMeleeWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenTestBuckler()
	{
		Category = EVaesenWeaponCategory::Shield;
		Slot = EVaesenEquipmentSlot::OffHand;
	}
};
