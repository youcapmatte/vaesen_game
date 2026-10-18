#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaesenInventoryComponent.generated.h"

class AVaesenPickup;
class AVaesenPlayerCharacter;
class UVaesenItem;

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

	/** Why not everything fit, e.g. "Too heavy". */
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
 * A slot-based inventory that can go on any actor: the player, a chest, a corpse, a cart. Each slot holds one item
 * stack (or nothing); stacks of the same item merge up to the item's stack size, and the total weight can be
 * limited. Items are instances owned by this actor and replicate as its subobjects, so everyone sees the contents.
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "200"))
	int32 Capacity = 20;

	/** Most it can hold, in kg. 0 = no limit (e.g. a chest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0", Units = "kg"))
	float WeightCapacity = 0.0f;

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

	/** One entry per slot; null = empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<UVaesenItem*>& GetSlots() const { return ObjectPtrDecay(Slots); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UVaesenItem* GetItemAt(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindSlotOf(const UVaesenItem* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	float GetCurrentWeight() const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetUsedSlotCount() const;

	/** How many of this kind of item it holds, across stacks. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(TSubclassOf<UVaesenItem> ItemClass) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool HasItem(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1) const { return GetItemCount(ItemClass) >= Quantity; }

	/** DisplayName, or the actor's name. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FText GetDisplayName() const;

	/** Whether this player may take from and put into it. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool CanBeAccessedBy(const AVaesenPlayerCharacter* Character) const;

	// --- Changing (server) ---

	/** Adds Quantity of ItemClass: onto existing stacks first, then into empty slots, as weight allows. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	FVaesenItemAddResult TryAddItemOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1);

	/** Adds a copy of Item (its quantity, or Quantity if given); Item itself is left as it is. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	FVaesenItemAddResult TryAddItem(const UVaesenItem* Item, int32 Quantity = -1);

	/** Takes Quantity from this item's stack (all of it if Quantity is -1 or more than the stack). Returns how many. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItem(UVaesenItem* Item, int32 Quantity = -1);

	/** Takes Quantity of ItemClass, from any stacks. Returns how many were taken. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	int32 RemoveItemsOfClass(TSubclassOf<UVaesenItem> ItemClass, int32 Quantity = 1);

	/**
	 * Moves Amount (-1 = the whole stack) from a slot here to a slot in To (which may be this inventory). Into an
	 * empty slot: moves or splits. Onto the same item: merges as far as the stack allows. Onto another item: swaps
	 * (whole stacks only). ToIndex -1: wherever it fits. Returns whether anything changed.
	 */
	bool MoveItem(int32 FromIndex, UVaesenInventoryComponent* To, int32 ToIndex, int32 Amount = -1);

	/** Uses the item in a slot for Character (eats it, reads it...). Used-up consumables leave the stack. */
	bool UseItem(int32 SlotIndex, AVaesenPlayerCharacter* Character);

	/** Drops Amount (-1 = all) from a slot as a pickup in front of the owner (or of DroppedBy). */
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

	/** How many of Item more the weight limit allows (MAX_int32 = no limit). */
	int32 GetWeightRoomFor(const UVaesenItem* Item) const;

	/** A new stack in this inventory: a copy of Template with Quantity. */
	UVaesenItem* MakeStack(const UVaesenItem* Template, int32 Quantity);

	/** Puts Item (owned by this actor) in a slot, or empties it (null), keeping replication in step. */
	void SetSlot(int32 SlotIndex, UVaesenItem* Item);

	int32 FindEmptySlot() const;

	/** Adds up to Quantity copies of Template; returns how many. */
	int32 AddInternal(const UVaesenItem* Template, int32 Quantity, FText& OutError);

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
