#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenEquippableItem.generated.h"

class UStaticMeshComponent;

/**
 * An item that's equipped into a slot. Equip/Unequip handle the look on the character, so they're called on every
 * machine (by the inventory, from its replicated equipment); game effects belong on the server.
 *
 * Rigid things worn or held (weapons, tools, talismans, bags) set EquippedMesh: it's attached to AttachSocket (a
 * bone or socket on the character's body) while equipped. Armor replaces a body part's mesh instead.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenEquippableItem : public UVaesenItem
{
	GENERATED_BODY()

public:
	UVaesenEquippableItem();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	EVaesenEquipmentSlot Slot = EVaesenEquipmentSlot::MainHand;

	/** Shown on the character while equipped, attached to AttachSocket. Empty = nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UStaticMesh> EquippedMesh;

	/** Bone or socket on the character's body (torso mesh) for EquippedMesh, e.g. hand_r. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName AttachSocket = TEXT("hand_r");

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Equip(AVaesenPlayerCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Unequip();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsEquipped() const { return EquippedCharacter.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AVaesenPlayerCharacter* GetEquippedCharacter() const { return EquippedCharacter.Get(); }

protected:
	/** Puts it on the character (by default: attaches EquippedMesh). Call the parent when overriding. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment")
	void OnEquipped(AVaesenPlayerCharacter* Character);

	/** Takes it off again. Call the parent when overriding. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment")
	void OnUnequipped(AVaesenPlayerCharacter* Character);

private:
	TWeakObjectPtr<AVaesenPlayerCharacter> EquippedCharacter;
	TWeakObjectPtr<UStaticMeshComponent> EquippedMeshComponent;
};
