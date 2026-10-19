#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "VaesenPouchItem.generated.h"

/**
 * A pouch, sack or case that holds other items in slots of its own: a herb pouch, a sack, a cartridge box. It takes
 * its own SlotSize in the inventory; while it has room, matching items the player gets go into it instead of taking
 * more slots. Inside, stacks take their items' slot sizes too, up to PouchSlots (items with slot size 0 always fit).
 * Dropping an item on it in the inventory puts it inside; right-click ("Empty") tips the contents out into the
 * inventory. Its weight is its own plus its contents'.
 *
 * What it accepts: items with any of AcceptedItemTags or of any AcceptedClasses; with both empty, anything (a plain
 * sack). Never other pouches. The contents are changed by the inventory it's in (on the server), which also
 * replicates them.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenPouchItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenPouchItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float GetStackWeight() const override;
	virtual void GetNestedItems(TArray<UVaesenItem*>& OutItems) const override;
	virtual UVaesenItem* DuplicateItem(UObject* NewOuter, int32 NewQuantity) const override;
	virtual bool CanBeQuickSlotted() const override { return false; }
	virtual bool CanUse_Implementation(const AVaesenPlayerCharacter* Character) const override;
	virtual void Use_Implementation(AVaesenPlayerCharacter* Character) override;

	/** How many slots there are inside; each stack in it takes its item's slot size (and holds up to its stack size). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pouch", meta = (ClampMin = "1", ClampMax = "50"))
	int32 PouchSlots = 4;

	/** Takes items with any of these tags, e.g. Item.Herb. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pouch")
	FGameplayTagContainer AcceptedItemTags;

	/** Takes items of these kinds (and their subclasses). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pouch")
	TArray<TSubclassOf<UVaesenItem>> AcceptedClasses;

	/** Whether this kind of item can go in. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	bool Accepts(const UVaesenItem* Item) const;

	/** The stacks inside. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	const TArray<UVaesenItem*>& GetContents() const { return ObjectPtrDecay(Contents); }

	/** Slots taken inside: the stacks' slot sizes added up. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	int32 GetUsedPouchSlots() const;

	/** Whether a new stack of Item fits inside (and the pouch takes it). */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	bool HasRoomFor(const UVaesenItem* Item) const;

	/** Total number of items inside, across stacks. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	int32 GetContentCount() const;

	/** Server, for the inventory: adds a stack inside, or takes one out. The inventory handles replication. */
	void AddContent(UVaesenItem* Item);
	bool RemoveContent(UVaesenItem* Item);

private:
	UFUNCTION()
	void OnRep_Contents();

	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<TObjectPtr<UVaesenItem>> Contents;
};
