#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenTrainingDummy.h"
#include "VaesenTrainingDummyAttacker.generated.h"

/**
 * BP_TrainingDummy_Attacker: swings a club at whoever is nearest in front of it, on a timer, to test block, parry,
 * stamina and durability. Each swing is telegraphed (the club rises for WindupTime) and then strikes through the same
 * damage path as a player's attack (blockable Blunt). A parried dummy is staggered and skips its next swing.
 */
UCLASS()
class VAESENGAME_API AVaesenTrainingDummyAttacker : public AVaesenTrainingDummy
{
	GENERATED_BODY()

public:
	AVaesenTrainingDummyAttacker();

	virtual void Tick(float DeltaSeconds) override;

	/** Time between swings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker", meta = (ClampMin = "0.5", Units = "s"))
	float SwingInterval = 3.0f;

	/** The club rises this long before it strikes (the moment to parry is just before the strike). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker", meta = (ClampMin = "0.1", Units = "s"))
	float WindupTime = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker", meta = (Units = "cm"))
	float Reach = 170.0f;

	/** Turns to face the nearest player within this distance before each swing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker", meta = (Units = "cm"))
	float AwarenessRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker")
	FVaesenDamagePacket SwingDamage;

	/** Off = an unblockable swing (a vaesen's grab), to test that. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker")
	bool bSwingCanBeBlocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker")
	TObjectPtr<USoundBase> WindupSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attacker")
	TObjectPtr<USoundBase> SwingSound;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attacker")
	TObjectPtr<USceneComponent> ClubPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attacker")
	TObjectPtr<UStaticMeshComponent> Club;

private:
	void StartSwing();
	void Strike();

	/** Every machine: the club's motion and sounds (0 = rest, 1 = raised, 2 = struck). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSwingPhase(uint8 Phase);

	FTimerHandle SwingTimer;
	FTimerHandle StrikeTimer;
	float ClubPitch = 0.0f;
	float TargetClubPitch = 0.0f;
};
