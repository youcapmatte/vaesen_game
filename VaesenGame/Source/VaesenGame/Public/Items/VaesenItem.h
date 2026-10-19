#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "VaesenItem.generated.h"

class AVaesenPlayerCharacter;
class UVaesenInventoryComponent;
class UStaticMesh;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenItemModified);

/**
 * An item: what it is (name, description, look) and how many of it there are. Each kind of item is a Blueprint
 * subclass (e.g. BP_Item_Lantern) with its values set in Class Defaults; that class is the item's template.
 * Actual items in the world (in a pickup, later an inventory) are instances of it, with their own Quantity.
 *
 * Kinds of item (subclass when an item behaves differently, use properties and ItemTags for variants):
 *   Equippable (slot, equip/unequip)  ->  Weapon, Armor (clothes too), Tool (lantern, spyglass...), Talisman,
 *                                         Container (satchel, backpack)
 *   Consumable (food, drink, medicine, poison), Ammo, Readable (letters, books, clues), Ingredient
 *   Pouch (a herb pouch, a sack: holds other items in its own slots)
 * Only consumables, ingredients and ammo stack by default (10 per stack).
 *
 * Instances are created with the actor that holds them as their outer (the pickup now, an inventory later) and
 * replicate as that actor's registered subobjects (see AVaesenPickup). Moving an item to another actor means a new
 * instance there (or re-registering it).
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class VAESENGAME_API UVaesenItem : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }

	/** The owning actor's world, so Blueprint items can use world nodes (timers, spawning, players...). */
	virtual UWorld* GetWorld() const override;

	/** The actor holding this item (its outer), e.g. a pickup. */
	UFUNCTION(BlueprintPure, Category = "Item")
	AActor* GetOwningActor() const;

	/** The inventory it's in, if any. */
	UFUNCTION(BlueprintPure, Category = "Item")
	UVaesenInventoryComponent* GetOwningInventory() const;

	/** Set by the inventory when it takes (or lets go of) the item. */
	void SetOwningInventory(UVaesenInventoryComponent* Inventory);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	/** What the item looks like lying in the world (on a pickup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> PickupMesh;

	/** Scale of PickupMesh when the item is dropped (placed pickups keep the scale they're given). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FVector PickupScale = FVector::OneVector;

	/** Shown in the inventory. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Thumbnail;

	/** Per item, in kilograms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", Units = "kg"))
	float Weight = 0.0f;

	/** What one is worth when bought or sold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 Value = 0;

	/** Traits shared across kinds of item, e.g. Item.Material.Iron (what a vaesen fears), Item.Light. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FGameplayTagContainer ItemTags;

	/** The verb for using it, e.g. "Eat", "Read", "Light". Empty = it can't be used on its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText UseActionText;

	/** Needed for an investigation or quest: can't be dropped or sold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bQuestItem = false;

	/** Off: it stays in the inventory (always off for quest items). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "!bQuestItem"))
	bool bCanBeDropped = true;

	/** Several fit in one stack (up to MaxStackSize). Off: one per stack, and MaxStackSize is locked at 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bStackable = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1", EditCondition = "bStackable"))
	int32 MaxStackSize = 1;

	/**
	 * Inventory slots one stack of it takes, however many are in the stack (a rifle might take 3, a ring 1).
	 * 0: it takes no room at all, so any number can be carried (letters, coins, keys).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0", ClampMax = "20"))
	int32 SlotSize = 1;

	/** Most that fit in one stack: MaxStackSize, or 1 when not stackable. */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetMaxStack() const { return bStackable ? FMath::Max(MaxStackSize, 1) : 1; }

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetQuantity() const { return Quantity; }

	/** Weight of the whole stack (and of anything inside it). */
	UFUNCTION(BlueprintPure, Category = "Item")
	virtual float GetStackWeight() const { return Weight * Quantity; }

	/** Items held inside this one (a pouch's contents); they replicate along with it. */
	virtual void GetNestedItems(TArray<UVaesenItem*>& OutItems) const {}

	/**
	 * A new instance like this one (its values, anything inside it copied too) with Quantity, owned by NewOuter (the
	 * actor that will hold it). This can be the class default object, for a fresh item of this kind.
	 */
	virtual UVaesenItem* DuplicateItem(UObject* NewOuter, int32 NewQuantity) const;

	/** Whether Character can use it now (has a use action by default). */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Item")
	bool CanUse(const AVaesenPlayerCharacter* Character) const;

	/** Uses it (eat, read, light...). Called on the server by the inventory; subclasses and Blueprints say what happens. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, BlueprintAuthorityOnly, Category = "Item")
	void Use(AVaesenPlayerCharacter* Character);

	/** Clamped to 1..GetMaxStack(). Server only (it replicates). */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetQuantity(int32 NewQuantity);

	/** The quantity changed (also on clients when it arrives). */
	UPROPERTY(BlueprintAssignable, Category = "Item")
	FVaesenItemModified OnItemModified;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Quantity, BlueprintReadOnly, Category = "Item")
	int32 Quantity = 1;

private:
	UFUNCTION()
	void OnRep_Quantity();

	/** Tells listeners (and the inventory) that it changed. */
	void NotifyModified();

	TWeakObjectPtr<UVaesenInventoryComponent> OwningInventory;
};
