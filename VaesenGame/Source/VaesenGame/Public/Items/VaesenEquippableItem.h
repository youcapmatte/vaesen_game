#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenItemTypes.h"
#include "VaesenEquippableItem.generated.h"

class AVaesenBaseCharacter;
class USoundBase;
class UStaticMeshComponent;

/**
 * An item that's equipped into a slot, and only that one (see UVaesenEquipmentComponent). Right-clicking it in the
 * inventory ("Equip") puts it on. Equip/Unequip handle the look of what's attached, so the equipment component calls
 * them on every machine; game effects belong on the server.
 *
 * Rigid things worn or held (weapons, tools, talismans, bags) set EquippedMesh: it's attached to AttachSocket (a
 * bone or socket on the character's body) while equipped. Armor replaces a body part's mesh instead (done by the
 * equipment component).
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

	/** Played to the player who puts it on (chainmail rattles, a hat doesn't). Empty = Project Settings > Vaesen's default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Sounds")
	TObjectPtr<USoundBase> EquipSound;

	/** Played to the player when it can't be put on (wrong slot, no room). Empty = Project Settings > Vaesen's default. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Sounds")
	TObjectPtr<USoundBase> EquipRejectedSound;

	virtual bool CanUse_Implementation(const AVaesenPlayerCharacter* Character) const override;
	virtual void Use_Implementation(AVaesenPlayerCharacter* Character) override;

	/** Shows it on Character (called by the equipment component on every machine). */
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Equip(AVaesenBaseCharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void Unequip();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	bool IsEquipped() const { return EquippedCharacter.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	AVaesenBaseCharacter* GetEquippedCharacter() const;

protected:
	/** Puts it on the character (by default: attaches EquippedMesh). Call the parent when overriding. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment")
	void OnEquipped(AVaesenBaseCharacter* Character);

	/** Takes it off again. Call the parent when overriding. */
	UFUNCTION(BlueprintNativeEvent, Category = "Equipment")
	void OnUnequipped(AVaesenBaseCharacter* Character);

private:
	TWeakObjectPtr<AVaesenBaseCharacter> EquippedCharacter;
	TWeakObjectPtr<UStaticMeshComponent> EquippedMeshComponent;
};
