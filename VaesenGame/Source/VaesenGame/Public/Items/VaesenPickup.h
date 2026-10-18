#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenPickup.generated.h"

class AVaesenPlayerCharacter;
class UStaticMeshComponent;
class UVaesenInteractionComponent;
class UVaesenItem;

/**
 * An item lying in the world that players can pick up. Place it (or a Blueprint of it) in a level and choose the
 * Item Template and Quantity: the mesh and the interaction card (name, description, quantity) follow the item, in
 * the editor too. Dropped items can be spawned on the server and set up with InitializePickup.
 *
 * The server creates the item instance from the template; it replicates with the pickup. Picking up happens on the
 * server (the interaction component's OnInteract): as much as fits goes into the player's inventory, and the pickup
 * goes away once it's empty.
 */
UCLASS()
class VAESENGAME_API AVaesenPickup : public AActor
{
	GENERATED_BODY()

public:
	AVaesenPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Server: makes this pickup hold Quantity of Template (e.g. for an item someone dropped). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void InitializePickup(TSubclassOf<UVaesenItem> Template, int32 InQuantity);

	/** Server: makes this pickup hold a copy of Source (Quantity of it), contents and all, e.g. a dropped pouch. */
	void InitializePickupFromItem(const UVaesenItem* Source, int32 InQuantity);

	/** The item instance (null until the server has created it). */
	UFUNCTION(BlueprintPure, Category = "Pickup")
	UVaesenItem* GetItem() const { return Item; }

protected:
	virtual void BeginPlay() override;

	/** Server: a player picked it up. */
	UFUNCTION()
	void OnTakePickup(AVaesenPlayerCharacter* Taker);

	/** Which item this is. Sets the mesh and the card. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TSubclassOf<UVaesenItem> ItemTemplate;

	/** How many; kept within the item's stack size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UVaesenInteractionComponent> InteractionComponent;

private:
	/** Holds NewItem from now on, replicating it (and what's inside it). */
	void SetItem(UVaesenItem* NewItem);

	/** Mesh and card from the item (or the template's defaults before there is one). */
	UFUNCTION()
	void UpdateFromItem();

	UFUNCTION()
	void OnRep_Item();

	UPROPERTY(ReplicatedUsing = OnRep_Item)
	TObjectPtr<UVaesenItem> Item;
};
