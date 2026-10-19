#include "Items/VaesenPickup.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "TimerManager.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenItem.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenPickup, Log, All);

#define LOCTEXT_NAMESPACE "VaesenPickup"

AVaesenPickup::AVaesenPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;

	// Blocks Visibility, so the interaction trace finds it, but not characters or cameras: small things on the floor
	// shouldn't trip anyone up or push the camera around.
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	PickupMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	PickupMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	PickupMesh->SetLinearDamping(0.2f);
	PickupMesh->SetAngularDamping(0.8f);
	SetRootComponent(PickupMesh);

	InteractionComponent = CreateDefaultSubobject<UVaesenInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetupAttachment(PickupMesh);
	InteractionComponent->SetRelativeLocation(FVector(0.0, 0.0, 50.0));
	InteractionComponent->ActionText = LOCTEXT("PickUp", "Pick up");
	InteractionComponent->InteractionTime = 0.0f;
	// Wherever it ends up lying, the card goes above it.
	InteractionComponent->bPlaceCardAboveOwner = true;
}

AVaesenPickup* AVaesenPickup::SpawnDropped(UWorld* World, TSubclassOf<AVaesenPickup> PickupClass, const UVaesenItem* SourceItem, int32 InQuantity, const AActor* DroppedBy)
{
	if (!World || !PickupClass || !SourceItem || !DroppedBy)
	{
		return nullptr;
	}

	// From hip height, a little in front; pulled back if that's inside a wall.
	const FVector Forward = DroppedBy->GetActorForwardVector();
	const FVector Origin = DroppedBy->GetActorLocation() + FVector(0.0, 0.0, 20.0);
	FVector Start = Origin + Forward * 60.0;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenDropItem), false, DroppedBy);
	if (World->SweepSingleByChannel(Hit, Origin, Start, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(15.0f), Params))
	{
		Start = Hit.Location - Forward * 5.0;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Transform(FRotator(0.0, DroppedBy->GetActorRotation().Yaw, 0.0), Start, SourceItem->PickupScale);
	AVaesenPickup* Pickup = World->SpawnActor<AVaesenPickup>(PickupClass, Transform, SpawnParameters);
	if (!Pickup)
	{
		return nullptr;
	}
	Pickup->InitializePickupFromItem(SourceItem, InQuantity);

	// A gentle toss forward and up.
	Pickup->Fall(Forward * 120.0 + FVector(0.0, 0.0, 120.0));
	return Pickup;
}

void AVaesenPickup::Fall(const FVector& Velocity)
{
	if (!HasAuthority())
	{
		return;
	}

	// Physics needs collision on the mesh; without it, just put it on whatever is below.
	const UStaticMesh* Mesh = PickupMesh->GetStaticMesh();
	if (!Mesh || !Mesh->GetBodySetup())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenPickupGround), false, this);
		const FVector Location = GetActorLocation();
		if (GetWorld()->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.0, 0.0, 500.0), ECC_Visibility, Params))
		{
			SetActorLocation(Hit.ImpactPoint);
		}
		return;
	}

	SetReplicateMovement(true);
	PickupMesh->SetMobility(EComponentMobility::Movable);
	PickupMesh->SetSimulatePhysics(true);
	PickupMesh->SetPhysicsLinearVelocity(Velocity);
	PickupMesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-180.0, 180.0), FMath::FRandRange(-180.0, 180.0), FMath::FRandRange(-90.0, 90.0)));
	PickupMesh->OnComponentSleep.AddUniqueDynamic(this, &AVaesenPickup::OnSettled);
	GetWorldTimerManager().SetTimer(FallTimer, this, &AVaesenPickup::StopFalling, MaxFallSeconds, false);
}

void AVaesenPickup::OnSettled(UPrimitiveComponent* SleepingComponent, FName BoneName)
{
	StopFalling();
}

void AVaesenPickup::StopFalling()
{
	GetWorldTimerManager().ClearTimer(FallTimer);
	PickupMesh->OnComponentSleep.RemoveDynamic(this, &AVaesenPickup::OnSettled);
	if (PickupMesh->IsSimulatingPhysics())
	{
		PickupMesh->SetSimulatePhysics(false);
	}
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

	SetItem(Template->GetDefaultObject<UVaesenItem>()->DuplicateItem(this, InQuantity));
}

void AVaesenPickup::InitializePickupFromItem(const UVaesenItem* Source, int32 InQuantity)
{
	if (HasAuthority() && Source)
	{
		SetItem(Source->DuplicateItem(this, InQuantity));
	}
}

void AVaesenPickup::SetItem(UVaesenItem* NewItem)
{
	const auto ForEachReplicated = [](UVaesenItem* Root, TFunctionRef<void(UVaesenItem*)> Function)
	{
		TArray<UVaesenItem*> Items{ Root };
		Root->GetNestedItems(Items);
		for (UVaesenItem* Each : Items)
		{
			Function(Each);
		}
	};

	if (Item)
	{
		Item->OnItemModified.RemoveDynamic(this, &AVaesenPickup::UpdateFromItem);
		ForEachReplicated(Item, [this](UVaesenItem* Each) { RemoveReplicatedSubObject(Each); });
	}

	Item = NewItem;
	ItemTemplate = Item->GetClass();
	Quantity = Item->GetQuantity();
	ForEachReplicated(Item, [this](UVaesenItem* Each) { AddReplicatedSubObject(Each); });
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

	UVaesenInventoryComponent* Inventory = Taker ? Taker->GetInventory() : nullptr;
	if (!Inventory)
	{
		return;
	}

	// Whatever doesn't fit stays here.
	const FVaesenItemAddResult Result = Inventory->TryAddItem(Item);
	UE_LOG(LogVaesenPickup, Display, TEXT("%s picked up %d of %d x %s. %s"), *Taker->GetName(), Result.AmountGiven,
		Result.AmountToGive, *Item->ItemName.ToString(), *Result.ErrorText.ToString());
	if (Result.Result == EVaesenAddItemOutcome::All)
	{
		Destroy();
	}
	else if (Result.AmountGiven > 0)
	{
		Item->SetQuantity(Item->GetQuantity() - Result.AmountGiven);
	}
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
