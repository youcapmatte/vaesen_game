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
class UVaesenMeleeWeaponItem;
class UVaesenWeaponItem;
class UAnimInstance;
class AVaesenWeaponActor;

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

/** Which carried weapon is in the hand: the main-hand set (main hand and an off-hand shield) or the sidearm. */
USTRUCT(BlueprintType)
struct FVaesenDrawState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool bDrawn = false;

	/** MainHand (with the off hand) or Sidearm. */
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	EVaesenEquipmentSlot WeaponSlot = EVaesenEquipmentSlot::MainHand;

	bool operator==(const FVaesenDrawState& Other) const { return bDrawn == Other.bDrawn && (!bDrawn || WeaponSlot == Other.WeaponSlot); }
	bool operator!=(const FVaesenDrawState& Other) const { return !(*this == Other); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenEquipmentChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenDrawStateChanged);

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

	// --- Weapons: drawn and sheathed ---
	//
	// Every weapon worn (main hand, off-hand shield, sidearm) hangs in its sheath socket. One set at a time is drawn:
	// the main-hand set (main hand + an off-hand shield) or the sidearm. The server decides; every machine plays the
	// draw or sheath montage of the weapon's sheath location and moves the weapon to the hand (or back) at the grab
	// moment: the montage's AN_WeaponGrab / AN_WeaponRelease notify, or the location's GrabTime without one.

	/** A weapon is in the hand now (the grab moment has passed). */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	bool IsWeaponDrawn() const { return AppliedDrawState.bDrawn; }

	/** Drawing or sheathing right now. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	bool IsSwitchingWeapon() const { return DrawSteps.Num() > 0 || bStepRunning; }

	/** The weapon in the hand (main hand or sidearm; an off-hand shield on its own counts). Null when sheathed or unarmed. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	UVaesenWeaponItem* GetDrawnWeapon() const;

	/** The shield in the off hand while the main-hand set is drawn. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	UVaesenMeleeWeaponItem* GetDrawnShield() const;

	/** The weapon worn in Slot (main hand, off hand, sidearm), if any. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	UVaesenWeaponItem* GetWeaponInSlot(EVaesenEquipmentSlot Slot) const;

	/** Whether the weapon in this slot is in the hand now (as opposed to in its sheath). */
	bool IsInHand(EVaesenEquipmentSlot Slot) const;

	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	const FVaesenDrawState& GetDrawState() const { return AppliedDrawState; }

	/** H: draw the main-hand set (or the sidearm if that's all there is), or sheathe what's drawn. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Weapons")
	void RequestToggleWeapon();

	/** Q: draw the other carried weapon (sidearm <-> main hand). */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Weapons")
	void RequestSwapWeapon();

	/** Sheathes whatever is drawn (opening the inventory, interacting). */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Weapons")
	void RequestSheathe();

	/** Server: draws the set in WeaponSlot (MainHand or Sidearm), or sheathes. Returns whether anything changes. */
	bool SetWeaponDrawn(bool bDraw, EVaesenEquipmentSlot WeaponSlot = EVaesenEquipmentSlot::MainHand);

	/** Server: takes the item out of Slot for throwing (no inventory involved); returns a copy owned by NewOuter. */
	UVaesenItem* TakeOutForThrow(EVaesenEquipmentSlot Slot, UObject* NewOuter);

	/** The weapon being drawn or sheathed right now (the hand reaches for it), or null. */
	AVaesenWeaponActor* GetSwitchingWeaponActor() const;

	/** The anim graph tag the weapon locomotion is linked into (UAnimInstance::LinkAnimGraphByTag). */
	static const FName WeaponLocomotionTag;

	/** The draw or sheath montage reached its grab frame (AN_WeaponGrab / AN_WeaponRelease), on every machine. */
	void HandleWeaponGrabNotify();

	/** Puts every weapon actor where it belongs now (hand or sheath). Called when one arrives on a client too. */
	void RefreshWeaponAttachments();

	/** The drawn weapon's stance again (its grip changed). */
	void RefreshDrawnLook() { ApplyDrawnLook(); }

	/**
	 * V: a weapon that can be held in both hands (a sword, an axe) switches between both hands and one. It starts in its
	 * natural grip (both hands if its category has two-handed animations, else one); a shield in the off hand always means one.
	 */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Weapons")
	void RequestToggleGrip();

	/** The player switched (V) from the weapons' natural grip: one hand where it would be both, or both where it would be one. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Weapons")
	bool HasSwitchedGrip() const { return bSwitchedGrip; }

	/** A weapon was drawn or sheathed (at the grab moment, on every machine). */
	UPROPERTY(BlueprintAssignable, Category = "Equipment|Weapons")
	FVaesenDrawStateChanged OnDrawStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// --- Drawing and sheathing ---

	/** The set to draw by default: the main-hand set if there is one, else the sidearm. */
	EVaesenEquipmentSlot GetDefaultDrawSlot() const;

	/** Whether there's a weapon to draw in this set. */
	bool HasWeaponSet(EVaesenEquipmentSlot WeaponSlot) const;

	/** The slots a drawn set holds: MainHand (and OffHand), or Sidearm. */
	static bool IsInSet(EVaesenEquipmentSlot Slot, EVaesenEquipmentSlot SetSlot);

	UFUNCTION(Server, Reliable)
	void ServerSetWeaponDrawn(bool bDraw, EVaesenEquipmentSlot WeaponSlot);

	UFUNCTION(Server, Reliable)
	void ServerSetSwitchedGrip(bool bSwitched);

	/** Holds the main-hand weapon in both hands or one, as the off hand and the player's choice say. */
	void UpdateGrip();

	UPROPERTY(Replicated)
	bool bSwitchedGrip = false;

	UFUNCTION()
	void OnRep_DrawState();

	/** Works toward DrawState from AppliedDrawState: sheathe what's drawn, draw what's wanted (bAnimate = with montages). */
	void ApplyDrawState(bool bAnimate);

	/** Starts the next queued draw or sheath. */
	void RunNextDrawStep();

	/** The grab moment of the running step: the weapon changes hands. */
	void GrabForCurrentStep();

	/** The running step is over. */
	void FinishCurrentStep();

	/** Sets the drawn tag and the weapon's anim layer to match AppliedDrawState. */
	void ApplyDrawnLook();

	struct FDrawStep
	{
		bool bDraw = false;
		EVaesenEquipmentSlot WeaponSlot = EVaesenEquipmentSlot::MainHand;
	};

	/** What the server wants. */
	UPROPERTY(ReplicatedUsing = OnRep_DrawState)
	FVaesenDrawState DrawState;

	/** What's in the hand on this machine now. */
	FVaesenDrawState AppliedDrawState;

	TArray<FDrawStep> DrawSteps;
	FDrawStep CurrentStep;
	bool bStepRunning = false;
	bool bStepGrabbed = false;
	bool bDrawStateInitialized = false;
	FTimerHandle GrabTimer;
	FTimerHandle StepTimer;

	/** The anim layer linked now (to unlink it again). */
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedAnimLayer;
};
