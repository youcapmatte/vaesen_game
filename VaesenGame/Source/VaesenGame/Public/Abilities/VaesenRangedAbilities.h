#pragma once

#include "CoreMinimal.h"
#include "Abilities/VaesenGameplayAbility.h"
#include "VaesenRangedAbilities.generated.h"

class AVaesenRangedWeaponActor;
class UVaesenRangedWeaponItem;

/** Base for abilities that use the drawn gun, bow or crossbow. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenRangedAbility : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenRangedAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

protected:
	UVaesenRangedWeaponItem* GetRangedItem(const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;

	/** Aiming (right mouse, or a bow being drawn) right now. */
	bool IsAiming() const;
	AVaesenRangedWeaponActor* GetRangedActor(const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;

	/** Whether this ability is for bows (Action Draw) or for everything else. */
	virtual bool WantsBow() const { return false; }
};

/**
 * GA_FireWeapon (left mouse, guns and crossbows): plays the shot at once on the shooter's machine, sends the aim to the
 * server, which checks and fires (AVaesenRangedWeaponActor::ServerFire). Empty or unready: a dry click. Bolt and lever
 * actions are worked automatically afterwards (GA_CycleAction). Interrupts a reload that can be interrupted.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_FireWeapon : public UVaesenRangedAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_FireWeapon();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;

	/** Worked after a shot when the action needs it. */
	UPROPERTY(EditDefaultsOnly, Category = "Fire")
	TSubclassOf<UGameplayAbility> CycleAbility;

protected:
	virtual void OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength) override;
	virtual void OnAimCancelled() override;

	/** Server: after the shot, work the bolt if it needs it. */
	void StartCycleIfNeeded();
};

/**
 * GA_Reload (R): takes ammo from the inventory (pouches included, a cartridge box) into the weapon. Magazine: all at
 * once after ReloadTime. SingleRound and Cylinder: a round per ReloadTime, interruptible between rounds (firing stops
 * it). Cylinder and breech-loaders throw out their spent casings first (when casings eject on reload). MuzzleLoading:
 * one long reload that can't be interrupted. Nothing to load (or no ammo): nothing happens.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_Reload : public UVaesenRangedAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Reload();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void StartRound();
	void FinishRound();

	bool bPerRound = false;
};

/** GA_CycleAction (R, or by itself after a shot): works the bolt or lever; firing waits until it's done. Casings out if they eject on cycle. */
UCLASS()
class VAESENGAME_API UVaesenGA_CycleAction : public UVaesenRangedAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_CycleAction();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	/** The bolt is back: the spent case flies out. */
	void EjectCase();

	void FinishCycle();
};

/** GA_Aim (right mouse held, ranged): State.Aiming: the camera zooms, the crosshair tightens, the body strafes. */
UCLASS()
class VAESENGAME_API UVaesenGA_Aim : public UVaesenRangedAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Aim();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
};

/**
 * GA_DrawBow (left mouse held, bows): Nock (NockTime) -> Draw (strength builds over DrawTime) -> Hold -> Release. The
 * draw strength scales speed and damage; releasing before MinDrawToShoot lets the arrow down without a shot. Holding
 * costs stamina per second. Arrows come straight from the inventory.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_DrawBow : public UVaesenRangedAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_DrawBow();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** 0..1 share of a full draw right now. */
	float GetDrawFraction() const;

protected:
	virtual bool WantsBow() const override { return true; }
	virtual void OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength) override;
	virtual void OnAimCancelled() override;

private:
	void TickDraw();

	double DrawStartTime = 0.0;
	bool bReleased = false;
};
