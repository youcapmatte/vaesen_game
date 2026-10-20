#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenThrownItem.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UVaesenItem;

/**
 * An item in flight after G: spawned by the server with a copy of the item, replicated. It spins as its throw style
 * says, deals the item's throw damage to the first thing it hits, then bounces and tumbles to a stop and becomes a
 * normal pickup there (with that same item: a thrown pouch keeps its contents). Items that break on impact (a
 * bottle) shatter instead.
 */
UCLASS()
class VAESENGAME_API AVaesenThrownItem : public AActor
{
	GENERATED_BODY()

public:
	AVaesenThrownItem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before it finishes spawning: takes ownership of Item (a copy made for the throw). */
	void InitThrow(UVaesenItem* Item, const FVector& Velocity);

	/** Still flying after this long: it lands where it is. */
	UPROPERTY(EditDefaultsOnly, Category = "Throw", meta = (Units = "s"))
	float MaxFlightTime = 8.0f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throw")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throw")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Throw")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void OnThrownHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnStopped(const FHitResult& ImpactResult);

	/** Server: becomes a pickup where it is. */
	void Land();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreak(FVector_NetQuantize Location);

	UFUNCTION()
	void OnRep_ItemClass();

	/** What it is (for the look on clients). */
	UPROPERTY(ReplicatedUsing = OnRep_ItemClass)
	TSubclassOf<UVaesenItem> ItemClass;

	/** The item itself (server only), handed to the pickup when it lands. */
	UPROPERTY(Transient)
	TObjectPtr<UVaesenItem> Item;

	bool bHasHitSomething = false;
	bool bLanded = false;
	FTimerHandle LandTimer;
	FVector LastDebugLocation = FVector::ZeroVector;
};
