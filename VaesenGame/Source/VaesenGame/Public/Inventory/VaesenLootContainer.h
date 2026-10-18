#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenLootContainer.generated.h"

class AVaesenPlayerCharacter;
class UStaticMeshComponent;
class UVaesenInteractionComponent;
class UVaesenInventoryComponent;

/**
 * Something with things in it that players search: a chest, a crate, a wardrobe, a desk. Interacting opens its
 * inventory beside the player's own, to take from and put into. Set the mesh, the card (InteractionComponent) and
 * the contents (Inventory > Starting Items) per placed actor or in a Blueprint.
 *
 * Any actor can hold things the same way: add a Vaesen Inventory component (and an interaction component whose
 * OnInteract calls the player's inventory OpenContainerForOwner).
 */
UCLASS()
class VAESENGAME_API AVaesenLootContainer : public AActor
{
	GENERATED_BODY()

public:
	AVaesenLootContainer();

	UFUNCTION(BlueprintPure, Category = "Container")
	UVaesenInventoryComponent* GetInventory() const { return Inventory; }

protected:
	virtual void BeginPlay() override;

	/** Server: a player searched it. */
	UFUNCTION()
	void OnSearched(AVaesenPlayerCharacter* Searcher);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UVaesenInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Container")
	TObjectPtr<UVaesenInventoryComponent> Inventory;
};
