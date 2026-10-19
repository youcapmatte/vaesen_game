#include "Inventory/VaesenInventoryComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "Core/VaesenSettings.h"
#include "Items/VaesenConsumableItem.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenPickup.h"
#include "Items/VaesenPouchItem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/VaesenPlayerController.h"

#define LOCTEXT_NAMESPACE "VaesenInventory"

UVaesenInventoryComponent::UVaesenInventoryComponent()
{
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	DropPickupClass = AVaesenPickup::StaticClass();
}

void UVaesenInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenInventoryComponent, Slots);
}

bool UVaesenInventoryComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	// For owners that don't use the registered subobject list (SetSlot registers the items for those that do).
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (IsUsingRegisteredSubObjectList())
	{
		return bWroteSomething;
	}
	for (UVaesenItem* Item : Slots)
	{
		if (Item)
		{
			bWroteSomething |= Channel->ReplicateSubobject(Item, *Bunch, *RepFlags);
			TArray<UVaesenItem*> Nested;
			Item->GetNestedItems(Nested);
			for (UVaesenItem* Inner : Nested)
			{
				bWroteSomething |= Channel->ReplicateSubobject(Inner, *Bunch, *RepFlags);
			}
		}
	}
	return bWroteSomething;
}

void UVaesenInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		for (const FVaesenItemStack& Stack : StartingItems)
		{
			TryAddItemOfClass(Stack.ItemClass, Stack.Quantity);
		}
	}
}

// --- Reading ---

UVaesenItem* UVaesenInventoryComponent::GetItemAt(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Get() : nullptr;
}

int32 UVaesenInventoryComponent::FindSlotOf(const UVaesenItem* Item) const
{
	return Item ? Slots.IndexOfByKey(Item) : INDEX_NONE;
}

float UVaesenInventoryComponent::GetCurrentWeight() const
{
	float Weight = 0.0f;
	for (const UVaesenItem* Item : Slots)
	{
		if (Item)
		{
			Weight += Item->GetStackWeight();
		}
	}
	return Weight;
}

int32 UVaesenInventoryComponent::GetUsedSlotCount() const
{
	int32 Used = 0;
	for (const UVaesenItem* Item : Slots)
	{
		Used += Item ? Item->SlotSize : 0;
	}
	return Used;
}

int32 UVaesenInventoryComponent::GetFreeSlotCount() const
{
	return HasUnlimitedCapacity() ? -1 : FMath::Max(0, Capacity - GetUsedSlotCount());
}

bool UVaesenInventoryComponent::HasRoomFor(const UVaesenItem* Item, int32 FreedSlots) const
{
	if (!Item)
	{
		return false;
	}
	return HasUnlimitedCapacity() || Item->SlotSize <= 0 || GetUsedSlotCount() - FreedSlots + Item->SlotSize <= Capacity;
}

int32 UVaesenInventoryComponent::GetItemCount(TSubclassOf<UVaesenItem> ItemClass) const
{
	int32 Count = 0;
	for (const UVaesenItem* Item : Slots)
	{
		if (!Item || !ItemClass)
		{
			continue;
		}
		if (Item->IsA(ItemClass))
		{
			Count += Item->GetQuantity();
		}
		TArray<UVaesenItem*> Nested;
		Item->GetNestedItems(Nested);
		for (const UVaesenItem* Inner : Nested)
		{
			Count += Inner->IsA(ItemClass) ? Inner->GetQuantity() : 0;
		}
	}
	return Count;
}

FText UVaesenInventoryComponent::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return GetOwner() ? FText::FromString(GetOwner()->GetName()) : FText::GetEmpty();
}

bool UVaesenInventoryComponent::CanBeAccessedBy(const AVaesenPlayerCharacter* Character) const
{
	const AActor* Owner = GetOwner();
	if (!Character || !Owner)
	{
		return false;
	}
	if (Owner == Character)
	{
		return true;
	}
	return bAccessibleByOthers && FVector::Dist(Character->GetActorLocation(), Owner->GetActorLocation()) <= AccessDistance;
}

AVaesenPlayerCharacter* UVaesenInventoryComponent::GetOwningCharacter() const
{
	return Cast<AVaesenPlayerCharacter>(GetOwner());
}

// --- Changing ---

UVaesenItem* UVaesenInventoryComponent::MakeStack(const UVaesenItem* Template, int32 Quantity)
{
	return Template->DuplicateItem(GetOwner(), Quantity);
}

void UVaesenInventoryComponent::RegisterItem(UVaesenItem* Item)
{
	AddReplicatedSubObject(Item);
	Item->SetOwningInventory(this);
	TArray<UVaesenItem*> Nested;
	Item->GetNestedItems(Nested);
	for (UVaesenItem* Inner : Nested)
	{
		AddReplicatedSubObject(Inner);
		Inner->SetOwningInventory(this);
	}
}

void UVaesenInventoryComponent::UnregisterItem(UVaesenItem* Item)
{
	TArray<UVaesenItem*> Nested;
	Item->GetNestedItems(Nested);
	for (UVaesenItem* Inner : Nested)
	{
		RemoveReplicatedSubObject(Inner);
		Inner->SetOwningInventory(nullptr);
	}
	RemoveReplicatedSubObject(Item);
	Item->SetOwningInventory(nullptr);
}

void UVaesenInventoryComponent::SetSlot(int32 SlotIndex, UVaesenItem* Item)
{
	if (SlotIndex < 0 || SlotIndex > GetMaxPosition())
	{
		return;
	}
	if (SlotIndex >= Slots.Num())
	{
		if (!Item)
		{
			return;
		}
		Slots.SetNum(SlotIndex + 1);
	}
	if (Slots[SlotIndex] == Item)
	{
		return;
	}

	if (UVaesenItem* Old = Slots[SlotIndex])
	{
		UnregisterItem(Old);
	}
	Slots[SlotIndex] = Item;
	if (Item)
	{
		RegisterItem(Item);
	}

	// No gaps at the end.
	while (Slots.Num() > 0 && !Slots.Last())
	{
		Slots.Pop();
	}
	NotifyChanged();
}

void UVaesenInventoryComponent::AddToPouchContents(UVaesenPouchItem* Pouch, UVaesenItem* Item)
{
	Pouch->AddContent(Item);
	RegisterItem(Item);
	NotifyChanged();
}

void UVaesenInventoryComponent::RemoveFromPouchContents(UVaesenPouchItem* Pouch, UVaesenItem* Item)
{
	if (Pouch->RemoveContent(Item))
	{
		UnregisterItem(Item);
		NotifyChanged();
	}
}

bool UVaesenInventoryComponent::FindItem(const UVaesenItem* Item, UVaesenPouchItem*& OutPouch, int32& OutSlotIndex) const
{
	OutPouch = nullptr;
	OutSlotIndex = FindSlotOf(Item);
	if (!Item || OutSlotIndex != INDEX_NONE)
	{
		return OutSlotIndex != INDEX_NONE;
	}
	for (UVaesenItem* SlotItem : Slots)
	{
		if (UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(SlotItem))
		{
			const int32 Inside = Pouch->GetContents().IndexOfByKey(Item);
			if (Inside != INDEX_NONE)
			{
				OutPouch = Pouch;
				OutSlotIndex = Inside;
				return true;
			}
		}
	}
	return false;
}

int32 UVaesenInventoryComponent::AddToPouch(UVaesenPouchItem* Pouch, const UVaesenItem* Template, int32 Quantity)
{
	if (!Pouch || !Template || Quantity <= 0 || !Pouch->Accepts(Template))
	{
		return 0;
	}

	const int32 MaxStack = Template->GetMaxStack();
	int32 Remaining = Quantity;

	// Top up stacks of the same item inside first.
	for (UVaesenItem* Inner : Pouch->GetContents())
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Inner->GetClass() == Template->GetClass() && Inner->GetQuantity() < MaxStack)
		{
			const int32 Amount = FMath::Min(Remaining, MaxStack - Inner->GetQuantity());
			Inner->SetQuantity(Inner->GetQuantity() + Amount);
			Remaining -= Amount;
		}
	}

	// Then new stacks while the pouch has room.
	while (Remaining > 0 && Pouch->HasRoomFor(Template))
	{
		const int32 Amount = FMath::Min(Remaining, MaxStack);
		AddToPouchContents(Pouch, MakeStack(Template, Amount));
		Remaining -= Amount;
	}
	return Quantity - Remaining;
}

void UVaesenInventoryComponent::EmptyPouch(UVaesenPouchItem* Pouch)
{
	if (!Pouch || !GetOwner()->HasAuthority() || FindSlotOf(Pouch) == INDEX_NONE)
	{
		return;
	}

	for (UVaesenItem* Inner : TArray<UVaesenItem*>(Pouch->GetContents()))
	{
		// Into the inventory (not back into a pouch); whatever doesn't fit stays in the pouch.
		FText Error;
		const int32 Moved = AddInternal(Inner, Inner->GetQuantity(), Error, false);
		RemoveItem(Inner, Moved);
	}
}

int32 UVaesenInventoryComponent::FindEmptySlot() const
{
	const int32 Gap = Slots.IndexOfByPredicate([](const UVaesenItem* Item) { return Item == nullptr; });
	return Gap != INDEX_NONE ? Gap : Slots.Num();
}

int32 UVaesenInventoryComponent::AddInternal(const UVaesenItem* Template, int32 Quantity, FText& OutError, bool bIntoPouches)
{
	if (!Template || Quantity <= 0)
	{
		return 0;
	}

	int32 Remaining = Quantity;
	const int32 MaxStack = Template->GetMaxStack();

	// Into pouches that take it, so it doesn't use up slots.
	if (bIntoPouches)
	{
		for (UVaesenItem* Item : Slots)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Item))
			{
				Remaining -= AddToPouch(Pouch, Template, Remaining);
			}
		}
	}

	// Top up stacks of the same item.
	if (MaxStack > 1)
	{
		for (UVaesenItem* Item : Slots)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (Item && Item->GetClass() == Template->GetClass() && Item->GetQuantity() < MaxStack)
			{
				const int32 Amount = FMath::Min(Remaining, MaxStack - Item->GetQuantity());
				Item->SetQuantity(Item->GetQuantity() + Amount);
				Remaining -= Amount;
			}
		}
	}

	// Then new stacks while there's room (always, for items that take no slots).
	while (Remaining > 0)
	{
		if (!HasRoomFor(Template))
		{
			OutError = LOCTEXT("Full", "Inventory full");
			break;
		}
		const int32 Amount = FMath::Min(Remaining, MaxStack);
		SetSlot(FindEmptySlot(), MakeStack(Template, Amount));
		Remaining -= Amount;
	}

	return Quantity - Remaining;
}

FVaesenItemAddResult UVaesenInventoryComponent::TryAddItemOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity)
{
	return ItemClass ? TryAddItem(ItemClass->GetDefaultObject<UVaesenItem>(), Quantity) : FVaesenItemAddResult();
}

FVaesenItemAddResult UVaesenInventoryComponent::TryAddItem(const UVaesenItem* Item, int32 Quantity)
{
	FVaesenItemAddResult Result;
	if (!Item || !GetOwner()->HasAuthority())
	{
		return Result;
	}

	Result.AmountToGive = Quantity < 0 ? Item->GetQuantity() : Quantity;
	Result.AmountGiven = AddInternal(Item, Result.AmountToGive, Result.ErrorText);
	Result.Result = Result.AmountGiven == 0 ? EVaesenAddItemOutcome::None
		: Result.AmountGiven < Result.AmountToGive ? EVaesenAddItemOutcome::Some : EVaesenAddItemOutcome::All;
	return Result;
}

int32 UVaesenInventoryComponent::RemoveItem(UVaesenItem* Item, int32 Quantity)
{
	UVaesenPouchItem* Pouch = nullptr;
	int32 SlotIndex = INDEX_NONE;
	if (Quantity == 0 || !GetOwner()->HasAuthority() || !FindItem(Item, Pouch, SlotIndex))
	{
		return 0;
	}

	const int32 Have = Item->GetQuantity();
	const int32 Taken = (Quantity < 0 || Quantity >= Have) ? Have : Quantity;
	if (Taken == Have)
	{
		if (Pouch)
		{
			RemoveFromPouchContents(Pouch, Item);
		}
		else
		{
			SetSlot(SlotIndex, nullptr);
		}
	}
	else
	{
		Item->SetQuantity(Have - Taken);
	}
	return Taken;
}

int32 UVaesenInventoryComponent::RemoveItemsOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity)
{
	// Pouch contents first (that's where the small things are), then the stacks, last first.
	TArray<UVaesenItem*> Candidates;
	for (UVaesenItem* Item : Slots)
	{
		if (Item)
		{
			Item->GetNestedItems(Candidates);
		}
	}
	for (int32 SlotIndex = Slots.Num() - 1; SlotIndex >= 0; --SlotIndex)
	{
		if (Slots[SlotIndex])
		{
			Candidates.Add(Slots[SlotIndex]);
		}
	}

	int32 Taken = 0;
	for (UVaesenItem* Item : Candidates)
	{
		if (Taken >= Quantity)
		{
			break;
		}
		if (ItemClass && Item->IsA(ItemClass))
		{
			Taken += RemoveItem(Item, Quantity - Taken);
		}
	}
	return Taken;
}

bool UVaesenInventoryComponent::MoveItem(int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount)
{
	UVaesenItem* Source = GetItemAt(FromIndex);
	if (!Source || !To || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const int32 SourceQuantity = Source->GetQuantity();
	Amount = (Amount <= 0 || Amount > SourceQuantity) ? SourceQuantity : Amount;
	const bool bSameInventory = To == this;

	// Anywhere it fits.
	if (ToIndex == INDEX_NONE)
	{
		if (bSameInventory)
		{
			return false;
		}
		FText Error;
		const int32 Given = To->AddInternal(Source, Amount, Error);
		RemoveItem(Source, Given);
		return Given > 0;
	}

	if (ToIndex < 0 || ToIndex > To->GetMaxPosition() || (bSameInventory && ToIndex == FromIndex))
	{
		return false;
	}

	UVaesenItem* Target = To->GetItemAt(ToIndex);

	// Into an empty position: move, or split off Amount (a new stack, so it needs room).
	if (!Target)
	{
		if (bSameInventory && Amount == SourceQuantity)
		{
			// Rearranging takes no extra room.
			if (ToIndex >= Slots.Num())
			{
				Slots.SetNum(ToIndex + 1);
			}
			Slots[ToIndex] = Source;
			Slots[FromIndex] = nullptr;
			while (Slots.Num() > 0 && !Slots.Last())
			{
				Slots.Pop();
			}
			NotifyChanged();
			return true;
		}
		if (!To->HasRoomFor(Source))
		{
			return false;
		}
		To->SetSlot(ToIndex, To->MakeStack(Source, Amount));
		RemoveItem(Source, Amount);
		return true;
	}

	// Onto the same item: merge as far as the stack allows.
	if (Target->GetClass() == Source->GetClass() && Target->GetQuantity() < Target->GetMaxStack())
	{
		const int32 Moving = FMath::Min(Amount, Target->GetMaxStack() - Target->GetQuantity());
		Target->SetQuantity(Target->GetQuantity() + Moving);
		RemoveItem(Source, Moving);
		return true;
	}

	// Onto a pouch that takes it: inside.
	if (UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Target); Pouch && Pouch->Accepts(Source))
	{
		const int32 Stored = To->AddToPouch(Pouch, Source, Amount);
		RemoveItem(Source, Stored);
		return Stored > 0;
	}

	// Onto another item: swap whole stacks.
	if (Amount != SourceQuantity)
	{
		return false;
	}
	if (bSameInventory)
	{
		Swap(Slots[FromIndex], Slots[ToIndex]);
		NotifyChanged();
		return true;
	}

	// Each side gives up one stack's slots and takes the other's.
	if (!To->HasRoomFor(Source, Target->SlotSize) || !HasRoomFor(Target, Source->SlotSize))
	{
		return false;
	}
	UVaesenItem* IntoTarget = To->MakeStack(Source, Source->GetQuantity());
	UVaesenItem* IntoHere = MakeStack(Target, Target->GetQuantity());
	To->SetSlot(ToIndex, IntoTarget);
	SetSlot(FromIndex, IntoHere);
	return true;
}

bool UVaesenInventoryComponent::UseItem(int32 SlotIndex, AVaesenPlayerCharacter* Character)
{
	UVaesenItem* Item = GetItemAt(SlotIndex);
	if (!Item || !GetOwner()->HasAuthority() || !Item->CanUse(Character))
	{
		return false;
	}

	Item->Use(Character);
	const UVaesenConsumableItem* Consumable = Cast<UVaesenConsumableItem>(Item);
	if (Consumable && Consumable->bConsumedOnUse)
	{
		RemoveItem(Item, 1);
	}
	return true;
}

AVaesenPickup* UVaesenInventoryComponent::DropItem(int32 SlotIndex, int32 Amount, AActor* DroppedBy)
{
	UVaesenItem* Item = GetItemAt(SlotIndex);
	UWorld* World = GetWorld();
	if (!Item || !World || !DropPickupClass || !GetOwner()->HasAuthority() || !Item->bCanBeDropped)
	{
		return nullptr;
	}

	const int32 Dropping = (Amount <= 0 || Amount > Item->GetQuantity()) ? Item->GetQuantity() : Amount;
	AVaesenPickup* Pickup = AVaesenPickup::SpawnDropped(World, DropPickupClass, Item, Dropping, DroppedBy ? DroppedBy : GetOwner());
	if (!Pickup)
	{
		return nullptr;
	}

	RemoveItem(Item, Dropping);
	return Pickup;
}

// --- Requests ---

void UVaesenInventoryComponent::RequestMoveItem(UVaesenInventoryComponent* From, int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount)
{
	if (GetOwner()->HasAuthority())
	{
		ServerMoveItem_Implementation(From, FromIndex, To, ToIndex, Amount);
	}
	else
	{
		ServerMoveItem(From, FromIndex, To, ToIndex, Amount);
	}
}

void UVaesenInventoryComponent::RequestUseItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	if (GetOwner()->HasAuthority())
	{
		ServerUseItem_Implementation(Inventory, SlotIndex);
	}
	else
	{
		ServerUseItem(Inventory, SlotIndex);
	}
}

void UVaesenInventoryComponent::RequestDropItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex, int32 Amount)
{
	// Heard straight away by the player dropping it.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled() && Inventory && Inventory->GetItemAt(SlotIndex))
	{
		if (USoundBase* Sound = UVaesenSettings::Get()->DropSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}

	if (GetOwner()->HasAuthority())
	{
		ServerDropItem_Implementation(Inventory, SlotIndex, Amount);
	}
	else
	{
		ServerDropItem(Inventory, SlotIndex, Amount);
	}
}

void UVaesenInventoryComponent::ServerMoveItem_Implementation(UVaesenInventoryComponent* From, int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount)
{
	const AVaesenPlayerCharacter* Character = GetOwningCharacter();
	if (From && To && From->CanBeAccessedBy(Character) && To->CanBeAccessedBy(Character))
	{
		From->MoveItem(FromIndex, To, ToIndex, Amount);
	}
}

void UVaesenInventoryComponent::ServerUseItem_Implementation(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	AVaesenPlayerCharacter* Character = GetOwningCharacter();
	if (Inventory && Inventory->CanBeAccessedBy(Character))
	{
		Inventory->UseItem(SlotIndex, Character);
	}
}

void UVaesenInventoryComponent::ServerDropItem_Implementation(UVaesenInventoryComponent* Inventory, int32 SlotIndex, int32 Amount)
{
	AVaesenPlayerCharacter* Character = GetOwningCharacter();
	if (Inventory && Inventory->CanBeAccessedBy(Character))
	{
		Inventory->DropItem(SlotIndex, Amount, Character);
	}
}

void UVaesenInventoryComponent::OpenContainerForOwner(UVaesenInventoryComponent* Container)
{
	if (GetOwner()->HasAuthority() && Container && Container->CanBeAccessedBy(GetOwningCharacter()))
	{
		ClientOpenContainer(Container);
	}
}

void UVaesenInventoryComponent::ClientOpenContainer_Implementation(UVaesenInventoryComponent* Container)
{
	const AVaesenPlayerCharacter* Character = GetOwningCharacter();
	if (AVaesenPlayerController* Controller = Character ? Cast<AVaesenPlayerController>(Character->GetController()) : nullptr)
	{
		Controller->OpenInventory(Container);
	}
	OnContainerOpened.Broadcast(Container);
}

// --- Updates ---

void UVaesenInventoryComponent::OnRep_Slots()
{
	for (UVaesenItem* Item : Slots)
	{
		if (Item)
		{
			Item->SetOwningInventory(this);
			TArray<UVaesenItem*> Nested;
			Item->GetNestedItems(Nested);
			for (UVaesenItem* Inner : Nested)
			{
				Inner->SetOwningInventory(this);
			}
		}
	}
	NotifyChanged();
}

void UVaesenInventoryComponent::NotifyChanged()
{
	OnInventoryUpdated.Broadcast();
}

#undef LOCTEXT_NAMESPACE
