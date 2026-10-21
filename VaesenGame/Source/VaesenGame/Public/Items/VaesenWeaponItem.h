#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "Weapons/VaesenWeaponData.h"
#include "VaesenWeaponItem.generated.h"

class AVaesenWeaponActor;
class UAnimInstance;
class UAnimMontage;
class USkeletalMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenDurabilityChanged);

/**
 * Something to fight with. The item is the data; when it's equipped the server spawns its weapon actor
 * (AVaesenWeaponActor), the thing in the hand or the sheath, which runs firing, reloading and attacks.
 *
 * Kinds: UVaesenRangedWeaponItem (guns, bows, crossbows) and UVaesenMeleeWeaponItem (daggers, swords, axes, clubs,
 * shields). Throwing isn't a kind of weapon: any item can be throwable (UVaesenItem::Throwing).
 *
 * Holding: the weapon mesh's Grip socket lines up with the character's hand socket (hand_r_weapon), so weapons of
 * the same category share animations; LeftHandIK places the off hand on two-handed weapons. Carrying: the weapon
 * hangs at SheathSocket (+ SheathOffset) until it's drawn (H).
 *
 * Animation, sounds and attacks default to the category's (DA_WeaponCategoryDefaults); set the overrides here only
 * for a weapon with its own.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenWeaponItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenWeaponItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Copies keep durability (and loaded rounds), never the weapon actor. */
	virtual UVaesenItem* DuplicateItem(UObject* NewOuter, int32 NewQuantity) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Picks the default anim layer, montages, attacks, block profile and sounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EVaesenWeaponCategory Category = EVaesenWeaponCategory::Improvised;

	/** Takes both hands (the off hand stays empty). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	bool bTwoHanded = false;

	// --- Mesh ---

	/** In the hand and the sheath, with sockets Grip (required), LeftHandIK, Muzzle, Eject, TraceStart/TraceEnd. Empty = Pickup Mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<UStaticMesh> WeaponMesh;

	/** Instead of WeaponMesh, for weapons with moving parts (a bow's string, a lever). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TObjectPtr<USkeletalMesh> WeaponSkeletalMesh;

	/** The actor spawned while it's equipped. Empty = the default for this kind (ranged or melee). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Mesh")
	TSubclassOf<AVaesenWeaponActor> WeaponActorClass;

	// --- Carrying ---

	/** Where it hangs while sheathed (sockets on the character, from DA_WeaponCategoryDefaults). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sheath", meta = (GetOptions = "GetSheathSocketOptions"))
	FName SheathSocket = TEXT("Sheath_BackLower");

	/** Fine-tunes how it sits in the sheath socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sheath")
	FTransform SheathOffset;

	/** How it sits in the hands, in every pose and shouldered while aiming (tune it in play: vaesen.weaponfit). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Hold")
	FVaesenWeaponHold Hold;

	/** Its own draw and sheath montages. Empty = the sheath location's shared ones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sheath")
	TObjectPtr<UAnimMontage> DrawMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sheath")
	TObjectPtr<UAnimMontage> SheathMontage;

	// --- Animation and sound overrides (empty = the category's) ---

	/** A unique anim layer for a signature weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> AnimLayerOverride;

	/** Replace single montages, keeping the category's locomotion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	FVaesenWeaponMontages MontageOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sound")
	TObjectPtr<UVaesenWeaponSoundSet> SoundSet;

	// --- Durability ---

	/** Durability when new; at zero the weapon is broken (can't attack or block; can still be thrown). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Durability", meta = (ClampMin = "1"))
	float MaxDurability = 100.0f;

	/** Below this share of MaxDurability the player is warned (and hears the creak). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Durability", meta = (ClampMin = "0", ClampMax = "1"))
	float NearBreakFraction = 0.2f;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetDurability() const { return CurrentDurability < 0.0f ? MaxDurability : CurrentDurability; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsBroken() const { return GetDurability() <= 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsNearBreaking() const { return !IsBroken() && GetDurability() <= MaxDurability * NearBreakFraction; }

	/** Server: wears it by Amount. Crossing the warning line or breaking plays the creak or the break on every machine. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void ApplyWear(float Amount);

	/** Server: back to full durability (repairs come later; the tests use it). */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Weapon")
	void Repair();

	/** Durability changed (on every machine). */
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FVaesenDurabilityChanged OnDurabilityChanged;

	// --- Resolved data ---

	/** Its category's defaults (DA_WeaponCategoryDefaults, or the built-in ones). */
	const FVaesenCategoryDefaults& GetCategoryDefaults() const;

	/** The anim layer while drawn: its own, else the category's (may be empty). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	TSubclassOf<UAnimInstance> GetAnimLayer() const;

	/** One of its montages: its own override, else the category's (may be null). */
	UAnimMontage* GetMontage(TObjectPtr<UAnimMontage> FVaesenWeaponMontages::* Which) const;

	/** Its sound for a moment, else the category's (may be null). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	USoundBase* GetSound(EVaesenWeaponSound Sound) const;

	/** The sound set the sound would come from (for attenuation and concurrency). */
	const UVaesenWeaponSoundSet* GetSoundSetFor(EVaesenWeaponSound Sound) const;

	virtual USoundBase* GetLandSound() const override { return LandSound ? LandSound.Get() : GetSound(EVaesenWeaponSound::DropLand); }

	/** How it blocks (melee weapons can override the category's). */
	virtual const FVaesenBlockProfile& GetBlockProfile() const { return GetCategoryDefaults().BlockProfile; }

	/** Its draw / sheath montage: its own, else the sheath location's (may be null). */
	UAnimMontage* GetDrawMontage(bool bDraw) const;

	/** Held in the off hand (a shield) rather than the main hand. */
	bool IsHeldInOffHand() const { return Slot == EVaesenEquipmentSlot::OffHand; }

	/** Whether it's a ranged weapon (guns, bows). */
	virtual bool IsRanged() const { return false; }

	/** The actor in the hand or sheath (server spawns it on equip; replicated). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	AVaesenWeaponActor* GetWeaponActor() const { return WeaponActor; }

	/** The mesh used in the hand (WeaponMesh, else PickupMesh). */
	UStaticMesh* GetHeldStaticMesh() const;

	UFUNCTION()
	TArray<FName> GetSheathSocketOptions() const;

protected:
	virtual void OnEquipped_Implementation(AVaesenBaseCharacter* Character) override;
	virtual void OnUnequipped_Implementation(AVaesenBaseCharacter* Character) override;

	/** The default actor class for this kind of weapon. */
	virtual TSubclassOf<AVaesenWeaponActor> GetDefaultActorClass() const;

	/** -1 = full (MaxDurability); so fresh items from class defaults start whole. */
	UPROPERTY(ReplicatedUsing = OnRep_Durability, BlueprintReadOnly, Category = "Weapon")
	float CurrentDurability = -1.0f;

private:
	UFUNCTION()
	void OnRep_Durability();

	/** The actor and this pointer to it can reach a client in either order; whichever comes second attaches it. */
	UFUNCTION()
	void OnRep_WeaponActor();

	UPROPERTY(ReplicatedUsing = OnRep_WeaponActor)
	TObjectPtr<AVaesenWeaponActor> WeaponActor;
};
