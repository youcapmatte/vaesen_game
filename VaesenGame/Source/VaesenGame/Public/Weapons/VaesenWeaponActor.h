#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/VaesenWeaponTypes.h"
#include "VaesenWeaponActor.generated.h"

class AVaesenBaseCharacter;
class UMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UVaesenWeaponItem;

/**
 * The weapon in the hand (or the sheath) while its item is equipped. The server spawns it, owned by the character, and
 * it replicates; the item stays the data (durability, loaded rounds), the actor is the thing you see and swing.
 *
 * It holds the item's mesh so the mesh's Grip socket sits at the actor's origin, and attaches that origin to the
 * character's hand socket (hand_r_weapon) or the item's sheath socket. Missing sockets fall back to bones (hand_r,
 * the sheath location's fallback bone), so it works on a skeleton that has none yet.
 *
 * Melee hit detection lives here (any weapon can strike: a sword, a rifle butt): while a hit window is open the server
 * sweeps between the mesh's TraceStart and TraceEnd sockets every tick, filling in between frames so fast swings
 * don't skip, and hits each target at most once per swing. AVaesenRangedWeaponActor adds firing.
 */
UCLASS()
class VAESENGAME_API AVaesenWeaponActor : public AActor
{
	GENERATED_BODY()

public:
	AVaesenWeaponActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before it finishes spawning. */
	void SetWeaponItem(UVaesenWeaponItem* Item);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UVaesenWeaponItem* GetWeaponItem() const { return WeaponItem; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AVaesenBaseCharacter* GetOwningCharacter() const;

	/** Moves it to the hand, or to its sheath socket. */
	void AttachToOwner(bool bInHand);

	/** Where it hangs when sheathed, in the world (its grip there), wherever it is now. */
	FTransform GetSheathedTransform() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsInHand() const { return bInHand; }

	/**
	 * How it sits in the hands: the item's, copied here (editing it on a weapon in play shows at once; the Weapon Fit
	 * rig, AVaesenWeaponFitRig, is the way to tune it and keep it).
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Hold")
	FVaesenWeaponHold Hold;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

	/** The visible mesh (static or skeletal), or null. */
	UMeshComponent* GetWeaponMesh() const;

	/** The skeleton socket on the weapon bone (ik_hand_gun) a right-hand weapon rides on (hand_r_weapon without it). */
	static const FName WeaponHeldSocket;

	/** How a weapon mesh sits under its grip so its Grip socket is at the grip (Scale: the mesh's scale). */
	static FTransform GripAlignment(const UMeshComponent* Mesh, const FVector& Scale);

	/** The scale its item's mesh is shown at (a grey-box stand-in at the pickup size). */
	static FVector HeldMeshScale(const UVaesenWeaponItem* Item);

	/** World transform of a socket on the weapon mesh; false (and the actor's transform) if it has none. */
	bool GetSocketTransform(FName Socket, FTransform& OutTransform) const;

	/** Plays one of its sounds here, on this machine (silent if the slot is empty). */
	void PlayWeaponSound(EVaesenWeaponSound Sound) const;

	/** Plays it on every machine. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySound(EVaesenWeaponSound Sound);

	// --- Melee hit window (server) ---

	/** Opens the hit window for Attack: from now until EndHitWindow every target touched is hit once. */
	void BeginHitWindow(const FVaesenMeleeAttack& Attack);
	void EndHitWindow();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsHitWindowOpen() const { return bHitWindowOpen; }

	/** Where the striking part runs now: TraceStart -> TraceEnd, or along the arm from the grip without them. */
	void GetStrikeLine(FVector& OutStart, FVector& OutEnd) const;

	/** How thick the swept blade is. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee", meta = (ClampMin = "0", Units = "cm"))
	float StrikeRadius = 6.0f;

	/** Most sweeps between two frames (fast swings are filled in so they don't skip thin targets). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Melee", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubSteps = 8;

	/** Impact sound and effect where a hit landed on something without an ability system (a wall), on every machine. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(const FHitResult& Hit, FGameplayTag DamageType, bool bBullet);

	/** A line drawn for vaesen.combat.debug, on every machine (the server's own debug lines don't show on clients). */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDebugLine(FVector_NetQuantize Start, FVector_NetQuantize End, FColor Color, float Duration);

protected:
	virtual void BeginPlay() override;

	/** Puts the item's mesh on, with its Grip at the origin. */
	virtual void ApplyWeaponMesh();

	/** It moved to the hand or the sheath (on every machine). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnAttachedToOwner(bool bNowInHand);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> StaticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

private:
	/** The socket (or fallback bone) and offset it attaches to, in the hand or the sheath. False without an owner. */
	bool ResolveAttachment(bool bToHand, FName& OutSocket, FTransform& OutOffset) const;

	UFUNCTION()
	void OnRep_WeaponItem();

	/** Sweeps from the last strike line to this one (server). */
	void SweepHitWindow();

	/** Server: a target was struck in the hit window. */
	void StrikeTarget(const FHitResult& Hit);

	UPROPERTY(ReplicatedUsing = OnRep_WeaponItem)
	TObjectPtr<UVaesenWeaponItem> WeaponItem;

	bool bInHand = false;

	bool bHitWindowOpen = false;
	FVaesenMeleeAttack ActiveAttack;
	FVector LastStrikeStart = FVector::ZeroVector;
	FVector LastStrikeEnd = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> StruckThisSwing;

	/** Ticks while a hit window is open or it hangs (and swings) in its sheath. */
	void UpdateTickEnabled();

	/** Sheathed at a location that hangs (HangAngle): hangs from the hilt and swings with the body (cosmetic). */
	void UpdateHang(float DeltaSeconds);

	/** Where it's attached now, and how (for the hang). */
	FName AttachSocket;
	FTransform AttachOffset;

	/** Hanging: the location's settings, and the pendulum (the blade's sideways offset and its speed; the hilt's last place and speed). */
	bool bHangs = false;
	float HangAngle = 0.0f;
	float SwingFrequency = 1.3f;
	float SwingDamping = 0.3f;
	float SwingAmount = 1.0f;
	FVector SwingOffset = FVector::ZeroVector;
	FVector SwingVelocity = FVector::ZeroVector;
	FVector LastPivot = FVector::ZeroVector;
	FVector PivotVelocity = FVector::ZeroVector;
	bool bPivotKnown = false;
};
