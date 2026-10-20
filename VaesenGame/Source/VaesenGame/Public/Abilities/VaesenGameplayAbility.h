#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "VaesenGameplayAbility.generated.h"

class AVaesenBaseCharacter;
class UAnimMontage;
class UVaesenEquipmentComponent;
class UVaesenWeaponItem;

/**
 * Base for Vaesen's abilities. Each reads its numbers from the equipped weapon's data, so one ability serves every
 * weapon of a kind (GA_FireWeapon fires any gun).
 *
 *   - Input: InputBinding is the spec's input; several abilities share one (left mouse fires a gun or swings a sword)
 *     and CanActivateAbility checks the drawn weapon.
 *   - Stamina: GetStaminaCost() is applied as the cost effect (GE_StaminaCost, SetByCaller Data.StaminaCost); not
 *     enough stamina and the ability doesn't activate.
 *   - Aim: abilities that shoot or throw send the player's aim (origin, direction, strength) to the server as
 *     replicated target data; the server's copy acts on it (OnAimReceived).
 *   - Montages play through the ability system, so other players see them; an empty montage is fine (timers instead).
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UVaesenGameplayAbility();

	/** The input it's bound to when granted. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EVaesenAbilityInput InputBinding = EVaesenAbilityInput::None;

	/** The stamina cost effect (GE_StaminaCost). Empty = the C++ one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Costs")
	TSubclassOf<UGameplayEffect> StaminaCostEffect;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** Stamina this activation costs (from the weapon's data). */
	virtual float GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const { return 0.0f; }

	AVaesenBaseCharacter* GetVaesenCharacter(const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;
	UVaesenEquipmentComponent* GetEquipment(const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;
	UVaesenWeaponItem* GetDrawnWeapon(const FGameplayAbilityActorInfo* ActorInfo = nullptr) const;
	UVaesenAbilitySystemComponent* GetVaesenASC() const;

	/** Takes Amount of stamina now (server; per second costs while blocking or drawing a bow). False if there wasn't enough. */
	bool DrainStamina(float Amount);

	/** Plays Montage through the ability system (replicated to other players). Returns its length at Rate, 0 if none. */
	float PlayAbilityMontage(UAnimMontage* Montage, float Rate = 1.0f, FName Section = NAME_None);

	/** Stops the montage this ability played (if it's still playing). */
	void StopAbilityMontage();

	/** Where this player is looking: the camera (or the eyes for AI). */
	void GetAimViewPoint(FVector& OutOrigin, FVector& OutDirection) const;

	// --- Aim to the server ---

	/** Locally controlled: hands the aim to the server (or straight to OnAimReceived when this machine is the server). */
	void SendAim(const FVector& Origin, const FVector& Direction, float Strength = 1.0f);

	/** Server, not locally controlled: waits for the client's aim (call from ActivateAbility). */
	void WaitForAim();

	/** The server has the aim (and, when this machine is also the player's, the local one). */
	virtual void OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength) {}

	/** The client cancelled before aiming (released too early, a bad throw). */
	virtual void OnAimCancelled() {}

	/** Sends that the aim won't come (the server's copy ends). */
	void SendAimCancelled();

	FTimerManager& GetTimers() const;

	/** Timers for the ability's steps (cleared when it ends). */
	FTimerHandle StepTimer;
	FTimerHandle RepeatTimer;

private:
	void OnTargetDataReplicated(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag);
	void OnTargetDataCancelled();

	FDelegateHandle TargetDataHandle;
	FDelegateHandle TargetCancelHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PlayingMontage;
};
