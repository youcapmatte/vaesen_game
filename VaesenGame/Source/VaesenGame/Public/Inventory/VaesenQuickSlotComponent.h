#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaesenQuickSlotComponent.generated.h"

class AVaesenPlayerCharacter;
class UVaesenInventoryComponent;
class UVaesenItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenQuickSlotsChanged);

/**
 * The player's quick slots: shortcuts (keys 1-8, and the quick bar) to usable things in their inventory, like potions
 * in The Witcher 3. A slot remembers a kind of item, not an item: the items stay in the inventory (pouches
 * included), the slot shows how many there are, and using it uses one of them. When the last one is gone (used,
 * dropped, put away) the slot empties.
 *
 * What fits: anything usable that isn't worn (UVaesenItem::CanBeQuickSlotted): potions, food, remedies, letters.
 * How many: BaseSlots, plus what a belt or bag worn in the Bag slot adds (UVaesenContainerItem::ExtraQuickSlots),
 * up to MaxSlots.
 *
 * The server keeps the assignments (they replicate to the owning player); the player asks through the Request
 * functions.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaesenQuickSlotComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Quick slots everyone has. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quick Slots", meta = (ClampMin = "0", ClampMax = "8"))
	int32 BaseSlots = 4;

	/** Most quick slots there can be (keys 1-8). */
	static constexpr int32 MaxSlots = 8;

	/** The kind of item in each quick slot (null = empty); as many entries as there are slots. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	const TArray<TSubclassOf<UVaesenItem>>& GetSlots() const { return Slots; }

	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	TSubclassOf<UVaesenItem> GetSlot(int32 Index) const;

	/** How many of the slot's kind of item the player has (pouches included). */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	int32 GetSlotCount(int32 Index) const;

	/** One of the slot's kind of item in the player's inventory (the first found), or null. */
	UFUNCTION(BlueprintPure, Category = "Quick Slots")
	UVaesenItem* FindItemForSlot(int32 Index) const;

	// --- Requests from the player's machine (the server checks them) ---

	/** Puts this kind of item in a quick slot (if it can be quick-slotted). It leaves any other slot it was in. */
	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void RequestAssign(int32 Index, TSubclassOf<UVaesenItem> ItemClass);

	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void RequestClear(int32 Index);

	/** Uses one of the slot's items (key 1-8). */
	UFUNCTION(BlueprintCallable, Category = "Quick Slots")
	void RequestUse(int32 Index);

	/** The slots changed: assigned, cleared, more or fewer of them (on the server and the owning player). */
	UPROPERTY(BlueprintAssignable, Category = "Quick Slots")
	FVaesenQuickSlotsChanged OnQuickSlotsChanged;

protected:
	virtual void BeginPlay() override;

private:
	AVaesenPlayerCharacter* GetCharacter() const;
	UVaesenInventoryComponent* GetInventory() const;

	/** Server: BaseSlots plus what the worn bag adds; assignments past the end are dropped. */
	UFUNCTION()
	void UpdateSlotCount();

	/** Server: empties slots whose kind of item the player no longer has. */
	UFUNCTION()
	void ClearRunOutSlots();

	void Assign(int32 Index, TSubclassOf<UVaesenItem> ItemClass);

	UFUNCTION(Server, Reliable)
	void ServerAssign(int32 Index, TSubclassOf<UVaesenItem> ItemClass);

	UFUNCTION(Server, Reliable)
	void ServerUse(int32 Index);

	UFUNCTION()
	void OnRep_Slots();

	UPROPERTY(ReplicatedUsing = OnRep_Slots)
	TArray<TSubclassOf<UVaesenItem>> Slots;
};
