#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Combat/VaesenDamageTypes.h"
#include "VaesenAbilitySystemComponent.generated.h"

/** Which input an ability listens to (its spec's InputID). Abilities on the same input check the drawn weapon to decide which one runs. */
UENUM(BlueprintType)
enum class EVaesenAbilityInput : uint8
{
	None,
	/** Left mouse: light attack / fire. */
	Attack,
	/** Shift + left mouse: heavy attack. */
	HeavyAttack,
	/** Right mouse (hold): aim (ranged) or block and parry (melee). */
	AimOrBlock,
	/** R: reload, or work the bolt. */
	Reload,
	/** G (hold to aim, release to throw). */
	Throw,
	/** H: draw or sheathe. */
	Holster,
	/** Q: draw the other carried weapon. */
	SwapWeapon,
	/** Left Alt: dodge (the way the character is moving; back when standing still). */
	Dodge
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVaesenDamageReported, const FVaesenDamageReport&, Report);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVaesenHitConfirmed, EVaesenHitOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVaesenOutOfHealth, AActor*, DamageInstigator);

/**
 * The ability system component on every character and training dummy. Adds what combat needs on top of GAS:
 *   - damage reports (what was dealt, before and after protection and block) for debug numbers and the log;
 *   - hit confirmation for the attacker's hit marker, only once the server has decided;
 *   - running out of health (State.Dead), until a condition system takes over.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UVaesenAbilitySystemComponent();

	/** Server: this one took a hit (called by the damage execution). Tells listeners, and the attacker's player. */
	void ReportDamageTaken(const FVaesenDamageReport& Report, UAbilitySystemComponent* Source);

	/** Server: health just reached zero. */
	void HandleOutOfHealth(AActor* DamageInstigator, const FGameplayEffectSpec& Spec);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const;

	/** Back to full health and stamina, alive again (the training dummies' reset). Server. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Combat")
	void ResetVitals();

	/** Pressed / released an ability input on this player's machine. */
	void PressAbilityInput(EVaesenAbilityInput Input);
	void ReleaseAbilityInput(EVaesenAbilityInput Input);

	/** Server: something this one is hit by (the damage report). */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FVaesenDamageReported OnDamageReported;

	/** On the attacker's own machine: the server confirmed a hit (for the hit marker). */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FVaesenHitConfirmed OnHitConfirmed;

	/** Server: health reached zero. */
	UPROPERTY(BlueprintAssignable, Category = "Combat")
	FVaesenOutOfHealth OnOutOfHealth;

private:
	UFUNCTION(Client, Unreliable)
	void ClientHitConfirmed(EVaesenHitOutcome Outcome);
};
