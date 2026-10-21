#include "Weapons/VaesenRangedWeaponActor.h"

#include "Anim/VaesenAnimInstance.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Weapons/VaesenCasing.h"
#include "Weapons/VaesenProjectile.h"

namespace VaesenRanged
{
	const FName Muzzle(TEXT("Muzzle"));
	const FName Eject(TEXT("Eject"));

	/** How far from the shooter's eyes the server accepts an aim origin (lag, camera boom). */
	constexpr float MaxAimOriginDistance = 600.0f;

	void SpawnEffectAttached(UFXSystemAsset* Effect, USceneComponent* AttachTo, FName Socket, float Scale)
	{
		if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Effect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(Niagara, AttachTo, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
				FVector(Scale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::None);
		}
		else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Effect))
		{
			UGameplayStatics::SpawnEmitterAttached(Cascade, AttachTo, Socket, FVector::ZeroVector, FRotator::ZeroRotator, FVector(Scale));
		}
	}

	void SpawnEffectAt(UWorld* World, UFXSystemAsset* Effect, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Effect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Niagara, Location, Rotation, FVector(Scale));
		}
		else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Effect))
		{
			UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Location, Rotation, FVector(Scale));
		}
	}
}

AVaesenRangedWeaponActor::AVaesenRangedWeaponActor()
{
}

void AVaesenRangedWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AVaesenRangedWeaponActor, DrawAlpha, COND_SkipOwner);
}

UVaesenRangedWeaponItem* AVaesenRangedWeaponActor::GetRangedItem() const
{
	return Cast<UVaesenRangedWeaponItem>(GetWeaponItem());
}

bool AVaesenRangedWeaponActor::IsOwnersMachine() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled();
}

bool AVaesenRangedWeaponActor::NeedsCycle() const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	// The prediction only bridges the wait for the server: once it has had time to answer, its word stands.
	if (bPredictedNeedsCycle && Item && GetWorld()->GetTimeSeconds() - LastFireTime > Item->CycleTime + 1.0)
	{
		bPredictedNeedsCycle = false;
	}
	return Item && (Item->NeedsCycle() || bPredictedNeedsCycle);
}

EVaesenFireBlock AVaesenRangedWeaponActor::GetFireBlock() const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item)
	{
		return EVaesenFireBlock::NoWeapon;
	}
	if (Item->IsBroken())
	{
		return EVaesenFireBlock::Broken;
	}
	if (Item->IsBow())
	{
		// Bows nock straight from the inventory.
		const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
		if (!Player || Item->CountAmmoIn(Player->GetInventory()) <= 0)
		{
			return EVaesenFireBlock::Empty;
		}
	}
	else if (NeedsCycle())
	{
		return EVaesenFireBlock::NeedsCycle;
	}
	else if (Item->GetLoadedRounds() <= 0)
	{
		return EVaesenFireBlock::Empty;
	}
	// A little slack for the client's clock being ahead of the server's.
	const double Slack = HasAuthority() && !IsOwnersMachine() ? 0.05 : 0.0;
	if (GetWorld()->GetTimeSeconds() - LastFireTime < Item->FireInterval - Slack)
	{
		return EVaesenFireBlock::TooSoon;
	}
	return EVaesenFireBlock::None;
}

float AVaesenRangedWeaponActor::GetCurrentSpread(bool bAiming) const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item)
	{
		return 0.0f;
	}
	float Spread = bAiming ? Item->SpreadAimed : Item->SpreadHip;
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const float MaxSpeed = FMath::Max(Character->GetCharacterMovement()->MaxWalkSpeed, 1.0f);
		Spread += Item->SpreadMoving * FMath::Clamp(Character->GetVelocity().Size2D() / MaxSpeed, 0.0f, 1.0f);
		if (Character->GetCharacterMovement()->IsFalling())
		{
			Spread += Item->SpreadMoving;
		}
	}
	return Spread;
}

FVector AVaesenRangedWeaponActor::ApplySpread(const FVector& Direction, float SpreadDegrees, FRandomStream& Random)
{
	if (SpreadDegrees <= 0.0f)
	{
		return Direction.GetSafeNormal();
	}
	return Random.VRandCone(Direction.GetSafeNormal(), FMath::DegreesToRadians(SpreadDegrees));
}

FVector AVaesenRangedWeaponActor::GetMuzzleLocation() const
{
	FTransform Muzzle;
	if (GetSocketTransform(VaesenRanged::Muzzle, Muzzle))
	{
		return Muzzle.GetLocation();
	}
	// No Muzzle socket: the far end of the mesh along where the weapon points.
	if (const UMeshComponent* Mesh = GetWeaponMesh())
	{
		return GetActorLocation() + GetActorForwardVector() * Mesh->Bounds.SphereRadius;
	}
	return GetActorLocation();
}

FVector AVaesenRangedWeaponActor::FindAimPoint(const FVector& AimOrigin, const FVector& AimDirection, float Range) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenAim), true, this);
	Params.AddIgnoredActor(GetOwner());
	const FVector End = AimOrigin + AimDirection * Range;
	return GetWorld()->LineTraceSingleByChannel(Hit, AimOrigin, End, ECC_Visibility, Params) ? Hit.ImpactPoint : End;
}

bool AVaesenRangedWeaponActor::ServerFire(const FVector& AimOrigin, const FVector& AimDirection, float DrawStrength)
{
	UVaesenRangedWeaponItem* Item = GetRangedItem();
	AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	AActor* Shooter = GetOwner();
	if (!HasAuthority() || !Item || !Shooter || GetFireBlock() != EVaesenFireBlock::None)
	{
		return false;
	}

	// The aim must start near the shooter's eyes (the camera can be a few metres behind in third person).
	FVector EyeLocation;
	FRotator EyeRotation;
	Shooter->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	const FVector Direction = AimDirection.GetSafeNormal();
	if (Direction.IsNearlyZero() || FVector::Dist(AimOrigin, EyeLocation) > VaesenRanged::MaxAimOriginDistance)
	{
		UE_LOG(LogVaesenCombat, Warning, TEXT("%s: implausible aim from %s, shot refused."), *GetNameSafe(Shooter), *AimOrigin.ToString());
		return false;
	}

	// Take the round: from the magazine, or (bows) an arrow from the inventory.
	TSubclassOf<UVaesenAmmoItem> AmmoClass;
	if (Item->IsBow())
	{
		AmmoClass = Item->FindAmmoIn(Player ? Player->GetInventory() : nullptr);
		if (!AmmoClass || Player->GetInventory()->RemoveItemsOfClass(AmmoClass, 1) <= 0)
		{
			return false;
		}
	}
	else
	{
		AmmoClass = Item->GetLoadedAmmo();
		Item->SetLoadedRounds(Item->GetLoadedRounds() - 1, AmmoClass);
		if (Item->HasCycleStep())
		{
			Item->SetNeedsCycle(true);
		}
	}
	LastFireTime = GetWorld()->GetTimeSeconds();
	Item->ApplyWear(Item->WearPerShot);

	// An old gun can fail: the round is spent, nothing leaves the barrel.
	if (Item->MisfireChance > 0.0f && FMath::FRand() < Item->MisfireChance)
	{
		MulticastMisfire();
		return false;
	}

	const UVaesenAmmoItem* Ammo = AmmoClass ? AmmoClass->GetDefaultObject<UVaesenAmmoItem>() : GetDefault<UVaesenAmmoItem>();
	const float Strength = FMath::Clamp(DrawStrength, 0.0f, 1.0f) * (Item->IsBow() || Item->Category == EVaesenWeaponCategory::Crossbow ? Item->MaxDrawStrength : 1.0f);
	const FVaesenDamagePacket Damage = Ammo->GetDamage().Scaled(Item->DamageMultiplier * (Item->IsBow() ? Strength : 1.0f));
	const FVector Muzzle = GetMuzzleLocation();
	const bool bDebug = UVaesenCombatLibrary::IsCombatDebugEnabled();
	FVector TraceEnd = Muzzle + Direction * Item->MaxRange;

	if (Item->ShotType == EVaesenShotType::InstantHit)
	{
		// What the aim hits, then whether the line from the muzzle to it is clear.
		const FVector AimPoint = FindAimPoint(AimOrigin, Direction, Item->MaxRange);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenShot), true, this);
		Params.AddIgnoredActor(Shooter);
		Params.bReturnPhysicalMaterial = true;
		const FVector ShotDirection = (AimPoint - Muzzle).GetSafeNormal();
		if (GetWorld()->LineTraceSingleByChannel(Hit, Muzzle, Muzzle + ShotDirection * Item->MaxRange, ECC_Pawn, Params))
		{
			TraceEnd = Hit.ImpactPoint;
			// Full damage out to the effective range, falling to half at the longest.
			const float Distance = FVector::Dist(Muzzle, Hit.ImpactPoint);
			const float Falloff = FMath::GetMappedRangeValueClamped(FVector2D(Item->EffectiveRange, FMath::Max(Item->MaxRange, Item->EffectiveRange + 1.0f)), FVector2D(1.0f, 0.5f), Distance);
			if (!UVaesenCombatLibrary::ApplyDamagePacket(Shooter, Hit.GetActor(), Damage.Scaled(Falloff), Hit, Item, true, true))
			{
				MulticastImpact(Hit, UVaesenCombatLibrary::GetMainDamageType(Damage), true);
			}
		}
		if (bDebug)
		{
			MulticastDebugLine(Muzzle, TraceEnd, FColor::Orange, 3.0f);
		}
	}
	else
	{
		const FVector AimPoint = FindAimPoint(AimOrigin, Direction, 20000.0f);
		const FVector ShotDirection = (AimPoint - Muzzle).GetSafeNormal();
		FActorSpawnParameters Params;
		Params.Owner = Shooter;
		Params.Instigator = Cast<APawn>(Shooter);
		Params.bDeferConstruction = true;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform SpawnTransform(ShotDirection.Rotation(), Muzzle);
		UE_LOG(LogVaesenCombat, Verbose, TEXT("%s fires a projectile from %s toward %s (aim from %s)"), *GetNameSafe(Shooter), *Muzzle.ToCompactString(), *AimPoint.ToCompactString(), *AimOrigin.ToCompactString());
		if (AVaesenProjectile* Projectile = GetWorld()->SpawnActor<AVaesenProjectile>(AVaesenProjectile::StaticClass(), SpawnTransform, Params))
		{
			const float Speed = Item->LaunchSpeed * (Item->IsBow() ? FMath::Lerp(0.35f, 1.0f, FMath::Clamp(DrawStrength, 0.0f, 1.0f)) : 1.0f);
			Projectile->InitProjectile(AmmoClass, Damage, Item, ShotDirection * Speed, Item->Effects.TracerOrTrail);
			Projectile->FinishSpawning(SpawnTransform);
		}
		TraceEnd = AimPoint;
	}

	MulticastFired(TraceEnd);
	return true;
}

int32 AVaesenRangedWeaponActor::ServerLoadRounds(int32 MaxRounds)
{
	UVaesenRangedWeaponItem* Item = GetRangedItem();
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner());
	UVaesenInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!HasAuthority() || !Item || !Inventory || Item->IsBow())
	{
		return 0;
	}

	const TSubclassOf<UVaesenAmmoItem> AmmoClass = Item->FindAmmoIn(Inventory);
	const int32 Room = Item->MagazineSize - Item->GetLoadedRounds();
	const int32 Wanted = FMath::Min(Room, MaxRounds);
	if (!AmmoClass || Wanted <= 0)
	{
		return 0;
	}
	const int32 Taken = Inventory->RemoveItemsOfClass(AmmoClass, Wanted);
	if (Taken > 0)
	{
		Item->SetLoadedRounds(Item->GetLoadedRounds() + Taken, AmmoClass);
		// A fresh round goes straight into the chamber when loading by hand.
		Item->SetNeedsCycle(false);
	}
	return Taken;
}

void AVaesenRangedWeaponActor::ServerFinishCycle()
{
	if (UVaesenRangedWeaponItem* Item = GetRangedItem(); Item && HasAuthority())
	{
		Item->SetNeedsCycle(false);
	}
}

void AVaesenRangedWeaponActor::ServerSetDrawAlpha(float Alpha)
{
	if (HasAuthority())
	{
		DrawAlpha = static_cast<uint8>(FMath::Clamp(Alpha, 0.0f, 1.0f) * 255.0f);
		OnRep_DrawAlpha();
	}
}

void AVaesenRangedWeaponActor::OnRep_DrawAlpha()
{
	OnDrawAlphaChanged(GetDrawAlpha());
}

int32 AVaesenRangedWeaponActor::GetSpentRounds() const
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	return Item ? FMath::Max(0, Item->MagazineSize - Item->GetLoadedRounds()) : 0;
}

// --- Cosmetics ---

void AVaesenRangedWeaponActor::MulticastFired_Implementation(FVector_NetQuantize TraceEnd)
{
	if (IsOwnersMachine())
	{
		return;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	// The fire montage reaches other machines through the ability system's montage replication (the server plays it).
	PlayFireCosmetics(Item ? Item->GetMontage(&FVaesenWeaponMontages::Fire) : nullptr);

	// A bullet passing close to this machine's player (within 3 m of the line, not the shooter) cracks past.
	const APlayerController* Local = GetWorld()->GetFirstPlayerController();
	const APawn* Listener = Local ? Local->GetPawn() : nullptr;
	if (Item && Listener && Listener != GetOwner())
	{
		const FVector Head = Listener->GetPawnViewLocation();
		const FVector Closest = FMath::ClosestPointOnSegment(Head, GetMuzzleLocation(), TraceEnd);
		if (FVector::DistSquared(Closest, Head) < FMath::Square(300.0f) && FVector::DistSquared(Closest, TraceEnd) > FMath::Square(50.0f))
		{
			if (USoundBase* Flyby = Item->GetSound(EVaesenWeaponSound::Flyby))
			{
				UGameplayStatics::PlaySoundAtLocation(this, Flyby, Closest);
			}
		}
	}
}

void AVaesenRangedWeaponActor::MulticastMisfire_Implementation()
{
	PlayWeaponSound(EVaesenWeaponSound::Misfire);
}

void AVaesenRangedWeaponActor::PlayFireCosmetics(UAnimMontage* FireMontage)
{
	LastFireTime = GetWorld()->GetTimeSeconds();
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (Item && Item->HasCycleStep() && IsOwnersMachine() && !HasAuthority())
	{
		bPredictedNeedsCycle = true;
	}
	if (!Item || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UMeshComponent* Mesh = GetWeaponMesh();
	USceneComponent* EffectParent = Mesh ? static_cast<USceneComponent*>(Mesh) : Root.Get();
	const FName MuzzleSocket = Mesh && Mesh->DoesSocketExist(VaesenRanged::Muzzle) ? VaesenRanged::Muzzle : NAME_None;
	VaesenRanged::SpawnEffectAttached(Item->Effects.MuzzleFlash, EffectParent, MuzzleSocket, 1.0f);
	// Smoke stays in the air where the shot was, rather than following the barrel.
	VaesenRanged::SpawnEffectAt(GetWorld(), Item->Effects.MuzzleSmoke, GetMuzzleLocation(), GetActorRotation(), Item->Effects.SmokeScale);

	PlayWeaponSound(EVaesenWeaponSound::Fire);
	PlayWeaponSound(EVaesenWeaponSound::FireDistantTail);
	PlayWeaponSound(EVaesenWeaponSound::GunshotEcho);

	// The trigger finger (an additive or Control Rig curl in the anim Blueprint).
	if (const AVaesenBaseCharacter* Character = GetOwningCharacter())
	{
		if (UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Character->GetMesh()->GetAnimInstance()))
		{
			Anim->PulseTriggerFinger();
		}
	}

	if (Item->CasingEjection == EVaesenCasingEjection::OnFire)
	{
		EjectCasingsUnlessNotified(EVaesenCasingMoment::Fire, FireMontage, 1);
	}
	OnFired();
}

void AVaesenRangedWeaponActor::PlayDryFire() const
{
	PlayWeaponSound(EVaesenWeaponSound::DryFire);
}

void AVaesenRangedWeaponActor::EjectCasingsAt(EVaesenCasingMoment Moment, int32 Count)
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item)
	{
		return;
	}
	const bool bMatches = (Moment == EVaesenCasingMoment::Fire && Item->CasingEjection == EVaesenCasingEjection::OnFire)
		|| (Moment == EVaesenCasingMoment::Cycle && Item->CasingEjection == EVaesenCasingEjection::OnCycle)
		|| (Moment == EVaesenCasingMoment::Reload && Item->CasingEjection == EVaesenCasingEjection::OnReload);
	if (!bMatches)
	{
		return;
	}
	const int32 Casings = Count > 0 ? Count : GetSpentRounds();
	for (int32 Index = 0; Index < Casings; ++Index)
	{
		SpawnCasing();
	}
}

void AVaesenRangedWeaponActor::EjectCasingsUnlessNotified(EVaesenCasingMoment Moment, const UAnimMontage* Montage, int32 Count)
{
	if (!UVaesenCombatLibrary::MontageHasCasingNotify(Montage))
	{
		EjectCasingsAt(Moment, Count);
	}
}

void AVaesenRangedWeaponActor::MulticastActionCosmetics_Implementation(EVaesenCasingMoment Moment, int32 Count)
{
	if (IsOwnersMachine())
	{
		return;
	}
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!Item)
	{
		return;
	}
	const UAnimMontage* Montage = Item->GetMontage(Moment == EVaesenCasingMoment::Cycle ? &FVaesenWeaponMontages::Cycle : &FVaesenWeaponMontages::Reload);
	EjectCasingsUnlessNotified(Moment, Montage, Count);
}

void AVaesenRangedWeaponActor::SpawnCasing()
{
	UWorld* World = GetWorld();
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	if (!World || !Item || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	FTransform Eject;
	if (!GetSocketTransform(VaesenRanged::Eject, Eject))
	{
		Eject = FTransform(GetActorRotation(), GetActorLocation() + GetActorUpVector() * 4.0f);
	}
	// Out to the right and up, spinning.
	const FVector Out = (Eject.GetRotation().GetRightVector() * 1.0f + Eject.GetRotation().GetUpVector() * 0.6f).GetSafeNormal();
	AVaesenCasing::Spawn(World, Item->CasingMesh, Eject, Out * FMath::FRandRange(180.0f, 260.0f), Item->GetSound(EVaesenWeaponSound::CasingBounce));
}
