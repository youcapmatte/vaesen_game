#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/VaesenDamageTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayCueInterface.h"
#include "VaesenTrainingDummy.generated.h"

class UCapsuleComponent;
class UPhysicalMaterial;
class UStaticMeshComponent;
class UTextRenderComponent;
class UVaesenAbilitySystemComponent;
class UVaesenAttributeSet;

/**
 * BP_TrainingDummy: something to hit. A grey-box post with the same ability system and attributes as a character,
 * so damage goes through GE_Damage and the damage execution exactly as it would on a person. Every hit shows floating
 * numbers on every machine: per damage type, the amount dealt and what got through protection (and block). It resets
 * to full health a few seconds after the last hit, or after "dying" (OnDummyDeath).
 *
 * BP_TrainingDummy_Armored sets protection per damage type, to see damage types differ.
 */
UCLASS()
class VAESENGAME_API AVaesenTrainingDummy : public AActor, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	AVaesenTrainingDummy();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	UFUNCTION(BlueprintPure, Category = "Dummy")
	UVaesenAbilitySystemComponent* GetVaesenAbilitySystem() const { return AbilitySystem; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy", meta = (ClampMin = "1"))
	float MaxHealth = 100.0f;

	/** Protection per damage type (armor), e.g. Slashing 6, Piercing 2, Blunt 0. Negative = a weakness. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float ProtectionSlashing = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float ProtectionPiercing = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float ProtectionBlunt = 0.0f;

	/** Back to full health this long after the last hit (or the death). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy", meta = (ClampMin = "0.5", Units = "s"))
	float ResetDelay = 4.0f;

	/** Its surface for the impact table (flesh by default; the commandlet sets PM_Flesh). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UPhysicalMaterial> SurfaceMaterial;

	/** Health reached zero (server). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Dummy")
	void OnDummyDeath(AActor* Killer);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Health shown above it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UTextRenderComponent> HealthText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UVaesenAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UVaesenAttributeSet> Attributes;

private:
	UFUNCTION()
	void OnDamageReported(const FVaesenDamageReport& Report);

	UFUNCTION()
	void OnOutOfHealth(AActor* Killer);

	void ResetDummy();

	/** Every machine: shows the numbers for a hit. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowDamage(const FVaesenDamageReport& Report);

	struct FFloatingNumber
	{
		TWeakObjectPtr<UTextRenderComponent> Text;
		float Age = 0.0f;
	};
	TArray<FFloatingNumber> FloatingNumbers;
	FTimerHandle ResetTimer;
};
