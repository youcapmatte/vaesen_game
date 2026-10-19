#include "Inventory/VaesenEquipmentComponent.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/SkeletalMesh.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenArmorItem.h"
#include "Items/VaesenEquippableItem.h"
#include "Items/VaesenWeaponItem.h"
#include "Net/UnrealNetwork.h"

UVaesenEquipmentComponent::UVaesenEquipmentComponent()
{
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UVaesenEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenEquipmentComponent, Equipped);
}

bool UVaesenEquipmentComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	// For owners that don't use the registered subobject list (SetEquipped registers the items for those that do).
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (IsUsingRegisteredSubObjectList())
	{
		return bWroteSomething;
	}
	for (const FVaesenEquippedItem& Entry : Equipped)
	{
		if (Entry.Item)
		{
			bWroteSomething |= Channel->ReplicateSubobject(Entry.Item, *Bunch, *RepFlags);
		}
	}
	return bWroteSomething;
}

void UVaesenEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		for (const TSubclassOf<UVaesenEquippableItem>& ItemClass : StartingEquipment)
		{
			EquipNew(ItemClass);
		}
	}
	ApplyAppearance();
}

TArray<EVaesenEquipmentSlot> UVaesenEquipmentComponent::GetPlayerSlots()
{
	// The armor from head to feet, then the back, the hands, the talisman and the bag.
	return {
		EVaesenEquipmentSlot::Head, EVaesenEquipmentSlot::Chest, EVaesenEquipmentSlot::Hands,
		EVaesenEquipmentSlot::Legs, EVaesenEquipmentSlot::Feet,
		EVaesenEquipmentSlot::Back, EVaesenEquipmentSlot::MainHand, EVaesenEquipmentSlot::OffHand,
		EVaesenEquipmentSlot::Talisman, EVaesenEquipmentSlot::Bag,
	};
}

AVaesenBaseCharacter* UVaesenEquipmentComponent::GetCharacter() const
{
	return Cast<AVaesenBaseCharacter>(GetOwner());
}

UVaesenInventoryComponent* UVaesenEquipmentComponent::GetOwnInventory() const
{
	return GetOwner()->FindComponentByClass<UVaesenInventoryComponent>();
}

UVaesenEquippableItem* UVaesenEquipmentComponent::GetItemInSlot(EVaesenEquipmentSlot Slot) const
{
	const FVaesenEquippedItem* Entry = Equipped.FindByPredicate([Slot](const FVaesenEquippedItem& Each) { return Each.Slot == Slot; });
	return Entry ? Entry->Item.Get() : nullptr;
}

bool UVaesenEquipmentComponent::CanEquip(const UVaesenItem* Item) const
{
	const UVaesenEquippableItem* Equippable = Cast<UVaesenEquippableItem>(Item);
	if (!Equippable)
	{
		return false;
	}
	return Equippable->Slot != EVaesenEquipmentSlot::FullBody || bAllowFullBodyArmor;
}

bool UVaesenEquipmentComponent::CanEquipInSlot(const UVaesenItem* Item, EVaesenEquipmentSlot Slot) const
{
	return CanEquip(Item) && CastChecked<UVaesenEquippableItem>(Item)->Slot == Slot;
}

TArray<EVaesenEquipmentSlot> UVaesenEquipmentComponent::GetSlotsClearedBy(const UVaesenEquippableItem* Item) const
{
	TArray<EVaesenEquipmentSlot> Slots{ Item->Slot };

	// A two-handed weapon needs the off hand free; anything in the off hand pushes a two-handed weapon out.
	const UVaesenWeaponItem* Weapon = Cast<UVaesenWeaponItem>(Item);
	if (Item->Slot == EVaesenEquipmentSlot::MainHand && Weapon && Weapon->bTwoHanded)
	{
		Slots.Add(EVaesenEquipmentSlot::OffHand);
	}
	const UVaesenWeaponItem* MainHand = Cast<UVaesenWeaponItem>(GetItemInSlot(EVaesenEquipmentSlot::MainHand));
	if (Item->Slot == EVaesenEquipmentSlot::OffHand && MainHand && MainHand->bTwoHanded)
	{
		Slots.Add(EVaesenEquipmentSlot::MainHand);
	}
	return Slots;
}

bool UVaesenEquipmentComponent::EquipFromInventory(UVaesenInventoryComponent* From, UVaesenItem* Item)
{
	UVaesenEquippableItem* Equippable = Cast<UVaesenEquippableItem>(Item);
	if (!GetOwner()->HasAuthority() || !From || !Equippable || !CanEquip(Equippable) || From->FindSlotOf(Equippable) == INDEX_NONE)
	{
		return false;
	}

	// Whatever is in the way goes back into From: it must all fit, counting the room this item leaves.
	TArray<EVaesenEquipmentSlot> Cleared = GetSlotsClearedBy(Equippable);
	int32 SlotsNeeded = -Equippable->SlotSize;
	TArray<UVaesenEquippableItem*> Displaced;
	for (const EVaesenEquipmentSlot Slot : Cleared)
	{
		if (UVaesenEquippableItem* Worn = GetItemInSlot(Slot))
		{
			Displaced.Add(Worn);
			SlotsNeeded += Worn->SlotSize;
		}
	}
	if (!From->HasUnlimitedCapacity() && SlotsNeeded > From->GetFreeSlotCount())
	{
		return false;
	}

	UVaesenEquippableItem* Wearing = CastChecked<UVaesenEquippableItem>(Equippable->DuplicateItem(GetOwner(), 1));
	From->RemoveItem(Equippable, 1);
	for (const EVaesenEquipmentSlot Slot : Cleared)
	{
		if (UVaesenEquippableItem* Worn = GetItemInSlot(Slot))
		{
			From->TryAddItem(Worn);
			SetEquipped(Slot, nullptr);
		}
	}
	SetEquipped(Wearing->Slot, Wearing);
	return true;
}

bool UVaesenEquipmentComponent::UnequipToInventory(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	UVaesenEquippableItem* Worn = GetItemInSlot(Slot);
	To = To ? To : GetOwnInventory();
	if (!GetOwner()->HasAuthority() || !Worn || !To || !To->HasRoomFor(Worn))
	{
		return false;
	}

	if (To->TryAddItem(Worn).AmountGiven == 0)
	{
		return false;
	}
	SetEquipped(Slot, nullptr);
	return true;
}

bool UVaesenEquipmentComponent::EquipNew(TSubclassOf<UVaesenEquippableItem> ItemClass)
{
	const UVaesenEquippableItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenEquippableItem>() : nullptr;
	if (!GetOwner()->HasAuthority() || !Defaults || !CanEquip(Defaults))
	{
		return false;
	}

	for (const EVaesenEquipmentSlot Slot : GetSlotsClearedBy(Defaults))
	{
		SetEquipped(Slot, nullptr);
	}
	SetEquipped(Defaults->Slot, CastChecked<UVaesenEquippableItem>(Defaults->DuplicateItem(GetOwner(), 1)));
	return true;
}

void UVaesenEquipmentComponent::SetEquipped(EVaesenEquipmentSlot Slot, UVaesenEquippableItem* Item)
{
	const int32 Index = Equipped.IndexOfByPredicate([Slot](const FVaesenEquippedItem& Each) { return Each.Slot == Slot; });
	if (Index != INDEX_NONE)
	{
		if (Equipped[Index].Item == Item)
		{
			return;
		}
		if (Equipped[Index].Item)
		{
			RemoveReplicatedSubObject(Equipped[Index].Item);
		}
		Equipped.RemoveAt(Index);
	}

	if (Item)
	{
		AddReplicatedSubObject(Item);
		FVaesenEquippedItem& Entry = Equipped.AddDefaulted_GetRef();
		Entry.Slot = Slot;
		Entry.Item = Item;
	}

	ApplyAppearance();
	OnEquipmentChanged.Broadcast();
}

void UVaesenEquipmentComponent::OnRep_Equipped()
{
	ApplyAppearance();
	OnEquipmentChanged.Broadcast();
}

void UVaesenEquipmentComponent::ApplyAppearance()
{
	AVaesenBaseCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	// Attached meshes (weapons, tools, talismans, bags): off for what was taken off, on for what was put on.
	TArray<UVaesenEquippableItem*> Current;
	for (const FVaesenEquippedItem& Entry : Equipped)
	{
		if (Entry.Item)
		{
			Current.Add(Entry.Item);
		}
	}
	for (UVaesenEquippableItem* Item : AppliedItems)
	{
		if (Item && !Current.Contains(Item))
		{
			Item->Unequip();
		}
	}
	for (UVaesenEquippableItem* Item : Current)
	{
		if (!AppliedItems.Contains(Item))
		{
			Item->Equip(Character);
		}
	}
	AppliedItems = Current;

	// The body: each part shows the character's own mesh (from its class defaults) unless armor replaces it, and its
	// worn-over layer shows armor worn over it. Full-body armor replaces the whole body (torso) and hides the other
	// parts and layers; a cloak stays on over it.
	const AVaesenBaseCharacter* Defaults = GetOwner()->GetClass()->GetDefaultObject<AVaesenBaseCharacter>();
	const UVaesenArmorItem* FullBody = Cast<UVaesenArmorItem>(GetItemInSlot(EVaesenEquipmentSlot::FullBody));
	const auto SetMesh = [](USkeletalMeshComponent* Component, USkeletalMesh* Mesh)
	{
		if (Component && Component->GetSkeletalMeshAsset() != Mesh)
		{
			Component->SetSkeletalMeshAsset(Mesh);
		}
	};

	for (const EVaesenEquipmentSlot Slot : { EVaesenEquipmentSlot::Head, EVaesenEquipmentSlot::Chest, EVaesenEquipmentSlot::Hands,
		EVaesenEquipmentSlot::Legs, EVaesenEquipmentSlot::Feet, EVaesenEquipmentSlot::Back })
	{
		const UVaesenArmorItem* Armor = Cast<UVaesenArmorItem>(GetItemInSlot(Slot));
		USkeletalMesh* ArmorMesh = Armor ? Armor->ArmorMesh.Get() : nullptr;
		const bool bWornOver = Armor && Armor->IsWornOver();

		// The back is a layer of its own: just the cloak.
		if (Slot == EVaesenEquipmentSlot::Back)
		{
			SetMesh(Character->GetBodyPartFor(Slot), ArmorMesh);
			continue;
		}

		const USkeletalMeshComponent* DefaultPart = Defaults ? Defaults->GetBodyPartFor(Slot) : nullptr;
		USkeletalMesh* PartMesh = DefaultPart ? DefaultPart->GetSkeletalMeshAsset() : nullptr;
		USkeletalMesh* OverMesh = nullptr;
		if (ArmorMesh)
		{
			(bWornOver ? OverMesh : PartMesh) = ArmorMesh;
		}
		if (FullBody)
		{
			PartMesh = Slot == EVaesenEquipmentSlot::Chest ? FullBody->ArmorMesh.Get() : nullptr;
			OverMesh = nullptr;
		}

		SetMesh(Character->GetBodyPartFor(Slot), PartMesh);
		SetMesh(Character->GetWornOverLayerFor(Slot), OverMesh);
	}
}

void UVaesenEquipmentComponent::RequestEquipFromInventory(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	if (GetOwner()->HasAuthority())
	{
		ServerEquipFromInventory_Implementation(Inventory, SlotIndex);
	}
	else
	{
		ServerEquipFromInventory(Inventory, SlotIndex);
	}
}

void UVaesenEquipmentComponent::RequestUnequip(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	if (GetOwner()->HasAuthority())
	{
		ServerUnequip_Implementation(Slot, To);
	}
	else
	{
		ServerUnequip(Slot, To);
	}
}

void UVaesenEquipmentComponent::ServerEquipFromInventory_Implementation(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	if (Inventory && Inventory->CanBeAccessedBy(Player))
	{
		EquipFromInventory(Inventory, Inventory->GetItemAt(SlotIndex));
	}
}

void UVaesenEquipmentComponent::ServerUnequip_Implementation(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	if (!To || To->CanBeAccessedBy(Player))
	{
		UnequipToInventory(Slot, To);
	}
}
