#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenItemDragDropOperation.generated.h"

class UVaesenInventoryComponent;

/** An item stack (or part of one) being dragged from an inventory slot, or a worn item from an equipment slot. */
UCLASS()
class VAESENGAME_API UVaesenItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UVaesenInventoryComponent> SourceInventory;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SourceIndex = INDEX_NONE;

	/** How many are being moved; -1 = the whole stack. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Amount = -1;

	/** Dragged off the player's equipment (EquipmentSlot) rather than out of an inventory. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	bool bFromEquipment = false;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EVaesenEquipmentSlot EquipmentSlot = EVaesenEquipmentSlot::Head;
};
