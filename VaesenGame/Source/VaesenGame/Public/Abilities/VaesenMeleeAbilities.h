#pragma once

#include "CoreMinimal.h"
#include "Abilities/VaesenGameplayAbility.h"
#include "Weapons/VaesenWeaponTypes.h"
#include "VaesenMeleeAbilities.generated.h"

/**
 * A melee attack, light or heavy, with the drawn weapon (or bare hands; a gun's butt for the heavy attack). Reads
 * the attack from the weapon (FVaesenMeleeAttack): montage, damage packet, stamina, cooldown. The hit window is the
 * montage's ANS_MeleeHitWindow, or the attack's timing fallback without one; during it the server traces the
 * weapon's striking part. The player sees the swing at once; hits and damage come from the server.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenGA_MeleeAttack : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_MeleeAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** The attack this would make now, or false if there's none (a heavy attack on a weapon without one). */
	bool ResolveAttack(const FGameplayAbilityActorInfo* ActorInfo, FVaesenMeleeAttack& OutAttack) const;

protected:
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Melee")
	bool bHeavy = false;

	/** Bare hands sweep a sphere this big in front of the chest. */
	UPROPERTY(EditDefaultsOnly, Category = "Melee", meta = (Units = "cm"))
	float UnarmedRadius = 25.0f;

private:
	void OpenHitWindow();
	void CloseHitWindow();
	void OnHitWindowNotify(bool bOpen);
	void FinishAttack();

	/** Server, bare hands: one sweep in front of the character. */
	void UnarmedStrike();

	FVaesenMeleeAttack Attack;
	bool bWindowOpen = false;
	FDelegateHandle HitWindowHandle;
	FTimerHandle WindowOpenTimer;
	FTimerHandle WindowCloseTimer;
};

/** GA_MeleeLight: left mouse (gamepad right bumper). */
UCLASS()
class VAESENGAME_API UVaesenGA_MeleeLight : public UVaesenGA_MeleeAttack
{
	GENERATED_BODY()

public:
	UVaesenGA_MeleeLight();
};

/** GA_MeleeHeavy: Shift + left mouse (gamepad right trigger with a melee weapon). Ranged weapons strike with their butt. */
UCLASS()
class VAESENGAME_API UVaesenGA_MeleeHeavy : public UVaesenGA_MeleeAttack
{
	GENERATED_BODY()

public:
	UVaesenGA_MeleeHeavy();
};

/**
 * GA_Block (right mouse held, melee or bare hands): State.Blocking while held; hits from the front are reduced by the
 * block profile (a shield in the off hand, else the weapon), and the absorbed part wears it. The first moments after
 * pressing are the parry window (State.ParryWindow): a hit then is negated and the attacker staggered. Costs stamina
 * per second; slows movement (the character does that from the tag). Broken weapons can't block.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_Block : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Block();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void CloseParryWindow();
	void DrainWhileHeld();

	float StaminaPerSecond = 0.0f;
	bool bParryWindowOpen = false;
};
