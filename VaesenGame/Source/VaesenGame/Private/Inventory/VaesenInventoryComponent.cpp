#include "Inventory/VaesenInventoryComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Engine/ActorChannel.h"
#include "Engine/World.h"
#include "Items/VaesenConsumableItem.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenPickup.h"
#include "Items/VaesenPouchItem.h"
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

	// Empty slots everywhere at first (clients get the real contents from the server).
	if (Slots.Num() != Capacity)
	{
		Slots.SetNum(Capacity);
	}

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
		Used += Item ? 1 : 0;
	}
	return Used;
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

int32 UVaesenInventoryComponent::GetWeightRoomFor(const UVaesenItem* Item) const
{
	// Per item, including anything inside it (a full pouch weighs more than an empty one).
	const float UnitWeight = Item ? Item->GetStackWeight() / FMath::Max(Item->GetQuantity(), 1) : 0.0f;
	if (WeightCapacity <= 0.0f || UnitWeight <= 0.0f)
	{
		return MAX_int32;
	}
	const float Room = WeightCapacity - GetCurrentWeight();
	return FMath::Max(0, FMath::FloorToInt32(Room / UnitWeight + KINDA_SMALL_NUMBER));
}

UVaesenItem* UVaesenInventoryComponent::MakeStack(const UVaesenItem* Template, int32 Quantity)
{
	UVaesenItem* Item = Template->DuplicateItem(GetOwner(), Quantity);
	if (UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Item))
	{
		Pouch->EnsurePouchSlots();
	}
	return Item;
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
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex] == Item)
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
	NotifyChanged();
}

void UVaesenInventoryComponent::SetPouchSlot(UVaesenPouchItem* Pouch, int32 SlotIndex, UVaesenItem* Item)
{
	Pouch->EnsurePouchSlots();
	const TArray<UVaesenItem*>& Contents = Pouch->GetContents();
	if (!Contents.IsValidIndex(SlotIndex) || Contents[SlotIndex] == Item)
	{
		return;
	}

	if (UVaesenItem* Old = Contents[SlotIndex])
	{
		UnregisterItem(Old);
	}
	Pouch->SetPouchSlot(SlotIndex, Item);
	if (Item)
	{
		RegisterItem(Item);
	}
	NotifyChanged();
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
	Pouch->EnsurePouchSlots();

	const int32 MaxStack = Template->GetMaxStack();
	int32 Remaining = Quantity;

	// Top up stacks of the same item inside first.
	for (UVaesenItem* Inner : Pouch->GetContents())
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Inner && Inner->GetClass() == Template->GetClass() && Inner->GetQuantity() < MaxStack)
		{
			const int32 Amount = FMath::Min(Remaining, MaxStack - Inner->GetQuantity());
			Inner->SetQuantity(Inner->GetQuantity() + Amount);
			Remaining -= Amount;
		}
	}

	// Then empty pouch slots.
	for (int32 SlotIndex = 0; SlotIndex < Pouch->GetContents().Num() && Remaining > 0; ++SlotIndex)
	{
		if (!Pouch->GetContents()[SlotIndex])
		{
			const int32 Amount = FMath::Min(Remaining, MaxStack);
			SetPouchSlot(Pouch, SlotIndex, MakeStack(Template, Amount));
			Remaining -= Amount;
		}
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
		if (Inner)
		{
			// Into the slots (not back into a pouch); whatever doesn't fit stays in the pouch.
			FText Error;
			const int32 Moved = AddInternal(Inner, Inner->GetQuantity(), Error, false);
			RemoveItem(Inner, Moved);
		}
	}
}

int32 UVaesenInventoryComponent::FindEmptySlot() const
{
	return Slots.IndexOfByPredicate([](const UVaesenItem* Item) { return Item == nullptr; });
}

int32 UVaesenInventoryComponent::AddInternal(const UVaesenItem* Template, int32 Quantity, FText& OutError, bool bIntoPouches)
{
	if (!Template || Quantity <= 0)
	{
		return 0;
	}

	const int32 WeightRoom = GetWeightRoomFor(Template);
	int32 Remaining = FMath::Min(Quantity, WeightRoom);
	const int32 MaxStack = Template->GetMaxStack();
	int32 Given = 0;

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
				const int32 Amount = AddToPouch(Pouch, Template, Remaining);
				Given += Amount;
				Remaining -= Amount;
			}
		}
	}

	// Top up stacks of the same item first.
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
				Given += Amount;
				Remaining -= Amount;
			}
		}
	}

	// Then new stacks in empty slots.
	while (Remaining > 0)
	{
		const int32 SlotIndex = FindEmptySlot();
		if (SlotIndex == INDEX_NONE)
		{
			OutError = LOCTEXT("Full", "Inventory full");
			break;
		}
		const int32 Amount = FMath::Min(Remaining, MaxStack);
		SetSlot(SlotIndex, MakeStack(Template, Amount));
		Given += Amount;
		Remaining -= Amount;
	}

	if (Given < Quantity && OutError.IsEmpty())
	{
		OutError = LOCTEXT("TooHeavy", "Too heavy");
	}
	return Given;
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
	if (!GetOwner()->HasAuthority() || !FindItem(Item, Pouch, SlotIndex))
	{
		return 0;
	}

	const int32 Have = Item->GetQuantity();
	const int32 Taken = (Quantity < 0 || Quantity >= Have) ? Have : Quantity;
	if (Taken == Have)
	{
		if (Pouch)
		{
			SetPouchSlot(Pouch, SlotIndex, nullptr);
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
	// Pouch contents first (that's where the small things are), then the slots, last slot first.
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

	if (!To->Slots.IsValidIndex(ToIndex) || (bSameInventory && ToIndex == FromIndex))
	{
		return false;
	}

	UVaesenItem* Target = To->Slots[ToIndex];

	// Into an empty slot: move, or split off Amount.
	if (!Target)
	{
		if (bSameInventory && Amount == SourceQuantity)
		{
			Slots[ToIndex] = Source;
			Slots[FromIndex] = nullptr;
			NotifyChanged();
			return true;
		}
		const int32 Moving = bSameInventory ? Amount : FMath::Min(Amount, To->GetWeightRoomFor(Source));
		if (Moving <= 0)
		{
			return false;
		}
		To->SetSlot(ToIndex, To->MakeStack(Source, Moving));
		RemoveItem(Source, Moving);
		return true;
	}

	// Onto the same item: merge as far as the stack allows.
	if (Target->GetClass() == Source->GetClass() && Target->GetQuantity() < Target->GetMaxStack())
	{
		int32 Moving = FMath::Min(Amount, Target->GetMaxStack() - Target->GetQuantity());
		if (!bSameInventory)
		{
			Moving = FMath::Min(Moving, To->GetWeightRoomFor(Source));
		}
		if (Moving <= 0)
		{
			return false;
		}
		Target->SetQuantity(Target->GetQuantity() + Moving);
		RemoveItem(Source, Moving);
		return true;
	}

	// Onto a pouch that takes it: inside.
	if (UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Target); Pouch && Pouch->Accepts(Source))
	{
		const int32 Moving = bSameInventory ? Amount : FMath::Min(Amount, To->GetWeightRoomFor(Source));
		const int32 Stored = To->AddToPouch(Pouch, Source, Moving);
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

	const auto Fits = [](const UVaesenInventoryComponent* Inventory, const UVaesenItem* Leaving, const UVaesenItem* Arriving)
	{
		return Inventory->WeightCapacity <= 0.0f
			|| Inventory->GetCurrentWeight() - Leaving->GetStackWeight() + Arriving->GetStackWeight() <= Inventory->WeightCapacity + KINDA_SMALL_NUMBER;
	};
	if (!Fits(To, Target, Source) || !Fits(this, Source, Target))
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

	// In front of whoever drops it, on the ground.
	const AActor* From = DroppedBy ? DroppedBy : GetOwner();
	const FVector Ahead = From->GetActorLocation() + From->GetActorForwardVector() * 80.0f;
	FVector Location = Ahead;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenDropItem), false, From);
	if (World->LineTraceSingleByChannel(Hit, Ahead + FVector(0.0, 0.0, 50.0), Ahead - FVector(0.0, 0.0, 300.0), ECC_Visibility, Params))
	{
		Location = Hit.ImpactPoint;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaesenPickup* Pickup = World->SpawnActor<AVaesenPickup>(DropPickupClass, FTransform(From->GetActorRotation(), Location), SpawnParameters);
	if (!Pickup)
	{
		return nullptr;
	}
	Pickup->InitializePickupFromItem(Item, Dropping);

	// Resting on the ground rather than half in it.
	const FBox Bounds = Pickup->GetComponentsBoundingBox(false);
	if (Bounds.IsValid)
	{
		Pickup->AddActorWorldOffset(FVector(0.0, 0.0, Location.Z - Bounds.Min.Z));
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
