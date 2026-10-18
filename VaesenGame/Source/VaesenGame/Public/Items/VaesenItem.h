#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VaesenItem.generated.h"

class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenItemModified);

/**
 * An item: what it is (name, description, look) and how many of it there are. Each kind of item is a Blueprint
 * subclass (e.g. BP_Item_Lantern) with its values set in Class Defaults; that class is the item's template.
 * Actual items in the world (in a pickup, later an inventory) are instances of it, with their own Quantity.
 *
 * Instances replicate as subobjects of the actor that owns them (see AVaesenPickup).
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class VAESENGAME_API UVaesenItem : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	/** What the item looks like lying in the world (on a pickup). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UStaticMesh> PickupMesh;

	/** Several fit in one stack (up to MaxStackSize). Off: one per stack, and MaxStackSize is locked at 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bStackable = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1", EditCondition = "bStackable"))
	int32 MaxStackSize = 20;

	/** Most that fit in one stack: MaxStackSize, or 1 when not stackable. */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetMaxStack() const { return bStackable ? FMath::Max(MaxStackSize, 1) : 1; }

	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetQuantity() const { return Quantity; }

	/** Clamped to 1..GetMaxStack(). Server only (it replicates). */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void SetQuantity(int32 NewQuantity);

	/** The quantity changed (also on clients when it arrives). */
	UPROPERTY(BlueprintAssignable, Category = "Item")
	FVaesenItemModified OnItemModified;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_Quantity, BlueprintReadOnly, Category = "Item")
	int32 Quantity = 1;

private:
	UFUNCTION()
	void OnRep_Quantity();
};
