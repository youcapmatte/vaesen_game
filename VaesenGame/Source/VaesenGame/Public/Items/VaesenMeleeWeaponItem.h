#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenWeaponItem.h"
#include "VaesenMeleeWeaponItem.generated.h"

/**
 * Daggers, swords, axes, clubs, warhammers, improvised weapons, and shields (category Shield, in the off hand).
 *
 * A light attack and an optional heavy one, each a FVaesenMeleeAttack (montage, damage packet, reach, stamina...):
 * the category's by default, or this weapon's own. The weapon mesh's TraceStart and TraceEnd sockets mark the
 * striking part (blade, hammer head); while the attack's hit window is open the server sweeps between them.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenMeleeWeaponItem : public UVaesenWeaponItem
{
	GENERATED_BODY()

public:
	UVaesenMeleeWeaponItem();

	/** Use this weapon's own attacks instead of the category's. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	bool bOverrideAttacks = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "bOverrideAttacks"))
	FVaesenMeleeAttack LightAttack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "bOverrideAttacks"))
	bool bHasHeavyAttack = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "bOverrideAttacks && bHasHeavyAttack"))
	FVaesenMeleeAttack HeavyAttack;

	/** Use this weapon's own block profile instead of the category's. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee")
	bool bOverrideBlockProfile = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "bOverrideBlockProfile"))
	FVaesenBlockProfile BlockProfile;

	/** The light (or heavy) attack; null when it has no heavy attack. */
	const FVaesenMeleeAttack* GetAttack(bool bHeavy) const;

	virtual const FVaesenBlockProfile& GetBlockProfile() const override;

	/** A shield: blocks with its own profile while a weapon is in the main hand. */
	bool IsShield() const { return Category == EVaesenWeaponCategory::Shield; }
};
