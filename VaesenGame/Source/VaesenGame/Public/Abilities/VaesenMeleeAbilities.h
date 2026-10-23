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

	/** The attack this would make now (the next swing of a combo), or false if there's none (a heavy attack on a weapon without one). */
	bool ResolveAttack(const FGameplayAbilityActorInfo* ActorInfo, FVaesenMeleeAttack& OutAttack) const;

	/** Which swing of the combo the next attack is (0 = the first). */
	int32 GetNextComboStep() const;

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

	/** The combo: the swing that comes next, and when the last one was over. */
	int32 ComboStep = 0;
	double LastSwingOverTime = -1.0;
	float ComboWindow = 0.0f;
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

/**
 * GA_Dodge (Left Alt, gamepad right stick click): a quick step out of the way with the drawn weapon, the way the
 * character is moving (back when standing still), carried by the montage's root motion. Uses the weapon category's
 * dodge montages; drops a block or aim.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_Dodge : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Dodge();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** The montage for a dodge the way Character is moving now (null = it can't dodge that way). */
	static UAnimMontage* ChooseMontage(const AVaesenBaseCharacter* Character, const UVaesenWeaponItem* Weapon);

protected:
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const override { return StaminaCost; }

	UPROPERTY(EditDefaultsOnly, Category = "Dodge", meta = (ClampMin = "0"))
	float StaminaCost = 15.0f;

private:
	void FinishDodge();
};
