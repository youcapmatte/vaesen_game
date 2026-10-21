#include "Weapons/VaesenThrownItem.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenPickup.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "TimerManager.h"

AVaesenThrownItem::AVaesenThrownItem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.0f);
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

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.25f;
	Movement->Friction = 0.6f;
	Movement->BounceVelocityStopSimulatingThreshold = 40.0f;
	Movement->ProjectileGravityScale = 1.0f;
}

void AVaesenThrownItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AVaesenThrownItem, ItemClass, COND_InitialOnly);
}

void AVaesenThrownItem::InitThrow(UVaesenItem* InItem, const FVector& Velocity)
{
	Item = InItem;
	ItemClass = InItem ? InItem->GetClass() : nullptr;
	Movement->Velocity = Velocity;
	Movement->InitialSpeed = Velocity.Size();
	Movement->MaxSpeed = FMath::Max(Velocity.Size() * 1.5f, 1.0f);
	const UVaesenItem* Defaults = InItem ? InItem->GetClass()->GetDefaultObject<UVaesenItem>() : nullptr;
	Movement->bRotationFollowsVelocity = Defaults && Defaults->Throwing.Style == EVaesenThrowStyle::PointFirst;
	OnRep_ItemClass();
}

void AVaesenThrownItem::BeginPlay()
{
	Super::BeginPlay();
	LastDebugLocation = GetActorLocation();
	if (AActor* Thrower = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(Thrower, true);
	}
	OnRep_ItemClass();

	if (HasAuthority())
	{
		Collision->OnComponentHit.AddDynamic(this, &AVaesenThrownItem::OnThrownHit);
		Movement->OnProjectileStop.AddDynamic(this, &AVaesenThrownItem::OnStopped);
		GetWorldTimerManager().SetTimer(LandTimer, this, &AVaesenThrownItem::Land, MaxFlightTime, false);
	}
}

void AVaesenThrownItem::OnRep_ItemClass()
{
	const UVaesenItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenItem>() : nullptr;
	Mesh->SetStaticMesh(Defaults ? Defaults->PickupMesh.Get() : nullptr);
	Mesh->SetRelativeScale3D(Defaults ? Defaults->PickupScale : FVector::OneVector);
	if (Defaults && Defaults->PickupMesh)
	{
		Collision->SetSphereRadius(FMath::Clamp(Defaults->PickupMesh->GetBounds().SphereRadius * Defaults->PickupScale.GetMax() * 0.5f, 3.0f, 30.0f));
	}
}

void AVaesenThrownItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bHasHitSomething || bLanded)
	{
		return;
	}

	// Spin the look (not the collision) the way the style throws it.
	const UVaesenItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenItem>() : nullptr;
	if (Defaults && Defaults->Throwing.Style != EVaesenThrowStyle::PointFirst)
	{
		const float Degrees = Defaults->Throwing.SpinRate * 360.0f * DeltaSeconds;
		const FRotator Spin = Defaults->Throwing.Style == EVaesenThrowStyle::Spinning ? FRotator(0.0f, Degrees, 0.0f)
			: Defaults->Throwing.Style == EVaesenThrowStyle::Overhand ? FRotator(-Degrees, 0.0f, 0.0f)
			: FRotator(-Degrees * 0.3f, Degrees * 0.2f, 0.0f);
		Mesh->AddLocalRotation(Spin);
	}

	if (UVaesenCombatLibrary::IsCombatDebugEnabled())
	{
		DrawDebugLine(GetWorld(), LastDebugLocation, GetActorLocation(), FColor::Cyan, false, 4.0f, 0, 1.0f);
	}
	LastDebugLocation = GetActorLocation();
}

void AVaesenThrownItem::OnThrownHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHasHitSomething || bLanded || OtherActor == GetOwner())
	{
		return;
	}
	bHasHitSomething = true;
	UE_LOG(LogVaesenCombat, Log, TEXT("Thrown %s hit %s at %s"), *GetNameSafe(ItemClass), *GetNameSafe(OtherActor), *Hit.ImpactPoint.ToCompactString());

	const UVaesenItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenItem>() : nullptr;
	if (!Defaults)
	{
		return;
	}
	if (!UVaesenCombatLibrary::ApplyDamagePacket(GetOwner(), OtherActor, Defaults->Throwing.Damage, Hit, Item, true, false))
	{
		UVaesenCombatLibrary::PlayImpactEffects(this, UVaesenCombatLibrary::GetMainDamageType(Defaults->Throwing.Damage), Hit);
	}

	if (Defaults->Throwing.bBreaksOnImpact)
	{
		MulticastBreak(Hit.ImpactPoint);
		bLanded = true;
		Destroy();
		return;
	}
	// Knocked down after hitting something: it falls from here.
	Movement->Velocity *= 0.3f;
}

void AVaesenThrownItem::OnStopped(const FHitResult& ImpactResult)
{
	Land();
}

void AVaesenThrownItem::Land()
{
	if (bLanded || !HasAuthority())
	{
		return;
	}
	bLanded = true;
	GetWorldTimerManager().ClearTimer(LandTimer);
	UE_LOG(LogVaesenCombat, Log, TEXT("Thrown %s landed at %s"), *GetNameSafe(ItemClass), *GetActorLocation().ToCompactString());

	if (Item)
	{
		const AVaesenPlayerCharacter* Thrower = Cast<AVaesenPlayerCharacter>(GetOwner());
		const UVaesenInventoryComponent* Inventory = Thrower ? Thrower->GetInventory() : nullptr;
		const TSubclassOf<AVaesenPickup> PickupClass = Inventory && Inventory->DropPickupClass ? Inventory->DropPickupClass : TSubclassOf<AVaesenPickup>(AVaesenPickup::StaticClass());
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const FTransform Transform(Mesh->GetComponentRotation(), GetActorLocation() + FVector(0.0, 0.0, 5.0));
		if (AVaesenPickup* Pickup = GetWorld()->SpawnActor<AVaesenPickup>(PickupClass, Transform, Params))
		{
			Pickup->InitializePickupFromItem(Item, Item->GetQuantity());
			// Settle onto whatever is below with physics, like a dropped item.
			Pickup->Fall(FVector::ZeroVector);
		}
	}
	Destroy();
}

void AVaesenThrownItem::MulticastBreak_Implementation(FVector_NetQuantize Location)
{
	const UVaesenItem* Defaults = ItemClass ? ItemClass->GetDefaultObject<UVaesenItem>() : nullptr;
	if (!Defaults || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Defaults->Throwing.BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Defaults->Throwing.BreakSound, Location);
	}
	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Defaults->Throwing.BreakEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Niagara, Location);
	}
	else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Defaults->Throwing.BreakEffect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(this, Cascade, Location);
	}
}
