#include "Items/VaesenRangedWeaponItem.h"

#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenPouchItem.h"
#include "Net/UnrealNetwork.h"
#include "Weapons/VaesenRangedWeaponActor.h"

UVaesenRangedWeaponItem::UVaesenRangedWeaponItem()
{
	Category = EVaesenWeaponCategory::Revolver;
	SheathSocket = TEXT("Sheath_HipRight");
	Slot = EVaesenEquipmentSlot::Sidearm;
}

void UVaesenRangedWeaponItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenRangedWeaponItem, LoadedRounds);
	DOREPLIFETIME(UVaesenRangedWeaponItem, bNeedsCycle);
	DOREPLIFETIME(UVaesenRangedWeaponItem, LoadedAmmo);
}

TSubclassOf<AVaesenWeaponActor> UVaesenRangedWeaponItem::GetDefaultActorClass() const
{
	return AVaesenRangedWeaponActor::StaticClass();
}

bool UVaesenRangedWeaponItem::Accepts(TSubclassOf<UVaesenItem> AmmoClass) const
{
	if (!AmmoClass || !AmmoClass->IsChildOf(UVaesenAmmoItem::StaticClass()))
	{
		return false;
	}
	if (AcceptedAmmo && AmmoClass->IsChildOf(AcceptedAmmo))
	{
		return true;
	}
	return AcceptedAmmoTag.IsValid() && AmmoClass->GetDefaultObject<UVaesenItem>()->ItemTags.HasTag(AcceptedAmmoTag);
}

TSubclassOf<UVaesenAmmoItem> UVaesenRangedWeaponItem::FindAmmoIn(const UVaesenInventoryComponent* Inventory) const
{
	if (!Inventory)
	{
		return nullptr;
	}
	// Keep loading what's loaded, so rounds in one magazine are all alike.
	if (LoadedRounds > 0 && LoadedAmmo && Inventory->HasItem(LoadedAmmo))
	{
		return LoadedAmmo;
	}
	if (AcceptedAmmo && Inventory->HasItem(AcceptedAmmo))
	{
		return AcceptedAmmo;
	}
	if (!AcceptedAmmoTag.IsValid())
	{
		return nullptr;
	}

	// Any ammo with the tag, in the slots or in pouches.
	TSubclassOf<UVaesenAmmoItem> Found;
	const auto Consider = [this, &Found](const UVaesenItem* Item)
	{
		if (!Found && Item && Accepts(Item->GetClass()))
		{
			Found = Item->GetClass();
		}
	};
	for (const UVaesenItem* Item : Inventory->GetSlots())
	{
		Consider(Item);
		if (const UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Item))
		{
			for (const UVaesenItem* Inner : Pouch->GetContents())
			{
				Consider(Inner);
			}
		}
	}
	return Found;
}

int32 UVaesenRangedWeaponItem::CountAmmoIn(const UVaesenInventoryComponent* Inventory) const
{
	const TSubclassOf<UVaesenAmmoItem> Ammo = FindAmmoIn(Inventory);
	return Ammo ? Inventory->GetItemCount(Ammo) : 0;
}

void UVaesenRangedWeaponItem::SetLoadedRounds(int32 Rounds, TSubclassOf<UVaesenAmmoItem> Ammo)
{
	LoadedRounds = FMath::Clamp(Rounds, 0, MagazineSize);
	LoadedAmmo = LoadedRounds > 0 ? Ammo : TSubclassOf<UVaesenAmmoItem>(LoadedAmmo);
	OnRep_Ammo();
}

void UVaesenRangedWeaponItem::SetNeedsCycle(bool bInNeedsCycle)
{
	bNeedsCycle = bInNeedsCycle;
	OnRep_Ammo();
}

void UVaesenRangedWeaponItem::OnRep_Ammo()
{
	OnAmmoChanged.Broadcast();
}
