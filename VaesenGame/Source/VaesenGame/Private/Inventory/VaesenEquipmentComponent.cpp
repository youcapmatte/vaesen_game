#include "Inventory/VaesenEquipmentComponent.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Core/VaesenSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/ActorChannel.h"
#include "Engine/SkeletalMesh.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenArmorItem.h"
#include "Items/VaesenEquippableItem.h"
#include "Items/VaesenPickup.h"
#include "Items/VaesenWeaponItem.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/VaesenWeaponActor.h"

UVaesenEquipmentComponent::UVaesenEquipmentComponent()
{
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
}

void UVaesenEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenEquipmentComponent, Equipped);
	DOREPLIFETIME(UVaesenEquipmentComponent, DrawState);
	DOREPLIFETIME(UVaesenEquipmentComponent, bPrefersOneHand);
}

bool UVaesenEquipmentComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	// For owners that don't use the registered subobject list (SetEquipped registers the items for those that do).
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	if (IsUsingRegisteredSubObjectList())
	{
		return bWroteSomething;
	}
	for (const FVaesenEquippedItem& Entry : Equipped)
	{
		if (Entry.Item)
		{
			bWroteSomething |= Channel->ReplicateSubobject(Entry.Item, *Bunch, *RepFlags);
		}
	}
	return bWroteSomething;
}

void UVaesenEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		for (const TSubclassOf<UVaesenEquippableItem>& ItemClass : StartingEquipment)
		{
			EquipNew(ItemClass);
		}
	}
	ApplyAppearance();
	ApplyDrawState(false);
	bDrawStateInitialized = true;
}

TArray<EVaesenEquipmentSlot> UVaesenEquipmentComponent::GetPlayerSlots()
{
	// The armor from head to feet, then the back, the hands and the sidearm, the talisman and the bag.
	return {
		EVaesenEquipmentSlot::Head, EVaesenEquipmentSlot::Chest, EVaesenEquipmentSlot::Hands,
		EVaesenEquipmentSlot::Legs, EVaesenEquipmentSlot::Feet,
		EVaesenEquipmentSlot::Back, EVaesenEquipmentSlot::MainHand, EVaesenEquipmentSlot::OffHand, EVaesenEquipmentSlot::Sidearm,
		EVaesenEquipmentSlot::Talisman, EVaesenEquipmentSlot::Bag,
	};
}

AVaesenBaseCharacter* UVaesenEquipmentComponent::GetCharacter() const
{
	return Cast<AVaesenBaseCharacter>(GetOwner());
}

UVaesenInventoryComponent* UVaesenEquipmentComponent::GetOwnInventory() const
{
	return GetOwner()->FindComponentByClass<UVaesenInventoryComponent>();
}

UVaesenEquippableItem* UVaesenEquipmentComponent::GetItemInSlot(EVaesenEquipmentSlot Slot) const
{
	const FVaesenEquippedItem* Entry = Equipped.FindByPredicate([Slot](const FVaesenEquippedItem& Each) { return Each.Slot == Slot; });
	return Entry ? Entry->Item.Get() : nullptr;
}

bool UVaesenEquipmentComponent::CanEquip(const UVaesenItem* Item) const
{
	const UVaesenEquippableItem* Equippable = Cast<UVaesenEquippableItem>(Item);
	if (!Equippable)
	{
		return false;
	}
	return Equippable->Slot != EVaesenEquipmentSlot::FullBody || bAllowFullBodyArmor;
}

bool UVaesenEquipmentComponent::CanEquipInSlot(const UVaesenItem* Item, EVaesenEquipmentSlot Slot) const
{
	return CanEquip(Item) && CastChecked<UVaesenEquippableItem>(Item)->Slot == Slot;
}

TArray<EVaesenEquipmentSlot> UVaesenEquipmentComponent::GetSlotsClearedBy(const UVaesenEquippableItem* Item) const
{
	TArray<EVaesenEquipmentSlot> Slots{ Item->Slot };

	// A two-handed weapon needs the off hand free; anything in the off hand pushes a two-handed weapon out.
	const UVaesenWeaponItem* Weapon = Cast<UVaesenWeaponItem>(Item);
	if (Item->Slot == EVaesenEquipmentSlot::MainHand && Weapon && Weapon->bTwoHanded)
	{
		Slots.Add(EVaesenEquipmentSlot::OffHand);
	}
	const UVaesenWeaponItem* MainHand = Cast<UVaesenWeaponItem>(GetItemInSlot(EVaesenEquipmentSlot::MainHand));
	if (Item->Slot == EVaesenEquipmentSlot::OffHand && MainHand && MainHand->bTwoHanded)
	{
		Slots.Add(EVaesenEquipmentSlot::MainHand);
	}
	return Slots;
}

bool UVaesenEquipmentComponent::EquipFromInventory(UVaesenInventoryComponent* From, UVaesenItem* Item)
{
	UVaesenEquippableItem* Equippable = Cast<UVaesenEquippableItem>(Item);
	if (!GetOwner()->HasAuthority() || !From || !Equippable || !CanEquip(Equippable) || From->FindSlotOf(Equippable) == INDEX_NONE)
	{
		return false;
	}

	// Whatever is in the way goes back into From: it must all fit, counting the room this item leaves.
	TArray<EVaesenEquipmentSlot> Cleared = GetSlotsClearedBy(Equippable);
	int32 SlotsNeeded = -Equippable->SlotSize;
	TArray<UVaesenEquippableItem*> Displaced;
	for (const EVaesenEquipmentSlot Slot : Cleared)
	{
		if (UVaesenEquippableItem* Worn = GetItemInSlot(Slot))
		{
			Displaced.Add(Worn);
			SlotsNeeded += Worn->SlotSize;
		}
	}
	if (!From->HasUnlimitedCapacity() && SlotsNeeded > From->GetFreeSlotCount())
	{
		return false;
	}

	UVaesenEquippableItem* Wearing = CastChecked<UVaesenEquippableItem>(Equippable->DuplicateItem(GetOwner(), 1));
	From->RemoveItem(Equippable, 1);
	for (const EVaesenEquipmentSlot Slot : Cleared)
	{
		if (UVaesenEquippableItem* Worn = GetItemInSlot(Slot))
		{
			From->TryAddItem(Worn);
			SetEquipped(Slot, nullptr);
		}
	}
	SetEquipped(Wearing->Slot, Wearing);
	return true;
}

bool UVaesenEquipmentComponent::UnequipToInventory(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	UVaesenEquippableItem* Worn = GetItemInSlot(Slot);
	To = To ? To : GetOwnInventory();
	if (!GetOwner()->HasAuthority() || !Worn || !To || !To->HasRoomFor(Worn))
	{
		return false;
	}

	if (To->TryAddItem(Worn).AmountGiven == 0)
	{
		return false;
	}
	SetEquipped(Slot, nullptr);
	return true;
}

bool UVaesenEquipmentComponent::EquipNew(TSubclassOf<UVaesenEquippableItem> ItemClass)
{
	const UVaesenEquippableItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenEquippableItem>() : nullptr;
	if (!GetOwner()->HasAuthority() || !Defaults || !CanEquip(Defaults))
	{
		return false;
	}

	for (const EVaesenEquipmentSlot Slot : GetSlotsClearedBy(Defaults))
	{
		SetEquipped(Slot, nullptr);
	}
	SetEquipped(Defaults->Slot, CastChecked<UVaesenEquippableItem>(Defaults->DuplicateItem(GetOwner(), 1)));
	return true;
}

void UVaesenEquipmentComponent::SetEquipped(EVaesenEquipmentSlot Slot, UVaesenEquippableItem* Item)
{
	const int32 Index = Equipped.IndexOfByPredicate([Slot](const FVaesenEquippedItem& Each) { return Each.Slot == Slot; });
	if (Index != INDEX_NONE)
	{
		if (Equipped[Index].Item == Item)
		{
			return;
		}
		if (Equipped[Index].Item)
		{
			// Off the body: in one hand again, next time.
			if (UVaesenWeaponItem* Leaving = Cast<UVaesenWeaponItem>(Equipped[Index].Item))
			{
				Leaving->SetHeldTwoHanded(false);
			}
			RemoveReplicatedSubObject(Equipped[Index].Item);
		}
		Equipped.RemoveAt(Index);
	}

	if (Item)
	{
		AddReplicatedSubObject(Item);
		FVaesenEquippedItem& Entry = Equipped.AddDefaulted_GetRef();
		Entry.Slot = Slot;
		Entry.Item = Item;
	}

	UpdateGrip();
	ApplyAppearance();

	// The drawn weapon went (taken off, dropped, thrown): nothing is in the hand any more.
	if (DrawState.bDrawn && !HasWeaponSet(DrawState.WeaponSlot))
	{
		DrawState.bDrawn = false;
		ApplyDrawState(false);
	}
	OnEquipmentChanged.Broadcast();
}

void UVaesenEquipmentComponent::OnRep_Equipped()
{
	ApplyAppearance();
	OnEquipmentChanged.Broadcast();
}

void UVaesenEquipmentComponent::ApplyAppearance()
{
	AVaesenBaseCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	// Attached meshes (weapons, tools, talismans, bags): off for what was taken off, on for what was put on.
	TArray<UVaesenEquippableItem*> Current;
	for (const FVaesenEquippedItem& Entry : Equipped)
	{
		if (Entry.Item)
		{
			Current.Add(Entry.Item);
		}
	}
	// Taking off or putting on a weapon plays its unequip / equip montage (not when joining or dressing an NPC).
	const auto PlayWeaponMontage = [this, Character](const UVaesenEquippableItem* Item, bool bEquip)
	{
		const UVaesenWeaponItem* Weapon = Cast<UVaesenWeaponItem>(Item);
		if (UAnimMontage* Montage = bAppearanceApplied && Weapon ? Weapon->GetMontage(bEquip ? &FVaesenWeaponMontages::Equip : &FVaesenWeaponMontages::Unequip) : nullptr)
		{
			Character->PlayAnimMontage(Montage);
		}
	};
	for (UVaesenEquippableItem* Item : AppliedItems)
	{
		if (Item && !Current.Contains(Item))
		{
			Item->Unequip();
			PlayWeaponMontage(Item, false);
		}
	}
	UVaesenEquippableItem* JustPutOn = nullptr;
	for (UVaesenEquippableItem* Item : Current)
	{
		if (!AppliedItems.Contains(Item))
		{
			Item->Equip(Character);
			PlayWeaponMontage(Item, true);
			JustPutOn = JustPutOn ? JustPutOn : Item;
		}
	}
	AppliedItems = Current;

	// Heard by this character's player only.
	if (bAppearanceApplied && JustPutOn && IsLocalPlayers())
	{
		if (USoundBase* Sound = JustPutOn->EquipSound ? JustPutOn->EquipSound.Get() : UVaesenSettings::Get()->DefaultEquipSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}
	bAppearanceApplied = true;
	RefreshWeaponAttachments();

	// The body: each part shows the character's own mesh (from its class defaults) unless armor replaces it, and its
	// worn-over layer shows armor worn over it. Full-body armor replaces the whole body (torso) and hides the other
	// parts and layers; a cloak stays on over it.
	const AVaesenBaseCharacter* Defaults = GetOwner()->GetClass()->GetDefaultObject<AVaesenBaseCharacter>();
	const UVaesenArmorItem* FullBody = Cast<UVaesenArmorItem>(GetItemInSlot(EVaesenEquipmentSlot::FullBody));
	const auto SetMesh = [](USkeletalMeshComponent* Component, USkeletalMesh* Mesh)
	{
		if (Component && Component->GetSkeletalMeshAsset() != Mesh)
		{
			Component->SetSkeletalMeshAsset(Mesh);
		}
	};

	for (const EVaesenEquipmentSlot Slot : { EVaesenEquipmentSlot::Head, EVaesenEquipmentSlot::Chest, EVaesenEquipmentSlot::Hands,
		EVaesenEquipmentSlot::Legs, EVaesenEquipmentSlot::Feet, EVaesenEquipmentSlot::Back })
	{
		const UVaesenArmorItem* Armor = Cast<UVaesenArmorItem>(GetItemInSlot(Slot));
		USkeletalMesh* ArmorMesh = Armor ? Armor->ArmorMesh.Get() : nullptr;
		const bool bWornOver = Armor && Armor->IsWornOver();

		// The back is a layer of its own: just the cloak.
		if (Slot == EVaesenEquipmentSlot::Back)
		{
			SetMesh(Character->GetBodyPartFor(Slot), ArmorMesh);
			continue;
		}

		const USkeletalMeshComponent* DefaultPart = Defaults ? Defaults->GetBodyPartFor(Slot) : nullptr;
		USkeletalMesh* PartMesh = DefaultPart ? DefaultPart->GetSkeletalMeshAsset() : nullptr;
		USkeletalMesh* OverMesh = nullptr;
		if (ArmorMesh)
		{
			(bWornOver ? OverMesh : PartMesh) = ArmorMesh;
		}
		if (FullBody)
		{
			PartMesh = Slot == EVaesenEquipmentSlot::Chest ? FullBody->ArmorMesh.Get() : nullptr;
			OverMesh = nullptr;
		}

		SetMesh(Character->GetBodyPartFor(Slot), PartMesh);
		SetMesh(Character->GetWornOverLayerFor(Slot), OverMesh);
	}
}

void UVaesenEquipmentComponent::RequestEquipFromInventory(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	if (GetOwner()->HasAuthority())
	{
		ServerEquipFromInventory_Implementation(Inventory, SlotIndex);
	}
	else
	{
		ServerEquipFromInventory(Inventory, SlotIndex);
	}
}

void UVaesenEquipmentComponent::RequestUnequip(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	if (GetOwner()->HasAuthority())
	{
		ServerUnequip_Implementation(Slot, To);
	}
	else
	{
		ServerUnequip(Slot, To);
	}
}

void UVaesenEquipmentComponent::ServerEquipFromInventory_Implementation(UVaesenInventoryComponent* Inventory, int32 SlotIndex)
{
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	UVaesenItem* Item = Inventory ? Inventory->GetItemAt(SlotIndex) : nullptr;
	if (!Item || !Inventory->CanBeAccessedBy(Player) || !EquipFromInventory(Inventory, Item))
	{
		ClientEquipRejected(Item ? Item->GetClass() : nullptr);
	}
}

void UVaesenEquipmentComponent::ServerUnequip_Implementation(EVaesenEquipmentSlot Slot, UVaesenInventoryComponent* To)
{
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	const UVaesenEquippableItem* Worn = GetItemInSlot(Slot);
	if ((To && !To->CanBeAccessedBy(Player)) || !UnequipToInventory(Slot, To))
	{
		ClientEquipRejected(Worn ? Worn->GetClass() : nullptr);
	}
}

void UVaesenEquipmentComponent::RequestDropEquipped(EVaesenEquipmentSlot Slot)
{
	if (IsLocalPlayers() && GetItemInSlot(Slot))
	{
		if (USoundBase* Sound = UVaesenSettings::Get()->DropSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySound2D(this, Sound);
		}
	}

	if (GetOwner()->HasAuthority())
	{
		ServerDropEquipped_Implementation(Slot);
	}
	else
	{
		ServerDropEquipped(Slot);
	}
}

void UVaesenEquipmentComponent::ServerDropEquipped_Implementation(EVaesenEquipmentSlot Slot)
{
	const UVaesenEquippableItem* Worn = GetItemInSlot(Slot);
	if (!DropEquipped(Slot))
	{
		ClientEquipRejected(Worn ? Worn->GetClass() : nullptr);
	}
}

bool UVaesenEquipmentComponent::DropEquipped(EVaesenEquipmentSlot Slot)
{
	UVaesenEquippableItem* Worn = GetItemInSlot(Slot);
	if (!GetOwner()->HasAuthority() || !Worn || !Worn->bCanBeDropped)
	{
		return false;
	}

	const UVaesenInventoryComponent* Inventory = GetOwnInventory();
	const TSubclassOf<AVaesenPickup> PickupClass = Inventory && Inventory->DropPickupClass ? Inventory->DropPickupClass : TSubclassOf<AVaesenPickup>(AVaesenPickup::StaticClass());
	if (!AVaesenPickup::SpawnDropped(GetWorld(), PickupClass, Worn, 1, GetOwner()))
	{
		return false;
	}
	SetEquipped(Slot, nullptr);
	return true;
}

void UVaesenEquipmentComponent::ClientEquipRejected_Implementation(TSubclassOf<UVaesenItem> ItemClass)
{
	PlayEquipRejectedSound(ItemClass);
}

void UVaesenEquipmentComponent::PlayEquipRejectedSound(TSubclassOf<UVaesenItem> ItemClass) const
{
	if (!IsLocalPlayers())
	{
		return;
	}
	const UVaesenEquippableItem* Defaults = ItemClass ? Cast<UVaesenEquippableItem>(ItemClass->GetDefaultObject()) : nullptr;
	USoundBase* Sound = Defaults && Defaults->EquipRejectedSound ? Defaults->EquipRejectedSound.Get() : UVaesenSettings::Get()->DefaultEquipRejectedSound.LoadSynchronous();
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

bool UVaesenEquipmentComponent::IsLocalPlayers() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

void UVaesenEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GrabTimer);
		World->GetTimerManager().ClearTimer(StepTimer);
	}
	Super::EndPlay(EndPlayReason);
}

// --- Weapons: drawn and sheathed ---

UVaesenWeaponItem* UVaesenEquipmentComponent::GetWeaponInSlot(EVaesenEquipmentSlot Slot) const
{
	return Cast<UVaesenWeaponItem>(GetItemInSlot(Slot));
}

bool UVaesenEquipmentComponent::IsInSet(EVaesenEquipmentSlot Slot, EVaesenEquipmentSlot SetSlot)
{
	return SetSlot == EVaesenEquipmentSlot::Sidearm
		? Slot == EVaesenEquipmentSlot::Sidearm
		: Slot == EVaesenEquipmentSlot::MainHand || Slot == EVaesenEquipmentSlot::OffHand;
}

bool UVaesenEquipmentComponent::HasWeaponSet(EVaesenEquipmentSlot WeaponSlot) const
{
	if (WeaponSlot == EVaesenEquipmentSlot::Sidearm)
	{
		return GetWeaponInSlot(EVaesenEquipmentSlot::Sidearm) != nullptr;
	}
	return GetWeaponInSlot(EVaesenEquipmentSlot::MainHand) || GetWeaponInSlot(EVaesenEquipmentSlot::OffHand);
}

EVaesenEquipmentSlot UVaesenEquipmentComponent::GetDefaultDrawSlot() const
{
	return HasWeaponSet(EVaesenEquipmentSlot::MainHand) ? EVaesenEquipmentSlot::MainHand : EVaesenEquipmentSlot::Sidearm;
}

bool UVaesenEquipmentComponent::IsInHand(EVaesenEquipmentSlot Slot) const
{
	return AppliedDrawState.bDrawn && IsInSet(Slot, AppliedDrawState.WeaponSlot);
}

UVaesenWeaponItem* UVaesenEquipmentComponent::GetDrawnWeapon() const
{
	if (!AppliedDrawState.bDrawn)
	{
		return nullptr;
	}
	if (AppliedDrawState.WeaponSlot == EVaesenEquipmentSlot::Sidearm)
	{
		return GetWeaponInSlot(EVaesenEquipmentSlot::Sidearm);
	}
	UVaesenWeaponItem* MainHand = GetWeaponInSlot(EVaesenEquipmentSlot::MainHand);
	return MainHand ? MainHand : GetWeaponInSlot(EVaesenEquipmentSlot::OffHand);
}

UVaesenMeleeWeaponItem* UVaesenEquipmentComponent::GetDrawnShield() const
{
	if (!IsInHand(EVaesenEquipmentSlot::OffHand))
	{
		return nullptr;
	}
	UVaesenMeleeWeaponItem* Shield = Cast<UVaesenMeleeWeaponItem>(GetWeaponInSlot(EVaesenEquipmentSlot::OffHand));
	return Shield && Shield->IsShield() ? Shield : nullptr;
}

void UVaesenEquipmentComponent::RequestToggleGrip()
{
	const UVaesenWeaponItem* MainHand = GetWeaponInSlot(EVaesenEquipmentSlot::MainHand);
	if (!MainHand || !MainHand->bCanBeTwoHanded || MainHand->bTwoHanded)
	{
		return;
	}
	const bool bOneHand = !bPrefersOneHand;
	if (!GetOwner()->HasAuthority())
	{
		// Seen at once here; the server agrees and tells everyone.
		bPrefersOneHand = bOneHand;
		UpdateGrip();
	}
	ServerSetPrefersOneHand(bOneHand);
}

void UVaesenEquipmentComponent::ServerSetPrefersOneHand_Implementation(bool bOneHand)
{
	bPrefersOneHand = bOneHand;
	UpdateGrip();
}

void UVaesenEquipmentComponent::UpdateGrip()
{
	UVaesenWeaponItem* MainHand = GetWeaponInSlot(EVaesenEquipmentSlot::MainHand);
	if (MainHand)
	{
		// Both hands when it can be, the off hand is free and the player hasn't chosen one.
		MainHand->SetHeldTwoHanded(!GetItemInSlot(EVaesenEquipmentSlot::OffHand) && !bPrefersOneHand);
	}
}

void UVaesenEquipmentComponent::RequestToggleWeapon()
{
	const bool bDraw = !DrawState.bDrawn;
	const EVaesenEquipmentSlot WeaponSlot = GetDefaultDrawSlot();
	if (bDraw && !HasWeaponSet(WeaponSlot))
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		SetWeaponDrawn(bDraw, WeaponSlot);
	}
	else
	{
		ServerSetWeaponDrawn(bDraw, WeaponSlot);
	}
}

void UVaesenEquipmentComponent::RequestSwapWeapon()
{
	// The other set from the one drawn (or that would be drawn by default).
	const EVaesenEquipmentSlot Current = DrawState.bDrawn ? DrawState.WeaponSlot : GetDefaultDrawSlot();
	const EVaesenEquipmentSlot Other = Current == EVaesenEquipmentSlot::Sidearm ? EVaesenEquipmentSlot::MainHand : EVaesenEquipmentSlot::Sidearm;
	if (!HasWeaponSet(Other))
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		SetWeaponDrawn(true, Other);
	}
	else
	{
		ServerSetWeaponDrawn(true, Other);
	}
}

void UVaesenEquipmentComponent::RequestSheathe()
{
	if (!DrawState.bDrawn)
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		SetWeaponDrawn(false);
	}
	else
	{
		ServerSetWeaponDrawn(false, EVaesenEquipmentSlot::MainHand);
	}
}

void UVaesenEquipmentComponent::ServerSetWeaponDrawn_Implementation(bool bDraw, EVaesenEquipmentSlot WeaponSlot)
{
	SetWeaponDrawn(bDraw, WeaponSlot);
}

bool UVaesenEquipmentComponent::SetWeaponDrawn(bool bDraw, EVaesenEquipmentSlot WeaponSlot)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	if (WeaponSlot != EVaesenEquipmentSlot::Sidearm)
	{
		WeaponSlot = EVaesenEquipmentSlot::MainHand;
	}
	if (bDraw && !HasWeaponSet(WeaponSlot))
	{
		return false;
	}

	FVaesenDrawState NewState;
	NewState.bDrawn = bDraw;
	NewState.WeaponSlot = WeaponSlot;
	if (NewState == DrawState)
	{
		return false;
	}
	DrawState = NewState;
	ApplyDrawState(true);
	return true;
}

UVaesenItem* UVaesenEquipmentComponent::TakeOutForThrow(EVaesenEquipmentSlot Slot, UObject* NewOuter)
{
	UVaesenEquippableItem* Item = GetItemInSlot(Slot);
	if (!GetOwner()->HasAuthority() || !Item || !Item->CanBeThrown())
	{
		return nullptr;
	}
	UVaesenItem* Copy = Item->DuplicateItem(NewOuter, 1);
	SetEquipped(Slot, nullptr);
	return Copy;
}

void UVaesenEquipmentComponent::OnRep_DrawState()
{
	// Joining (or the first update): straight to the right state, no montages.
	ApplyDrawState(bDrawStateInitialized && HasBegunPlay());
}

void UVaesenEquipmentComponent::ApplyDrawState(bool bAnimate)
{
	UWorld* World = GetWorld();
	if (!bAnimate || !World)
	{
		DrawSteps.Reset();
		bStepRunning = false;
		if (World)
		{
			World->GetTimerManager().ClearTimer(GrabTimer);
			World->GetTimerManager().ClearTimer(StepTimer);
		}
		const bool bChanged = AppliedDrawState != DrawState;
		AppliedDrawState = DrawState;
		RefreshWeaponAttachments();
		ApplyDrawnLook();
		if (bChanged)
		{
			OnDrawStateChanged.Broadcast();
		}
		return;
	}

	// Where the hand will be once the running step has grabbed; from there: sheathe what's drawn, draw what's wanted.
	FVaesenDrawState From = AppliedDrawState;
	if (bStepRunning && !bStepGrabbed)
	{
		From.bDrawn = CurrentStep.bDraw;
		From.WeaponSlot = CurrentStep.WeaponSlot;
	}
	DrawSteps.Reset();
	if (From != DrawState)
	{
		if (From.bDrawn)
		{
			DrawSteps.Add({ false, From.WeaponSlot });
		}
		if (DrawState.bDrawn)
		{
			DrawSteps.Add({ true, DrawState.WeaponSlot });
		}
	}
	if (!bStepRunning)
	{
		RunNextDrawStep();
	}
}

void UVaesenEquipmentComponent::RunNextDrawStep()
{
	AVaesenBaseCharacter* Character = GetCharacter();
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	if (DrawSteps.Num() == 0 || !Character)
	{
		bStepRunning = false;
		if (ASC)
		{
			ASC->SetLooseGameplayTagCount(VaesenTags::State_SwitchingWeapon, 0);
		}
		return;
	}

	CurrentStep = DrawSteps[0];
	DrawSteps.RemoveAt(0);
	bStepRunning = true;
	bStepGrabbed = false;
	if (ASC)
	{
		ASC->SetLooseGameplayTagCount(VaesenTags::State_SwitchingWeapon, 1);
		if (!CurrentStep.bDraw)
		{
			// Whatever the weapon was doing stops (reloading, aiming, blocking).
			const FGameplayTagContainer WeaponAbilities(VaesenTags::Ability_Weapon);
			ASC->CancelAbilities(&WeaponAbilities);
		}
	}

	// The weapon leading the motion: the main hand (or a lone shield), or the sidearm.
	const UVaesenWeaponItem* Weapon = CurrentStep.WeaponSlot == EVaesenEquipmentSlot::Sidearm
		? GetWeaponInSlot(EVaesenEquipmentSlot::Sidearm)
		: (GetWeaponInSlot(EVaesenEquipmentSlot::MainHand) ? GetWeaponInSlot(EVaesenEquipmentSlot::MainHand) : GetWeaponInSlot(EVaesenEquipmentSlot::OffHand));
	if (!Weapon)
	{
		GrabForCurrentStep();
		FinishCurrentStep();
		return;
	}

	const FVaesenSheathLocation* Location = UVaesenWeaponCategoryDefaults::Get()->FindSheathLocation(Weapon->SheathSocket);
	const float FallbackGrab = Location ? Location->GrabTime : 0.25f;
	const float FallbackDuration = Location ? FMath::Max(Location->Duration, FallbackGrab) : 0.5f;

	UAnimMontage* Montage = Weapon->GetDrawMontage(CurrentStep.bDraw);
	const float Length = Montage ? Character->PlayAnimMontage(Montage) : 0.0f;

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	if (Length > 0.0f && UVaesenCombatLibrary::MontageHasWeaponHandoffNotify(Montage))
	{
		// The notify swaps it at the right frame; this only catches a montage cut short.
		Timers.SetTimer(GrabTimer, this, &UVaesenEquipmentComponent::GrabForCurrentStep, Length * 0.95f, false);
		Timers.SetTimer(StepTimer, this, &UVaesenEquipmentComponent::FinishCurrentStep, Length, false);
	}
	else
	{
		const float Duration = Length > 0.0f ? Length : FallbackDuration;
		Timers.SetTimer(GrabTimer, this, &UVaesenEquipmentComponent::GrabForCurrentStep, FMath::Clamp(FallbackGrab, 0.01f, Duration), false);
		Timers.SetTimer(StepTimer, this, &UVaesenEquipmentComponent::FinishCurrentStep, Duration, false);
	}
}

void UVaesenEquipmentComponent::HandleWeaponGrabNotify()
{
	if (bStepRunning && !bStepGrabbed)
	{
		GrabForCurrentStep();
	}
}

void UVaesenEquipmentComponent::GrabForCurrentStep()
{
	if (!bStepRunning || bStepGrabbed)
	{
		return;
	}
	bStepGrabbed = true;
	GetWorld()->GetTimerManager().ClearTimer(GrabTimer);

	AppliedDrawState.bDrawn = CurrentStep.bDraw;
	AppliedDrawState.WeaponSlot = CurrentStep.WeaponSlot;
	RefreshWeaponAttachments();
	ApplyDrawnLook();

	// Every machine runs this, so the sound plays locally on each.
	const UVaesenWeaponItem* Weapon = CurrentStep.WeaponSlot == EVaesenEquipmentSlot::Sidearm
		? GetWeaponInSlot(EVaesenEquipmentSlot::Sidearm) : GetWeaponInSlot(EVaesenEquipmentSlot::MainHand);
	if (AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr)
	{
		Actor->PlayWeaponSound(CurrentStep.bDraw ? EVaesenWeaponSound::Draw : EVaesenWeaponSound::Sheath);
	}
	OnDrawStateChanged.Broadcast();
}

void UVaesenEquipmentComponent::FinishCurrentStep()
{
	GrabForCurrentStep();
	GetWorld()->GetTimerManager().ClearTimer(StepTimer);
	bStepRunning = false;
	RunNextDrawStep();
}

void UVaesenEquipmentComponent::RefreshWeaponAttachments()
{
	for (const FVaesenEquippedItem& Entry : Equipped)
	{
		const UVaesenWeaponItem* Weapon = Cast<UVaesenWeaponItem>(Entry.Item);
		if (AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr)
		{
			Actor->AttachToOwner(IsInHand(Entry.Slot));
		}
	}
}

void UVaesenEquipmentComponent::ApplyDrawnLook()
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		ASC->SetLooseGameplayTagCount(VaesenTags::State_WeaponDrawn, AppliedDrawState.bDrawn ? 1 : 0);
	}

	// The drawn weapon's anim layer, or the unarmed one; sheathed means normal unarmed locomotion.
	AVaesenBaseCharacter* Character = GetCharacter();
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (!Mesh)
	{
		return;
	}
	const UVaesenWeaponItem* Weapon = GetDrawnWeapon();
	const TSubclassOf<UAnimInstance> Layer = Weapon ? Weapon->GetAnimLayer()
		: UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).AnimLayer;
	if (Layer == LinkedAnimLayer)
	{
		return;
	}

	// An anim Blueprint with a linked anim graph tagged WeaponLocomotion (ABP_VaesenCharacter) swaps that graph: the
	// weapon's, or the unarmed one. Otherwise, Lyra-style anim layers are linked onto the mesh.
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (AnimInstance && AnimInstance->GetLinkedAnimGraphInstanceByTag(WeaponLocomotionTag))
	{
		const TSubclassOf<UAnimInstance> Unarmed = UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).AnimLayer;
		if (const TSubclassOf<UAnimInstance> Graph = Layer ? Layer : Unarmed)
		{
			AnimInstance->LinkAnimGraphByTag(WeaponLocomotionTag, Graph);
		}
		LinkedAnimLayer = Layer;
		return;
	}
	if (LinkedAnimLayer)
	{
		Mesh->UnlinkAnimClassLayers(LinkedAnimLayer);
	}
	if (Layer)
	{
		Mesh->LinkAnimClassLayers(Layer);
	}
	LinkedAnimLayer = Layer;
}

const FName UVaesenEquipmentComponent::WeaponLocomotionTag(TEXT("WeaponLocomotion"));

AVaesenWeaponActor* UVaesenEquipmentComponent::GetSwitchingWeaponActor() const
{
	if (!bStepRunning)
	{
		return nullptr;
	}
	const UVaesenWeaponItem* Weapon = CurrentStep.WeaponSlot == EVaesenEquipmentSlot::Sidearm
		? GetWeaponInSlot(EVaesenEquipmentSlot::Sidearm)
		: (GetWeaponInSlot(EVaesenEquipmentSlot::MainHand) ? GetWeaponInSlot(EVaesenEquipmentSlot::MainHand) : GetWeaponInSlot(EVaesenEquipmentSlot::OffHand));
	return Weapon ? Weapon->GetWeaponActor() : nullptr;
}
