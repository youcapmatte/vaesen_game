#include "Inventory/VaesenLootContainer.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Inventory/VaesenInventoryComponent.h"

#define LOCTEXT_NAMESPACE "VaesenLootContainer"

AVaesenLootContainer::AVaesenLootContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bReplicateUsingRegisteredSubObjectList = true;

	// Blocks Visibility, so the interaction trace finds it.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	SetRootComponent(Mesh);

	InteractionComponent = CreateDefaultSubobject<UVaesenInteractionComponent>(TEXT("InteractionComponent"));
	InteractionComponent->SetupAttachment(Mesh);
	InteractionComponent->SetRelativeLocation(FVector(0.0, 0.0, 80.0));
	InteractionComponent->InteractableName = LOCTEXT("DefaultName", "Chest");
	InteractionComponent->ActionText = LOCTEXT("Search", "Search");
	InteractionComponent->InteractionTime = 0.5f;

	Inventory = CreateDefaultSubobject<UVaesenInventoryComponent>(TEXT("Inventory"));
	Inventory->Capacity = 12;
	Inventory->bAccessibleByOthers = true;
}

void AVaesenLootContainer::BeginPlay()
{
	// Titled like its card unless it has its own name.
	if (Inventory->DisplayName.IsEmpty())
	{
		Inventory->DisplayName = InteractionComponent->InteractableName;
	}
	Super::BeginPlay();

	InteractionComponent->OnInteract.AddDynamic(this, &AVaesenLootContainer::OnSearched);
}

void AVaesenLootContainer::OnSearched(AVaesenPlayerCharacter* Searcher)
{
	if (UVaesenInventoryComponent* SearcherInventory = Searcher ? Searcher->GetInventory() : nullptr)
	{
		SearcherInventory->OpenContainerForOwner(Inventory);
	}
}

#undef LOCTEXT_NAMESPACE
