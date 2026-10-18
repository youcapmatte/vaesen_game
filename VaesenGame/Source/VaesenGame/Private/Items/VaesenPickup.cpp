#include "Items/VaesenPickup.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Items/VaesenItem.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenPickup, Log, All);

#define LOCTEXT_NAMESPACE "VaesenPickup"

AVaesenPickup::AVaesenPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;

	// Blocks Visibility, so the interaction trace finds it.
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	SetRootComponent(PickupMesh);

	InteractionComponent = CreateDefaultSubobject<UVaesenInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetupAttachment(PickupMesh);
	InteractionComponent->SetRelativeLocation(FVector(0.0, 0.0, 50.0));
	InteractionComponent->ActionText = LOCTEXT("PickUp", "Pick up");
	InteractionComponent->InteractionTime = 0.0f;
}

void AVaesenPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVaesenPickup, Item);
}

void AVaesenPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateFromItem();
}

void AVaesenPickup::BeginPlay()
{
	Super::BeginPlay();

	InteractionComponent->OnInteract.AddDynamic(this, &AVaesenPickup::OnTakePickup);

	if (HasAuthority() && !Item && ItemTemplate)
	{
		InitializePickup(ItemTemplate, Quantity);
	}
}

void AVaesenPickup::InitializePickup(TSubclassOf<UVaesenItem> Template, int32 InQuantity)
{
	if (!HasAuthority() || !Template)
	{
		return;
	}

	if (Item)
	{
		Item->OnItemModified.RemoveDynamic(this, &AVaesenPickup::UpdateFromItem);
		RemoveReplicatedSubObject(Item);
	}

	ItemTemplate = Template;
	Item = NewObject<UVaesenItem>(this, Template);
	Item->SetQuantity(InQuantity);
	Quantity = Item->GetQuantity();
	AddReplicatedSubObject(Item);
	Item->OnItemModified.AddDynamic(this, &AVaesenPickup::UpdateFromItem);
	UpdateFromItem();
}

void AVaesenPickup::OnRep_Item()
{
	if (Item)
	{
		Item->OnItemModified.AddUniqueDynamic(this, &AVaesenPickup::UpdateFromItem);
	}
	UpdateFromItem();
}

void AVaesenPickup::UpdateFromItem()
{
	const UVaesenItem* Source = Item ? Item.Get() : (ItemTemplate ? ItemTemplate->GetDefaultObject<UVaesenItem>() : nullptr);
	if (!Source)
	{
		PickupMesh->SetStaticMesh(nullptr);
		InteractionComponent->SetInteractableName(FText::GetEmpty());
		InteractionComponent->SetDescription(FText::GetEmpty());
		InteractionComponent->SetQuantity(0);
		return;
	}

	const int32 Count = Item ? Item->GetQuantity() : FMath::Clamp(Quantity, 1, Source->GetMaxStack());
	PickupMesh->SetStaticMesh(Source->PickupMesh);
	InteractionComponent->SetInteractableName(Source->ItemName);
	InteractionComponent->SetDescription(Source->Description);
	// "x1" says nothing, so only stacks show their count.
	InteractionComponent->SetQuantity(Count > 1 ? Count : 0);
}

void AVaesenPickup::OnTakePickup(AVaesenPlayerCharacter* Taker)
{
	if (!HasAuthority() || !Item)
	{
		return;
	}

	// TODO: put it in Taker's inventory (and only remove what fit) once there is one.
	UE_LOG(LogVaesenPickup, Display, TEXT("%s picked up %d x %s."), Taker ? *Taker->GetName() : TEXT("Someone"),
		Item->GetQuantity(), *Item->ItemName.ToString());
	Destroy();
}

#if WITH_EDITOR
void AVaesenPickup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Choosing the item sets the mesh (and card) right away; the quantity stays within the item's stack size.
	const FName Changed = PropertyChangedEvent.GetMemberPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(AVaesenPickup, ItemTemplate) || Changed == GET_MEMBER_NAME_CHECKED(AVaesenPickup, Quantity))
	{
		if (ItemTemplate)
		{
			Quantity = FMath::Clamp(Quantity, 1, ItemTemplate->GetDefaultObject<UVaesenItem>()->GetMaxStack());
		}
		UpdateFromItem();
	}
}
#endif

#undef LOCTEXT_NAMESPACE
