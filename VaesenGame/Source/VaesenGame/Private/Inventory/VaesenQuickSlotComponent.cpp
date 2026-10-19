#include "Inventory/VaesenQuickSlotComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenContainerItem.h"
#include "Items/VaesenItem.h"
#include "Net/UnrealNetwork.h"

UVaesenQuickSlotComponent::UVaesenQuickSlotComponent()
{
	SetIsReplicatedByDefault(true);
}

void UVaesenQuickSlotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only the player needs their own shortcuts.
	DOREPLIFETIME_CONDITION(UVaesenQuickSlotComponent, Slots, COND_OwnerOnly);
}

void UVaesenQuickSlotComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		// A worn bag can add slots.
		if (const AVaesenPlayerCharacter* Character = GetCharacter())
		{
			if (UVaesenEquipmentComponent* Equipment = Character->GetEquipment())
			{
				Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &UVaesenQuickSlotComponent::UpdateSlotCount);
			}
		}
		UpdateSlotCount();
	}
}

AVaesenPlayerCharacter* UVaesenQuickSlotComponent::GetCharacter() const
{
	return Cast<AVaesenPlayerCharacter>(GetOwner());
}

UVaesenInventoryComponent* UVaesenQuickSlotComponent::GetInventory() const
{
	const AVaesenPlayerCharacter* Character = GetCharacter();
	return Character ? Character->GetInventory() : nullptr;
}

TSubclassOf<UVaesenItem> UVaesenQuickSlotComponent::GetSlot(int32 Index) const
{
	return Slots.IsValidIndex(Index) ? Slots[Index] : nullptr;
}

int32 UVaesenQuickSlotComponent::GetSlotCount(int32 Index) const
{
	const UVaesenInventoryComponent* Inventory = GetInventory();
	const TSubclassOf<UVaesenItem> ItemClass = GetSlot(Index);
	return Inventory && ItemClass ? Inventory->GetItemCount(ItemClass) : 0;
}

UVaesenItem* UVaesenQuickSlotComponent::FindItemForSlot(int32 Index) const
{
	const UVaesenInventoryComponent* Inventory = GetInventory();
	const TSubclassOf<UVaesenItem> ItemClass = GetSlot(Index);
	return Inventory && ItemClass ? Inventory->FindItemOfClass(ItemClass) : nullptr;
}

void UVaesenQuickSlotComponent::UpdateSlotCount()
{
	int32 Count = BaseSlots;
	if (const AVaesenPlayerCharacter* Character = GetCharacter())
	{
		const UVaesenEquipmentComponent* Equipment = Character->GetEquipment();
		if (const UVaesenContainerItem* Bag = Equipment ? Cast<UVaesenContainerItem>(Equipment->GetItemInSlot(EVaesenEquipmentSlot::Bag)) : nullptr)
		{
			Count += Bag->ExtraQuickSlots;
		}
	}
	Count = FMath::Clamp(Count, 0, MaxSlots);

	if (Slots.Num() != Count)
	{
		Slots.SetNum(Count);
		OnQuickSlotsChanged.Broadcast();
	}
}

void UVaesenQuickSlotComponent::Assign(int32 Index, TSubclassOf<UVaesenItem> ItemClass)
{
	if (!Slots.IsValidIndex(Index))
	{
		return;
	}
	if (ItemClass)
	{
		const UVaesenItem* Defaults = ItemClass->GetDefaultObject<UVaesenItem>();
		if (!Defaults->CanBeQuickSlotted())
		{
			return;
		}
		// One slot per kind of item.
		for (TSubclassOf<UVaesenItem>& Other : Slots)
		{
			if (Other == ItemClass)
			{
				Other = nullptr;
			}
		}
	}
	Slots[Index] = ItemClass;
	OnQuickSlotsChanged.Broadcast();
}

void UVaesenQuickSlotComponent::RequestAssign(int32 Index, TSubclassOf<UVaesenItem> ItemClass)
{
	if (GetOwner()->HasAuthority())
	{
		Assign(Index, ItemClass);
	}
	else
	{
		ServerAssign(Index, ItemClass);
	}
}

void UVaesenQuickSlotComponent::RequestClear(int32 Index)
{
	RequestAssign(Index, nullptr);
}

void UVaesenQuickSlotComponent::ServerAssign_Implementation(int32 Index, TSubclassOf<UVaesenItem> ItemClass)
{
	Assign(Index, ItemClass);
}

void UVaesenQuickSlotComponent::RequestUse(int32 Index)
{
	if (GetOwner()->HasAuthority())
	{
		ServerUse_Implementation(Index);
	}
	else
	{
		ServerUse(Index);
	}
}

void UVaesenQuickSlotComponent::ServerUse_Implementation(int32 Index)
{
	UVaesenInventoryComponent* Inventory = GetInventory();
	if (UVaesenItem* Item = FindItemForSlot(Index); Item && Inventory)
	{
		Inventory->UseItemInstance(Item, GetCharacter());
	}
}

void UVaesenQuickSlotComponent::OnRep_Slots()
{
	OnQuickSlotsChanged.Broadcast();
}
