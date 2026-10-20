#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenCasing.generated.h"

class UStaticMeshComponent;

/**
 * A spent casing: cosmetic only, never replicated (each machine spawns its own). Tossed out of the Eject socket with
 * a spin, it bounces (one bounce sound), settles and disappears after Lifetime. At most MaxCasings exist per world;
 * the oldest goes first.
 */
UCLASS()
class VAESENGAME_API AVaesenCasing : public AActor
{
	GENERATED_BODY()

public:
	AVaesenCasing();

	/** Spawns one (Mesh null = a small grey-box cylinder). Null on a dedicated server. */
	static AVaesenCasing* Spawn(UWorld* World, UStaticMesh* Mesh, const FTransform& Transform, const FVector& Velocity, USoundBase* BounceSound);

	static constexpr int32 MaxCasings = 40;
	static constexpr float Lifetime = 20.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Casing")
	TObjectPtr<UStaticMeshComponent> Mesh;

private:
	UFUNCTION()
	void OnBounce(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> BounceSound;

	bool bBounced = false;
};
