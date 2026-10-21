#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "VaesenCombatSelfTest.generated.h"

class AAIController;
class AVaesenPlayerCharacter;
class AVaesenTrainingDummy;
class UVaesenAbilitySystemComponent;

/**
 * The in-map combat check: console command `vaesen.combat.selftest` (best in L_CombatTest, works in any map with a
 * floor). On the server it spawns a fighter (BP_VaesenPlayerCharacter, AI-controlled, so it uses the same abilities,
 * GA_ Blueprints and input as a player) and training dummies 15 m to the side of the first player start, then runs
 * scenarios with the test weapon Blueprints made by the commandlet: revolver reload and fire, Krag bolt cycle, sword
 * light and heavy, block and parry against an attacker, throwing a stone, drawing the bow. Each check prints PASS or
 * FAIL on screen and to the log (LogVaesenCombat); everything spawned is removed at the end.
 *
 * It checks the assets and the map together (Blueprints, data assets, collision), which the C++ automation tests
 * (Vaesen.Combat.*) can't. Run it with two clients watching: they should see everything the fighter does.
 */
UCLASS()
class VAESENGAME_API UVaesenCombatSelfTest : public UObject
{
	GENERATED_BODY()

public:
	/** Starts the run in World (the server's). */
	static void Run(UWorld* World);

	virtual UWorld* GetWorld() const override { return World.Get(); }

private:
	struct FStep
	{
		FString Name;
		/** Does the step; returns how long to wait before the next one. */
		TFunction<float()> Action;
	};

	void Start();
	void BuildSteps();
	void RunNextStep();
	void Finish();

	/** Records a check. */
	void Check(bool bPassed, const FString& What);

	/** The fighter's left palm (hand_l_weapon) is on the drawn weapon's LeftGrip, place and turn. */
	void CheckLeftPalm(const TCHAR* When);

	/** Spent cases lying in the world (cosmetic AVaesenCasing actors). */
	int32 CountCasings() const;
	int32 CasingsBefore = 0;

	/** The rifle's stock and barrel just before a shot, to measure its kick. */
	bool RiflePose(FVector& OutStock, FVector& OutBarrel) const;
	FVector RestStock = FVector::ZeroVector;
	FVector RestBarrel = FVector::ForwardVector;
	void Say(const FString& Line, const FColor& Color) const;

	/** A test asset from the commandlet (null + FAIL if missing). */
	UClass* LoadTestClass(const TCHAR* Path);

	/** Gives the fighter an item and equips it (weapons). */
	bool GiveAndEquip(const TCHAR* Path);
	void Press(uint8 Input) const;
	void Release(uint8 Input) const;
	float DummyDamage(const AVaesenTrainingDummy* Dummy) const;
	void ResetAll();

	TWeakObjectPtr<UWorld> World;
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<AVaesenPlayerCharacter> Fighter;

	UPROPERTY()
	TObjectPtr<AAIController> Controller;

	UPROPERTY()
	TObjectPtr<AVaesenTrainingDummy> Target;

	UPROPERTY()
	TObjectPtr<AVaesenTrainingDummy> Armored;

	UPROPERTY()
	TObjectPtr<AVaesenTrainingDummy> Attacker;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> Spawned;

	TArray<FStep> Steps;
	int32 NextStep = 0;
	int32 Passed = 0;
	int32 Failed = 0;
	FTimerHandle StepTimer;

	/** The run in progress (one at a time). */
	static TStrongObjectPtr<UVaesenCombatSelfTest> Current;
};
