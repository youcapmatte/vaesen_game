#pragma once

#include "CoreMinimal.h"
#include "Combat/VaesenDamageTypes.h"
#include "GameFramework/Actor.h"
#include "VaesenProjectile.generated.h"

class UFXSystemAsset;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UVaesenAmmoItem;
class UVaesenRangedWeaponItem;

/**
 * A shot with travel time and drop (Projectile weapons): spawned by the server, replicated, moved by a projectile
 * movement component with the ammo's gravity. It deals the ammo's damage packet on the first thing it hits (server).
 * Ammo that sticks (arrows) stays in what it hit; recoverable ammo then becomes a pickup there.
 */
UCLASS()
class VAESENGAME_API AVaesenProjectile : public AActor
{
	GENERATED_BODY()

public:
	AVaesenProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before it finishes spawning. */
	void InitProjectile(TSubclassOf<UVaesenAmmoItem> InAmmo, const FVaesenDamagePacket& InDamage, UVaesenRangedWeaponItem* InWeapon, const FVector& Velocity, UFXSystemAsset* Trail);

	/** Gone this far without hitting anything: gone. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (Units = "s"))
	float MaxFlightTime = 10.0f;

	/** Stuck arrows that aren't picked up stay this long. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (Units = "s"))
	float StuckLifetime = 60.0f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStuck(FVector_NetQuantize Location, FRotator Rotation, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastImpact(const FHitResult& Hit);

	UFUNCTION()
	void OnRep_Ammo();

	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	TSubclassOf<UVaesenAmmoItem> Ammo;

	UPROPERTY(Replicated)
	TObjectPtr<UFXSystemAsset> TrailEffect;

	FVaesenDamagePacket Damage;

	UPROPERTY(Transient)
	TObjectPtr<UVaesenRangedWeaponItem> Weapon;

	bool bHasHit = false;
	FVector LastDebugLocation = FVector::ZeroVector;
};
