#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenDamageTypes.h"
#include "Weapons/VaesenWeaponData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaesenCombatLibrary.generated.h"

class UAnimMontage;
class UAnimNotify;
class UAbilitySystemComponent;
class UVaesenWeaponItem;

VAESENGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogVaesenCombat, Log, All);

/** Combat helpers shared by melee, ranged and thrown attacks. */
UCLASS()
class VAESENGAME_API UVaesenCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Server: deals Packet to Target through GE_Damage (one SetByCaller per damage type); UVaesenDamageExecution applies
	 * protection, block and parry. The one way damage is dealt, for melee, ranged and thrown alike. Targets without an
	 * ability system only get the packet logged. Returns whether it reached an ability system.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat")
	static bool ApplyDamagePacket(AActor* DamageInstigator, AActor* Target, const FVaesenDamagePacket& Packet, const FHitResult& Hit,
		UObject* SourceObject, bool bCanBeBlocked = true, bool bProjectile = false, float Knockback = 0.0f);

	/** Plays the impact table's sound and effect for a hit of DamageType, delivered so, on what Hit touched (locally, cosmetic). Shots can ricochet. */
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (WorldContext = "WorldContext"))
	static void PlayImpactEffects(const UObject* WorldContext, FGameplayTag DamageType, const FHitResult& Hit, EVaesenImpactDelivery Delivery = EVaesenImpactDelivery::ByHand);

	/** The surface (physical material) Hit touched. */
	static EPhysicalSurface GetSurface(const FHitResult& Hit);

	/** Whether vaesen.combat.debug is on (draw traces, projectile paths, hit points). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static bool IsCombatDebugEnabled();

	/** vaesen.combat.freefire: guns and bows fire without ammunition (for now, on outside shipping builds). */
	static bool IsFreeFire();

	/** What would block a hit on Target now: its drawn shield, else its drawn weapon, else bare hands. Null profile = can't block. */
	static const FVaesenBlockProfile* FindBlockProfile(const AActor* Target, bool bProjectile, UVaesenWeaponItem*& OutBlockingItem);

	/** Whether the montage has a notify of this class (any track). */
	static bool MontageHasNotify(const UAnimMontage* Montage, TSubclassOf<UObject> NotifyClass);

	/** Whether it has AN_WeaponGrab or AN_WeaponRelease (so the notify, not a timer, swaps the weapon). */
	static bool MontageHasWeaponHandoffNotify(const UAnimMontage* Montage);

	/** Whether it has an ANS_MeleeHitWindow (so the notify, not a timer, opens the hit window). */
	static bool MontageHasHitWindow(const UAnimMontage* Montage);

	/** Whether it has an AN_EjectCasing (so the notify, not the code, throws out the casing). */
	static bool MontageHasCasingNotify(const UAnimMontage* Montage);

	/** The damage type of a packet's biggest entry (for the hit cue and impact effects). */
	static FGameplayTag GetMainDamageType(const FVaesenDamagePacket& Packet);
};
