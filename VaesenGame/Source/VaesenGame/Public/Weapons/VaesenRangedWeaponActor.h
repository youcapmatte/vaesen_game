#pragma once

#include "CoreMinimal.h"
#include "Weapons/VaesenWeaponActor.h"
#include "VaesenRangedWeaponActor.generated.h"

class UAnimMontage;
class UVaesenRangedWeaponItem;
class UPoseableMeshComponent;

/** Why a shot didn't happen. */
UENUM(BlueprintType)
enum class EVaesenFireBlock : uint8
{
	None,
	NoWeapon,
	Broken,
	Empty,
	NeedsCycle,
	TooSoon
};

/**
 * A gun, bow or crossbow in the hand. The server fires (GA_FireWeapon asks it, with the player's aim): it checks the
 * shot (loaded, not too soon, aim plausible), traces (InstantHit) or spawns a projectile, takes the round, and tells
 * the other machines to play the shot. The firing player's machine has already played it (prediction).
 *
 * Casings are cosmetic: each machine spawns its own at the Eject socket, never replicated (AN_EjectCasing in a
 * montage, or the code at the right moment when the montage has none).
 */
UCLASS()
class VAESENGAME_API AVaesenRangedWeaponActor : public AVaesenWeaponActor
{
	GENERATED_BODY()

public:
	AVaesenRangedWeaponActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UVaesenRangedWeaponItem* GetRangedItem() const;

	/** What stops a shot now (None = it can fire). Checked by the predicting client and again by the server. */
	EVaesenFireBlock GetFireBlock() const;

	/** The cone half-angle now: hip or aimed, wider when moving. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetCurrentSpread(bool bAiming) const;

	/** Direction with a random spread inside a cone of SpreadDegrees. */
	static FVector ApplySpread(const FVector& Direction, float SpreadDegrees, FRandomStream& Random);

	/** Where shots start: the Muzzle socket, or the front of the mesh. */
	FVector GetMuzzleLocation() const;

	/** A bolt or lever must be worked before the next shot: the server's word, or (on the shooter's machine) a shot it just predicted. */
	bool NeedsCycle() const;

	/** The shooter's machine worked the action; it no longer predicts a spent case in the chamber. */
	void ClearPredictedCycle() { bPredictedNeedsCycle = false; }

	/** Bows: how far the string is drawn (0..1), replicated for other players' bows. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetDrawAlpha() const { return DrawAlpha / 255.0f; }

	// --- Server ---

	/** Fires along the aim (from the player's eyes). DrawStrength scales bows. Returns whether a shot left the barrel. */
	bool ServerFire(const FVector& AimOrigin, const FVector& AimDirection, float DrawStrength = 1.0f);

	/** Loads up to MaxRounds from the owner's inventory (pouches included). Returns how many went in. */
	int32 ServerLoadRounds(int32 MaxRounds);

	/** The bolt or lever was worked: the chamber is ready. */
	void ServerFinishCycle();

	void ServerSetDrawAlpha(float Alpha);

	// --- Cosmetics (this machine) ---

	/** Flash, smoke, sound, trigger finger; casing too if it ejects on fire and the fire montage has no notify. */
	void PlayFireCosmetics(UAnimMontage* FireMontage);

	/** The click of an empty or unready gun (this machine): the trigger pulled for nothing, and why, for the HUD. */
	void PlayDryFire(EVaesenFireBlock Why);

	/** The last dry fire here: when (real seconds) and why. False if there was none. */
	bool GetLastDryFire(double& OutTime, EVaesenFireBlock& OutWhy) const;

	/** From AN_EjectCasing: ejects only if the weapon's setting matches this moment. Count 0 = the spent rounds. */
	void EjectCasingsAt(EVaesenCasingMoment Moment, int32 Count);

	/** The code's casing for a moment: only when the montage played for it has no AN_EjectCasing (and the setting matches). */
	void EjectCasingsUnlessNotified(EVaesenCasingMoment Moment, const UAnimMontage* Montage, int32 Count = 1);

	/** Other machines: a cycle or reload happened (for their casings and sounds). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastActionCosmetics(EVaesenCasingMoment Moment, int32 Count);

	/** How many spent rounds a reload would throw out now (revolver emptied, breech opened). */
	int32 GetSpentRounds() const;

protected:
	/** Other machines play the shot (the shooter's own already has). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFired(FVector_NetQuantize TraceEnd);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastMisfire();

	/** A shot was fired (on every machine but a dedicated server); for Blueprint extras. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnFired();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnDrawAlphaChanged(float Alpha);

	virtual void Tick(float DeltaSeconds) override;
	virtual bool WantsTick() const override;

	/** A bow's arrow in the drawing hand (the item's NockedArrowMesh), shown and placed by the animation's BowArrow curves. Cosmetic. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> NockedArrow;

	/** A bow whose string draws (the item's StringBowMesh), its bow bone at the grip; String pulled by the BowString curves. */
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	TObjectPtr<UPoseableMeshComponent> StringBow;

	virtual void ApplyWeaponMesh() override;

private:
	double LastDryFireTime = -100.0;
	EVaesenFireBlock LastDryFireBlock = EVaesenFireBlock::None;
	void SpawnCasing();

	/** The point the shot heads for: what the aim from the eyes hits, else far along it. */
	FVector FindAimPoint(const FVector& AimOrigin, const FVector& AimDirection, float Range) const;

	bool IsOwnersMachine() const;

	UFUNCTION()
	void OnRep_DrawAlpha();

	UPROPERTY(ReplicatedUsing = OnRep_DrawAlpha)
	uint8 DrawAlpha = 0;

	mutable bool bPredictedNeedsCycle = false;

	/** When it last fired (server: for the fire interval; owner: for prediction). */
	double LastFireTime = -1000.0;

	/** The nocked arrow follows the drawing hand by the animation's curves (or hides). */
	void UpdateNockedArrow();

	/** The drawing bow's String: at rest, or pulled by the BowString curves (in the hand). */
	void UpdateString(bool bInHandNow);

	/** The string bow's bow bone and its String at rest (component space of the bow mesh, its reference pose). */
	FTransform BowBoneRef;
	FVector StringRest = FVector::ZeroVector;

	/** A bow's arrow has just been loosed: the one in the hand stays hidden until the animation hides it too. */
	bool bArrowLoosed = false;
};
