#include "Weapons/VaesenCasing.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace VaesenCasings
{
	/** The casings alive per world, oldest first (for the cap). */
	TMap<TWeakObjectPtr<UWorld>, TArray<TWeakObjectPtr<AVaesenCasing>>> Alive;
}

AVaesenCasing::AVaesenCasing()
{
	bReplicates = false;
	InitialLifeSpan = Lifetime;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Bounces off the world, not off people (or the camera).
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->CastShadow = false;
}

AVaesenCasing* AVaesenCasing::Spawn(UWorld* World, UStaticMesh* CasingMesh, const FTransform& Transform, const FVector& Velocity, USoundBase* Sound)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<AVaesenCasing>>& List = VaesenCasings::Alive.FindOrAdd(World);
	List.RemoveAll([](const TWeakObjectPtr<AVaesenCasing>& Each) { return !Each.IsValid(); });
	while (List.Num() >= MaxCasings)
	{
		if (AVaesenCasing* Oldest = List[0].Get())
		{
			Oldest->Destroy();
		}
		List.RemoveAt(0);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVaesenCasing* Casing = World->SpawnActor<AVaesenCasing>(AVaesenCasing::StaticClass(), Transform, Params);
	if (!Casing)
	{
		return nullptr;
	}

	UStaticMesh* Shown = CasingMesh ? CasingMesh : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Casing->Mesh->SetStaticMesh(Shown);
	if (!CasingMesh)
	{
		// A cartridge case is about 1 x 1 x 4 cm; the basic cylinder is a metre.
		Casing->Mesh->SetWorldScale3D(FVector(0.012, 0.012, 0.04));
	}
	Casing->BounceSound = Sound;
	Casing->Mesh->SetPhysicsLinearVelocity(Velocity);
	Casing->Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-1440.0f, 1440.0f)));
	Casing->Mesh->OnComponentHit.AddDynamic(Casing, &AVaesenCasing::OnBounce);
	List.Add(Casing);
	return Casing;
}

void AVaesenCasing::OnBounce(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bBounced)
	{
		return;
	}
	bBounced = true;
	if (BounceSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BounceSound, GetActorLocation());
	}
}
