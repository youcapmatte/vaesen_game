#pragma once

#include "CoreMinimal.h"
#include "Abilities/VaesenGameplayAbility.h"
#include "VaesenGeneralAbilities.generated.h"

class UVaesenItem;

/**
 * GA_Holster (H) and GA_SwapWeapon (Q): ask the equipment to draw or sheathe. The server changes the drawn state;
 * every machine then plays the draw or sheath montage and moves the weapon at the grab moment.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_Holster : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Holster();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** Draw the other carried weapon instead of toggling. */
	UPROPERTY(EditDefaultsOnly, Category = "Holster")
	bool bSwap = false;
};

UCLASS()
class VAESENGAME_API UVaesenGA_SwapWeapon : public UVaesenGA_Holster
{
	GENERATED_BODY()

public:
	UVaesenGA_SwapWeapon();
};

/** What would be thrown: the drawn weapon if it's throwable (even broken), else the first throwable stack in the inventory. */
struct FVaesenThrowable
{
	UVaesenItem* Item = nullptr;
	/** Set when it's the drawn weapon (it's taken out of the equipment, not the inventory). */
	bool bEquipped = false;
	uint8 EquippedSlot = 0;

	bool IsValid() const { return Item != nullptr; }
};

/**
 * GA_Throw (G held, released): hold to aim (State.Throwing; the HUD draws the arc on this player's screen), release to
 * throw. The server takes one from the stack (or unequips the drawn weapon), spawns a thrown item that deals the
 * item's throw damage and lands as a pickup. Costs the item's throw stamina.
 */
UCLASS()
class VAESENGAME_API UVaesenGA_Throw : public UVaesenGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGA_Throw();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
		const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	/** What Character would throw now. */
	static FVaesenThrowable FindThrowable(const AActor* Character);

	/** Where a throw leaves the hand and how fast, for the item and aim (the HUD's arc uses this too). */
	static void GetThrowLaunch(const AActor* Character, const UVaesenItem* Item, const FVector& AimDirection, FVector& OutStart, FVector& OutVelocity);

protected:
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const override;
	virtual void OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength) override;
	virtual void OnAimCancelled() override;

private:
	bool bReleased = false;
};
