#include "Items/VaesenItem.h"

#include "GameFramework/Actor.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Net/UnrealNetwork.h"

UWorld* UVaesenItem::GetWorld() const
{
	// Not calling Super (even for the class default object, which has no owner) is what tells the Blueprint editor
	// this class provides a world, so item Blueprints get world nodes.
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}
	const AActor* Owner = GetOwningActor();
	return Owner ? Owner->GetWorld() : nullptr;
}

AActor* UVaesenItem::GetOwningActor() const
{
	return GetTypedOuter<AActor>();
}

UVaesenInventoryComponent* UVaesenItem::GetOwningInventory() const
{
	return OwningInventory.Get();
}

void UVaesenItem::SetOwningInventory(UVaesenInventoryComponent* Inventory)
{
	OwningInventory = Inventory;
}

UVaesenItem* UVaesenItem::DuplicateItem(UObject* NewOuter, int32 NewQuantity) const
{
	// A class default object only gives the values; it's not an object to copy flags from.
	const bool bFromDefaults = HasAnyFlags(RF_ClassDefaultObject);
	UVaesenItem* Item = NewObject<UVaesenItem>(NewOuter, GetClass(), NAME_None, RF_Transient,
		bFromDefaults ? nullptr : const_cast<UVaesenItem*>(this));
	// Don't inherit whoever listened to the original (e.g. the pickup it came from).
	Item->OnItemModified.Clear();
	Item->SetQuantity(NewQuantity);
	return Item;
}

void UVaesenItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenItem, Quantity);
}

void UVaesenItem::SetQuantity(int32 NewQuantity)
{
	const int32 Clamped = FMath::Clamp(NewQuantity, 1, GetMaxStack());
	if (Clamped != Quantity)
	{
		Quantity = Clamped;
		NotifyModified();
	}
}

bool UVaesenItem::CanUse_Implementation(const AVaesenPlayerCharacter* Character) const
{
	return Character && !UseActionText.IsEmpty();
}

void UVaesenItem::Use_Implementation(AVaesenPlayerCharacter* Character)
{
}

void UVaesenItem::OnRep_Quantity()
{
	NotifyModified();
}

void UVaesenItem::NotifyModified()
{
	OnItemModified.Broadcast();
	if (UVaesenInventoryComponent* Inventory = OwningInventory.Get())
	{
		Inventory->NotifyChanged();
	}
}

#if WITH_EDITOR
void UVaesenItem::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Not stackable: one per stack (MaxStackSize is greyed out in the details panel meanwhile).
	const FName Changed = PropertyChangedEvent.GetMemberPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UVaesenItem, bStackable))
	{
		// Not stackable: one per stack. Stackable again: the usual 10.
		MaxStackSize = bStackable ? FMath::Max(MaxStackSize, 10) : 1;
	}

	// Quest items never leave the inventory.
	if (Changed == GET_MEMBER_NAME_CHECKED(UVaesenItem, bQuestItem) && bQuestItem)
	{
		bCanBeDropped = false;
	}
}
#endif
