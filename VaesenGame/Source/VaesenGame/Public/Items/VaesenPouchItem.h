#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "VaesenPouchItem.generated.h"

/**
 * A pouch, sack or case that holds other items in slots of its own: a herb pouch, a sack, a cartridge box. It takes
 * one inventory slot; while it has room, matching items the player gets go into it instead of taking more slots.
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
	virtual bool CanUse_Implementation(const AVaesenPlayerCharacter* Character) const override;
	virtual void Use_Implementation(AVaesenPlayerCharacter* Character) override;

	/** How many stacks fit inside (each up to its item's stack size). */
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

	/** One entry per pouch slot; null = empty. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	const TArray<UVaesenItem*>& GetContents() const { return ObjectPtrDecay(Contents); }

	UFUNCTION(BlueprintPure, Category = "Pouch")
	int32 GetUsedPouchSlots() const;

	/** Total number of items inside, across stacks. */
	UFUNCTION(BlueprintPure, Category = "Pouch")
	int32 GetContentCount() const;

	/** Server, for the inventory: makes sure there are PouchSlots entries. */
	void EnsurePouchSlots();

	/** Server, for the inventory: puts Item in a pouch slot (or empties it). The inventory handles replication. */
	void SetPouchSlot(int32 SlotIndex, UVaesenItem* Item);

private:
	UFUNCTION()
	void OnRep_Contents();

	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<TObjectPtr<UVaesenItem>> Contents;
};
