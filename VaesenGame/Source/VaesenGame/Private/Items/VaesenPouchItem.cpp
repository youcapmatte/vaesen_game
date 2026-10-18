#include "Items/VaesenPouchItem.h"

#include "Inventory/VaesenInventoryComponent.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "VaesenItems"

UVaesenPouchItem::UVaesenPouchItem()
{
	bStackable = false;
	MaxStackSize = 1;
	UseActionText = LOCTEXT("PouchUse", "Empty");
}

void UVaesenPouchItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenPouchItem, Contents);
}

bool UVaesenPouchItem::Accepts(const UVaesenItem* Item) const
{
	if (!Item || Item->IsA<UVaesenPouchItem>())
	{
		return false;
	}
	if (AcceptedItemTags.IsEmpty() && AcceptedClasses.IsEmpty())
	{
		return true;
	}
	if (!AcceptedItemTags.IsEmpty() && Item->ItemTags.HasAny(AcceptedItemTags))
	{
		return true;
	}
	return AcceptedClasses.ContainsByPredicate([Item](const TSubclassOf<UVaesenItem>& Class) { return Class && Item->IsA(Class); });
}

int32 UVaesenPouchItem::GetUsedPouchSlots() const
{
	int32 Used = 0;
	for (const UVaesenItem* Item : Contents)
	{
		Used += Item ? Item->SlotSize : 0;
	}
	return Used;
}

bool UVaesenPouchItem::HasRoomFor(const UVaesenItem* Item) const
{
	return Accepts(Item) && (Item->SlotSize <= 0 || GetUsedPouchSlots() + Item->SlotSize <= PouchSlots);
}

int32 UVaesenPouchItem::GetContentCount() const
{
	int32 Count = 0;
	for (const UVaesenItem* Item : Contents)
	{
		Count += Item ? Item->GetQuantity() : 0;
	}
	return Count;
}

float UVaesenPouchItem::GetStackWeight() const
{
	float Total = Super::GetStackWeight();
	for (const UVaesenItem* Item : Contents)
	{
		Total += Item ? Item->GetStackWeight() : 0.0f;
	}
	return Total;
}

void UVaesenPouchItem::GetNestedItems(TArray<UVaesenItem*>& OutItems) const
{
	for (UVaesenItem* Item : Contents)
	{
		if (Item)
		{
			OutItems.Add(Item);
		}
	}
}

UVaesenItem* UVaesenPouchItem::DuplicateItem(UObject* NewOuter, int32 NewQuantity) const
{
	UVaesenPouchItem* Copy = CastChecked<UVaesenPouchItem>(Super::DuplicateItem(NewOuter, NewQuantity));

	// The copy shares the original's contents until each is copied too.
	Copy->Contents.Reset();
	for (const UVaesenItem* Item : Contents)
	{
		if (Item)
		{
			Copy->Contents.Add(Item->DuplicateItem(NewOuter, Item->GetQuantity()));
		}
	}
	return Copy;
}

void UVaesenPouchItem::AddContent(UVaesenItem* Item)
{
	if (Item)
	{
		Contents.Add(Item);
		Item->SetOwningInventory(GetOwningInventory());
	}
}

bool UVaesenPouchItem::RemoveContent(UVaesenItem* Item)
{
	return Contents.Remove(Item) > 0;
}

bool UVaesenPouchItem::CanUse_Implementation(const AVaesenPlayerCharacter* Character) const
{
	return Character && Contents.Num() > 0;
}

void UVaesenPouchItem::Use_Implementation(AVaesenPlayerCharacter* Character)
{
	if (UVaesenInventoryComponent* Inventory = GetOwningInventory())
	{
		Inventory->EmptyPouch(this);
	}
}

void UVaesenPouchItem::OnRep_Contents()
{
	UVaesenInventoryComponent* Inventory = GetOwningInventory();
	for (UVaesenItem* Item : Contents)
	{
		if (Item)
		{
			Item->SetOwningInventory(Inventory);
		}
	}
	if (Inventory)
	{
		Inventory->NotifyChanged();
	}
}

#undef LOCTEXT_NAMESPACE
