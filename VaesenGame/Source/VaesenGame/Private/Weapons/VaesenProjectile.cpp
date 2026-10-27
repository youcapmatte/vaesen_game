#include "Weapons/VaesenProjectile.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenPickup.h"
#include "GameFramework/PlayerController.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Weapons/VaesenWeaponData.h"

AVaesenProjectile::AVaesenProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(2.0f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	Collision->bReturnMaterialOnMove = true;
	Collision->SetCanEverAffectNavigation(false);
	SetRootComponent(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);

	Streak = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Streak"));
	Streak->SetupAttachment(Collision);
	Streak->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Streak->SetCanEverAffectNavigation(false);
	Streak->SetCastShadow(false);
	Streak->SetVisibility(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->bInitialVelocityInLocalSpace = false;
}

void AVaesenProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AVaesenProjectile, Ammo, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AVaesenProjectile, TrailEffect, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AVaesenProjectile, FlybySound, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(AVaesenProjectile, StickSound, COND_InitialOnly);
}

void AVaesenProjectile::InitProjectile(TSubclassOf<UVaesenAmmoItem> InAmmo, const FVaesenDamagePacket& InDamage, UVaesenRangedWeaponItem* InWeapon, const FVector& Velocity, UFXSystemAsset* Trail)
{
	Ammo = InAmmo;
	Damage = InDamage;
	Weapon = InWeapon;
	TrailEffect = Trail;
	FlybySound = InWeapon ? InWeapon->GetSound(EVaesenWeaponSound::Flyby) : nullptr;
	StickSound = InWeapon ? InWeapon->GetSound(EVaesenWeaponSound::ArrowStick) : nullptr;
	Movement->Velocity = Velocity;
	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = FMath::Max(Velocity.Size(), 1.0f);
	OnRep_Ammo();
}

void AVaesenProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(MaxFlightTime);
	LastDebugLocation = GetActorLocation();
	LaunchLocation = GetActorLocation();
	if (AActor* Shooter = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(Shooter, true);
	}
	OnRep_Ammo();

	if (UNiagaraSystem* Trail = Cast<UNiagaraSystem>(TrailEffect); Trail && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(Trail, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
}

void AVaesenProjectile::OnRep_Ammo()
{
	const UVaesenAmmoItem* AmmoDefaults = Ammo ? Ammo->GetDefaultObject<UVaesenAmmoItem>() : nullptr;
	UStaticMesh* Shown = AmmoDefaults && AmmoDefaults->ProjectileMesh ? AmmoDefaults->ProjectileMesh.Get() : LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Mesh->SetStaticMesh(Shown);
	FTransform Offset = AmmoDefaults ? AmmoDefaults->ProjectileMeshOffset : FTransform::Identity;
	Offset.SetScale3D(AmmoDefaults ? AmmoDefaults->ProjectileScale : FVector(0.05));
	Mesh->SetRelativeTransform(Offset);
	Movement->ProjectileGravityScale = AmmoDefaults ? AmmoDefaults->GravityScale : 1.0f;

	// The streak: a thin rod (the engine's cylinder, 100 tall along Z, 50 round) laid along -X behind it.
	const bool bStreak = AmmoDefaults && AmmoDefaults->StreakMaterial && AmmoDefaults->StreakLength > 0.0f && GetNetMode() != NM_DedicatedServer;
	if (bStreak && !Streak->GetStaticMesh())
	{
		Streak->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
		Streak->SetMaterial(0, AmmoDefaults->StreakMaterial);
	}
	Streak->SetVisibility(false);
	UpdateStreak();

	if (HasAuthority() && !Collision->OnComponentHit.IsAlreadyBound(this, &AVaesenProjectile::OnProjectileHit))
	{
		Collision->OnComponentHit.AddDynamic(this, &AVaesenProjectile::OnProjectileHit);
	}
}

void AVaesenProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The flight path, on every machine (each draws its own).
	if (UVaesenCombatLibrary::IsCombatDebugEnabled() && !bHasHit)
	{
		DrawDebugLine(GetWorld(), LastDebugLocation, GetActorLocation(), FColor::Yellow, false, 4.0f, 0, 1.0f);
	}
	UpdateStreak();
	UpdateFlyby();
	LastDebugLocation = GetActorLocation();
}

void AVaesenProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHit || !HasAuthority() || OtherActor == GetOwner())
	{
		return;
	}
	bHasHit = true;
	UE_LOG(LogVaesenCombat, Log, TEXT("Projectile %s hit %s at %s"), *GetNameSafe(Ammo), *GetNameSafe(OtherActor), *Hit.ImpactPoint.ToCompactString());

	const bool bDamaged = UVaesenCombatLibrary::ApplyDamagePacket(GetOwner(), OtherActor, Damage, Hit, Weapon, true, true);
	if (!bDamaged)
	{
		MulticastImpact(Hit);
	}

	const UVaesenAmmoItem* AmmoDefaults = Ammo ? Ammo->GetDefaultObject<UVaesenAmmoItem>() : nullptr;
	if (!AmmoDefaults || !AmmoDefaults->bSticksInSurfaces)
	{
		Destroy();
		return;
	}

	// Stuck in what it hit, pointing the way it flew.
	const FRotator Rotation = Movement->Velocity.IsNearlyZero() ? GetActorRotation() : Movement->Velocity.Rotation();
	MulticastStuck(Hit.ImpactPoint, Rotation, Hit);

	if (AmmoDefaults->bRecoverable)
	{
		// It becomes a pickup where it stuck (it doesn't fall); picking it up puts the arrow back in the inventory.
		const AVaesenPlayerCharacter* Shooter = Cast<AVaesenPlayerCharacter>(GetOwner());
		const UVaesenInventoryComponent* Inventory = Shooter ? Shooter->GetInventory() : nullptr;
		const TSubclassOf<AVaesenPickup> PickupClass = Inventory && Inventory->DropPickupClass ? Inventory->DropPickupClass : TSubclassOf<AVaesenPickup>(AVaesenPickup::StaticClass());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The pickup shows the arrow as it flew when it's the same mesh (placed as on the projectile, its point a little
		// into what it hit); otherwise the pickup's own mesh at the point.
		FTransform StuckAt(Rotation, Hit.ImpactPoint);
		if (AmmoDefaults->PickupMesh && AmmoDefaults->PickupMesh == AmmoDefaults->ProjectileMesh)
		{
			FTransform Offset = AmmoDefaults->ProjectileMeshOffset;
			Offset.SetScale3D(AmmoDefaults->ProjectileScale);
			StuckAt = Offset * FTransform(Rotation, Hit.ImpactPoint + Rotation.Vector() * 6.0);
		}
		if (AVaesenPickup* Pickup = GetWorld()->SpawnActor<AVaesenPickup>(PickupClass, StuckAt, Params))
		{
			Pickup->InitializePickup(Ammo, 1);
			if (AActor* StuckIn = Hit.GetActor(); StuckIn && StuckIn->IsRootComponentMovable())
			{
				Pickup->AttachToActor(StuckIn, FAttachmentTransformRules::KeepWorldTransform);
			}
		}
		Destroy();
		return;
	}
	SetLifeSpan(StuckLifetime);
}

void AVaesenProjectile::MulticastStuck_Implementation(FVector_NetQuantize Location, FRotator Rotation, const FHitResult& Hit)
{
	bHasHit = true;
	Streak->SetVisibility(false);
	Movement->StopMovementImmediately();
	Movement->ProjectileGravityScale = 0.0f;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorLocationAndRotation(Location, Rotation);
	if (USceneComponent* StuckIn = Hit.GetComponent())
	{
		AttachToComponent(StuckIn, FAttachmentTransformRules::KeepWorldTransform, Hit.BoneName);
	}

	const UVaesenAmmoItem* AmmoDefaults = Ammo ? Ammo->GetDefaultObject<UVaesenAmmoItem>() : nullptr;
	const FGameplayTag DamageType = AmmoDefaults ? UVaesenCombatLibrary::GetMainDamageType(AmmoDefaults->Damage) : FGameplayTag();
	UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit, EVaesenImpactDelivery::Shot);
	if (USoundBase* Sound = StickSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location);
	}
}

void AVaesenProjectile::UpdateStreak()
{
	const UVaesenAmmoItem* AmmoDefaults = Ammo ? Ammo->GetDefaultObject<UVaesenAmmoItem>() : nullptr;
	if (!AmmoDefaults || bHasHit || !Streak->GetStaticMesh())
	{
		return;
	}
	// Behind the arrow, as long as the flight so far allows (it grows out from the bow rather than reaching back past it).
	const float Length = FMath::Clamp(static_cast<float>(FVector::Dist(LaunchLocation, GetActorLocation())) - AmmoDefaults->StreakBehind, 0.0f, AmmoDefaults->StreakLength);
	if (Length < 1.0f)
	{
		Streak->SetVisibility(false);
		return;
	}
	const float Width = AmmoDefaults->StreakWidth / 100.0f;
	// Its Z (the cylinder's length, the bright end up) along +X: toward the arrow.
	Streak->SetRelativeTransform(FTransform(FRotator(-90.0, 0.0, 0.0), FVector(-(AmmoDefaults->StreakBehind + Length * 0.5f), 0.0, 0.0),
		FVector(Width, Width, Length / 100.0f)));
	Streak->SetVisibility(true);
}

void AVaesenProjectile::MulticastImpact_Implementation(const FHitResult& Hit)
{
	const UVaesenAmmoItem* AmmoDefaults = Ammo ? Ammo->GetDefaultObject<UVaesenAmmoItem>() : nullptr;
	const FGameplayTag DamageType = AmmoDefaults ? UVaesenCombatLibrary::GetMainDamageType(AmmoDefaults->Damage) : FGameplayTag();
	UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit, EVaesenImpactDelivery::Shot);
}

void AVaesenProjectile::UpdateFlyby()
{
	if (bFlybyPlayed || bHasHit || !FlybySound || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	const APawn* Listener = Local ? Local->GetPawn() : nullptr;
	if (!Listener || Listener == GetOwner())
	{
		return;
	}
	// Close (within 3 m of the listener's head) and just past them.
	const FVector Head = Listener->GetPawnViewLocation();
	const FVector ToHead = Head - GetActorLocation();
	const FVector Velocity = Movement->Velocity;
	if (ToHead.SizeSquared() < FMath::Square(300.0f) && FVector::DotProduct(ToHead, Velocity) <= 0.0f)
	{
		bFlybyPlayed = true;
		UGameplayStatics::PlaySoundAtLocation(this, FlybySound, GetActorLocation());
	}
}
