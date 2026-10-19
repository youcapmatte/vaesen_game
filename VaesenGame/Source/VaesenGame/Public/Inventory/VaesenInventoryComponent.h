#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaesenInventoryComponent.generated.h"

class AVaesenPickup;
class AVaesenPlayerCharacter;
class UVaesenItem;
class UVaesenPouchItem;

UENUM(BlueprintType)
enum class EVaesenAddItemOutcome : uint8
{
	/** Nothing fit. */
	None,
	/** Some fit; the rest is left where it was. */
	Some,
	All
};

USTRUCT(BlueprintType)
struct FVaesenItemAddResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 AmountToGive = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 AmountGiven = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EVaesenAddItemOutcome Result = EVaesenAddItemOutcome::None;

	/** Why not everything fit, e.g. "Inventory full". */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	FText ErrorText;
};

/** An amount of one kind of item, e.g. for a chest's starting contents. */
USTRUCT(BlueprintType)
struct FVaesenItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<UVaesenItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenInventoryUpdated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVaesenContainerOpened, UVaesenInventoryComponent*, Container);

/**
 * A slot-based inventory that can go on any actor: the player, a chest, a corpse, a cart. It holds item stacks; each
 * stack takes its item's SlotSize in slots (0 = none, however many are in it), and the stacks together may take up
 * to Capacity slots (-1 = unlimited). Stacks of the same item merge up to the item's stack size. Items are instances
 * owned by this actor and replicate as its subobjects, so everyone sees the contents.
 *
 * The stacks are kept in a list of positions (GetSlots) so the player can arrange them; a moved or used-up stack
 * leaves a gap (null) that new stacks fill first. Positions aren't capacity: capacity is counted in slot sizes.
 *
 * Pouches (UVaesenPouchItem) in the slots hold items of their own: new items go into a pouch that accepts them
 * first, and only take a slot when no pouch has room. Counting and removing items (GetItemCount,
 * RemoveItemsOfClass) include what's in pouches.
 *
 * Changes happen on the server. A player changes inventories through their own inventory's Request functions
 * (move, split, use, drop), which go to the server and are checked there: a player may touch their own inventory
 * and others they're allowed into (bAccessibleByOthers, within AccessDistance), e.g. a chest they've opened.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaesenInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	// --- Setup ---

	/** Shown above the slots, e.g. "Inventory", "Chest". Empty = the actor's name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FText DisplayName;

	/** How many slots the stacks in it may take together. -1 = unlimited (e.g. a storage chest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "-1", ClampMax = "500"))
	int32 Capacity = 20;

	/** Other players may take from and put into it (a chest). Off: only the owning actor (a player's own). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	bool bAccessibleByOthers = true;

	/** How close other players must be to use it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0", Units = "cm", EditCondition = "bAccessibleByOthers"))
	float AccessDistance = 350.0f;

	/** Put in on the server when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TArray<FVaesenItemStack> StartingItems;

	/** What dropped items become in the world. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<AVaesenPickup> DropPickupClass;

	// --- Reading ---

	/** The stacks by position; null = a gap. Not one entry per slot of capacity (see GetUsedSlotCount). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<UVaesenItem*>& GetSlots() const { return ObjectPtrDecay(Slots); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UVaesenItem* GetItemAt(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindSlotOf(const UVaesenItem* Item) const;

	/** Total weight of everything in it (information only; capacity is in slots). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentWeight() const;

	/** Slots taken: the stacks' slot sizes added up. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetUsedSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasUnlimitedCapacity() const { return Capacity < 0; }

	/** Slots still free; -1 when unlimited. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetFreeSlotCount() const;

	/** Whether a new stack of Item fits, after FreedSlots are given up (e.g. by a stack leaving in a swap). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasRoomFor(const UVaesenItem* Item, int32 FreedSlots = 0) const;

	/** How many of this kind of item it holds, across stacks (pouches included). */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(TSubclassOf<UVaesenItem> ItemClass) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1) const { return GetItemCount(ItemClass) >= Quantity; }

	/** A stack of this kind of item: in the slots first, then in pouches. Null if there's none. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	UVaesenItem* FindItemOfClass(TSubclassOf<UVaesenItem> ItemClass) const;

	/** DisplayName, or the actor's name. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FText GetDisplayName() const;

	/** Whether this player may take from and put into it. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanBeAccessedBy(const AVaesenPlayerCharacter* Character) const;

	// --- Changing (server) ---

	/** Adds Quantity of ItemClass: into pouches, onto existing stacks, then as new stacks while there's room. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	FVaesenItemAddResult TryAddItemOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1);

	/** Adds a copy of Item (its quantity, or Quantity if given); Item itself is left as it is. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	FVaesenItemAddResult TryAddItem(const UVaesenItem* Item, int32 Quantity = -1);

	/** Takes Quantity from this item's stack (all of it if Quantity is -1 or more than the stack). Returns how many. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItem(UVaesenItem* Item, int32 Quantity = -1);

	/** Takes Quantity of ItemClass, from any stacks (pouches included). Returns how many were taken. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItemsOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1);

	/**
	 * Moves Amount (-1 = the whole stack) from a position here to a position in To (which may be this inventory; a
	 * position past the last stack is an empty one). Into an empty position: moves or splits. Onto the same item: merges as far as the stack allows. Onto another item: swaps
	 * (whole stacks only), unless it's a pouch that takes it: then it goes inside. ToIndex -1: wherever it fits.
	 * Returns whether anything changed.
	 */
	bool MoveItem(int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount = -1);

	/** Tips a pouch's contents out into this inventory's slots, as far as they fit. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void EmptyPouch(UVaesenPouchItem* Pouch);

	/** Uses the item in a slot for Character (eats it, reads it...). Used-up consumables leave the stack. */
	bool UseItem(int32 SlotIndex, AVaesenPlayerCharacter* Character);

	/** The same for any item in this inventory, including one inside a pouch (e.g. from a quick slot). */
	bool UseItemInstance(UVaesenItem* Item, AVaesenPlayerCharacter* Character);

	/** Drops Amount (-1 = all) from a slot as a pickup that falls to the ground in front of the owner (or DroppedBy). */
	AVaesenPickup* DropItem(int32 SlotIndex, int32 Amount = -1, AActor* DroppedBy = nullptr);

	// --- Requests from this player's own inventory (callable on their machine; the server checks access) ---

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestMoveItem(UVaesenInventoryComponent* From, int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount = -1);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestUseItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestDropItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex, int32 Amount = -1);

	/** Server: shows Container (e.g. a chest just opened) next to this player's inventory on their screen. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void OpenContainerForOwner(UVaesenInventoryComponent* Container);

	// --- Events ---

	/** Anything changed: slots, quantities. On the server and every client. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FVaesenInventoryUpdated OnInventoryUpdated;

	/** This player opened a container (on their machine). */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FVaesenContainerOpened OnContainerOpened;

	/** Called by items when their quantity changes. */
	void NotifyChanged();

protected:
	virtual void BeginPlay() override;

private:
	AVaesenPlayerCharacter* GetOwningCharacter() const;

	/** A new stack in this inventory: a copy of Template with Quantity. */
	UVaesenItem* MakeStack(const UVaesenItem* Template, int32 Quantity);

	/** Starts (or stops) replicating Item and anything inside it as this actor's subobjects. */
	void RegisterItem(UVaesenItem* Item);
	void UnregisterItem(UVaesenItem* Item);

	/**
	 * Puts Item (owned by this actor) at a position, or empties it (null), keeping replication in step. Positions
	 * past the end are added; gaps at the end are dropped.
	 */
	void SetSlot(int32 SlotIndex, UVaesenItem* Item);

	/** Adds Item (owned by this actor) to a pouch here, or takes one out, keeping replication in step. */
	void AddToPouchContents(UVaesenPouchItem* Pouch, UVaesenItem* Item);
	void RemoveFromPouchContents(UVaesenPouchItem* Pouch, UVaesenItem* Item);

	/** Where a new stack goes: the first gap, or a new position at the end. */
	int32 FindEmptySlot() const;

	/** Highest position a player may name (a little past the end, for the UI's empty cells). */
	int32 GetMaxPosition() const { return Slots.Num() + 200; }

	/** Where Item is: a slot here (OutPouch null), or a slot inside a pouch here. False if it's not in this inventory. */
	bool FindItem(const UVaesenItem* Item, UVaesenPouchItem*& OutPouch, int32& OutSlotIndex) const;

	/** Adds up to Quantity copies of Template into a pouch (topping up stacks first, then while it has room); returns how many. */
	int32 AddToPouch(UVaesenPouchItem* Pouch, const UVaesenItem* Template, int32 Quantity);

	/** Adds up to Quantity copies of Template (into pouches first if bIntoPouches); returns how many. */
	int32 AddInternal(const UVaesenItem* Template, int32 Quantity, FText& OutError, bool bIntoPouches = true);

	UFUNCTION(Server, Reliable)
	void ServerMoveItem(UVaesenInventoryComponent* From, int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerUseItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(UVaesenInventoryComponent* Inventory, int32 SlotIndex, int32 Amount);

	UFUNCTION(Client, Reliable)
	void ClientOpenContainer(UVaesenInventoryComponent* Container);

	UFUNCTION()
	void OnRep_Slots();

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<TObjectPtr<UVaesenItem>> Slots;
};
