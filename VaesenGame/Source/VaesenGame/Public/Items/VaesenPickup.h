#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenPickup.generated.h"

class AVaesenPlayerCharacter;
class UStaticMeshComponent;
class UVaesenInteractionComponent;
class UVaesenItem;

/**
 * An item lying in the world that players can pick up. One pickup serves every item: place it (BP_Pickup) in a level,
 * select it and choose its Item Template and Quantity. The mesh becomes the item's Pickup Mesh (none if the item has
 * none), it takes the item's Pickup Scale, and the interaction card shows the item; all in the editor too. Dropped items are spawned on the server with SpawnDropped: tossed out in front of whoever dropped
 * them, they fall with physics (off a chair onto the floor, say) and come to rest lying naturally on what they land
 * on; then physics switches off. Everyone sees the same fall (the movement replicates). The mesh needs collision.
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

	/**
	 * Server: a pickup of a copy of Quantity x Item, tossed out in front of DroppedBy (at hip height, pulled back if a
	 * wall is in the way) to fall to the ground. Null if it couldn't be spawned.
	 */
	static AVaesenPickup* SpawnDropped(UWorld* World, TSubclassOf<AVaesenPickup> PickupClass, const UVaesenItem* Item, int32 InQuantity, const AActor* DroppedBy);

	/** Server: lets it fall with physics from where it is, starting at Velocity (cm/s), until it comes to rest. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Pickup")
	void Fall(const FVector& Velocity);

	/** The item instance (null until the server has created it). */
	UFUNCTION(BlueprintPure, Category = "Pickup")
	UVaesenItem* GetItem() const { return Item; }

protected:
	virtual void BeginPlay() override;

	/** Server: a player picked it up. */
	UFUNCTION()
	void OnTakePickup(AVaesenPlayerCharacter* Taker);

	/** Which item this is. Sets the mesh, the size (once, when chosen) and the card. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TSubclassOf<UVaesenItem> ItemTemplate;

	/** How many; kept within the item's stack size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 Quantity = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UVaesenInteractionComponent> InteractionComponent;

	/** Physics stops after this long even if it's still rolling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup|Dropping", meta = (ClampMin = "0.5", Units = "s"))
	float MaxFallSeconds = 8.0f;

private:
	/** Came to rest: physics off. */
	UFUNCTION()
	void OnSettled(UPrimitiveComponent* SleepingComponent, FName BoneName);

	void StopFalling();

	FTimerHandle FallTimer;

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
