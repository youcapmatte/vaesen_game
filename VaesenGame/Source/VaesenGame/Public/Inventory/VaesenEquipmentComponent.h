#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenEquipmentComponent.generated.h"

class AVaesenBaseCharacter;
class AVaesenPlayerCharacter;
class USkeletalMesh;
class USoundBase;
class UVaesenEquippableItem;
class UVaesenInventoryComponent;
class UVaesenItem;

/** One worn or held item and the slot it's in. */
USTRUCT(BlueprintType)
struct FVaesenEquippedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EVaesenEquipmentSlot Slot = EVaesenEquipmentSlot::Head;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquippableItem> Item;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenEquipmentChanged);

/**
 * What a character (player or NPC) wears and holds, one item per slot. An item only ever goes in its own slot
 * (UVaesenEquippableItem::Slot): boots in Feet, a coat in Chest. Full-body armor is for NPCs only
 * (bAllowFullBodyArmor is off on players). Two-handed weapons take the off hand too.
 *
 * The look follows on every machine: armor replaces its body part's mesh (Chest is the torso with the arms), a cloak
 * shows on the back, full-body armor replaces the whole body, and weapons and tools attach their meshes.
 *
 * Changes happen on the server: equipping takes the item out of an inventory, and unequipping puts it back (if
 * there's room). Players ask through the Request functions on their own equipment.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaesenEquipmentComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	/** Off for players: they dress piece by piece. NPCs may wear one suit for the whole body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	bool bAllowFullBodyArmor = true;

	/** Put on (on the server) when play starts, e.g. an NPC's outfit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TArray<TSubclassOf<UVaesenEquippableItem>> StartingEquipment;

	/**
	 * Plays the sound for Item (of this kind) not going on: its own rejected sound, or the default from Project
	 * Settings > Vaesen. Only on this character's player's machine (e.g. a slot refusing a drop).
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void PlayEquipRejectedSound(TSubclassOf<UVaesenItem> ItemClass) const;

	/** The slots shown in the player's equipment panel, in order. */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	static TArray<EVaesenEquipmentSlot> GetPlayerSlots();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UVaesenEquippableItem* GetItemInSlot(EVaesenEquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	const TArray<FVaesenEquippedItem>& GetEquippedItems() const { return Equipped; }

	/** Whether this character may wear Item at all (it's equippable, and full-body armor only if allowed). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanEquip(const UVaesenItem* Item) const;

	/** Whether Item goes in Slot here (its own slot, and allowed). */
	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool CanEquipInSlot(const UVaesenItem* Item, EVaesenEquipmentSlot Slot) const;

	// --- Server ---

	/** Takes Item out of From and puts it on; what was in the way goes into From (only if it all fits). */
	bool EquipFromInventory(UVaesenInventoryComponent* From, UVaesenItem* Item);

	/** Takes off what's in Slot and puts it into To (the character's own inventory if null), if there's room. */
	bool UnequipToInventory(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To = nullptr);

	/** Puts on a new item of this kind (from nowhere), replacing what's in its slot. For NPC outfits and setup. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Equipment")
	bool EquipNew(TSubclassOf<UVaesenEquippableItem> ItemClass);

	// --- Requests from the player's machine (the server checks them) ---

	/** Equips the item in slot SlotIndex of Inventory (the player's own, or a container they have open). */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RequestEquipFromInventory(UVaesenInventoryComponent* Inventory, int32 SlotIndex);

	/** Takes off what's in Slot into To (null = the player's own inventory). */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RequestUnequip(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To = nullptr);

	/** Takes off what's in Slot and drops it on the ground. */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void RequestDropEquipped(EVaesenEquipmentSlot Slot);

	/** Server: takes off what's in Slot and drops it on the ground in front of the character. */
	bool DropEquipped(EVaesenEquipmentSlot Slot);

	/** Something was put on or taken off (on the server and every client). */
	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FVaesenEquipmentChanged OnEquipmentChanged;

protected:
	virtual void BeginPlay() override;

private:
	AVaesenBaseCharacter* GetCharacter() const;
	UVaesenInventoryComponent* GetOwnInventory() const;

	/** The slots Item would empty when put on: its own, and the other hand for two-handed weapons. */
	TArray<EVaesenEquipmentSlot> GetSlotsClearedBy(const UVaesenEquippableItem* Item) const;

	/** Puts Item (owned by this actor) in Slot, or empties it (null), keeping replication and the look in step. */
	void SetEquipped(EVaesenEquipmentSlot Slot, UVaesenEquippableItem* Item);

	/** Makes the character look like what's equipped: body part meshes, cloak, full-body armor, attached items. */
	void ApplyAppearance();

	UFUNCTION()
	void OnRep_Equipped();

	UFUNCTION(Server, Reliable)
	void ServerEquipFromInventory(UVaesenInventoryComponent* Inventory, int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerUnequip(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To);

	UFUNCTION(Server, Reliable)
	void ServerDropEquipped(EVaesenEquipmentSlot Slot);

	/** The server refused to put on (or take off) an item of this kind. */
	UFUNCTION(Client, Unreliable)
	void ClientEquipRejected(TSubclassOf<UVaesenItem> ItemClass);

	/** Whether this is the character of the player on this machine. */
	bool IsLocalPlayers() const;

	/** The first look is applied silently (joining, NPC outfits); after that, putting things on is heard. */
	bool bAppearanceApplied = false;

	UPROPERTY(ReplicatedUsing = OnRep_Equipped)
	TArray<FVaesenEquippedItem> Equipped;

	/** The items whose look is on the character now (so the ones taken off can be removed). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaesenEquippableItem>> AppliedItems;
};
