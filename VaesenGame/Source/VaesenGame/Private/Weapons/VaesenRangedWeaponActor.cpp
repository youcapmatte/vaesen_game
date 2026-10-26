#include "Weapons/VaesenRangedWeaponActor.h"

#include "Anim/VaesenAnimInstance.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/VaesenGameplayTags.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "AnimationRuntime.h"
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
	NockedArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NockedArrow"));
	NockedArrow->SetupAttachment(GetRootComponent());
	NockedArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NockedArrow->SetGenerateOverlapEvents(false);
	NockedArrow->SetHiddenInGame(true);

	StringBow = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("StringBow"));
	StringBow->SetupAttachment(GetRootComponent());
	StringBow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StringBow->SetGenerateOverlapEvents(false);
	StringBow->SetVisibility(false);
}

void AVaesenRangedWeaponActor::ApplyWeaponMesh()
{
	Super::ApplyWeaponMesh();
	// A bow whose string draws: shown in place of the mesh (which stays, hidden, for its sockets), its bow bone at the
	// grip (the actor's origin).
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	USkeletalMesh* Bow = Item ? Item->StringBowMesh.Get() : nullptr;
	const int32 BowBone = Bow ? Bow->GetRefSkeleton().FindBoneIndex(TEXT("bow")) : INDEX_NONE;
	const int32 StringBone = Bow ? Bow->GetRefSkeleton().FindBoneIndex(TEXT("String")) : INDEX_NONE;
	if (BowBone == INDEX_NONE || StringBone == INDEX_NONE)
	{
		StringBow->SetSkinnedAssetAndUpdate(nullptr);
		StringBow->SetVisibility(false);
		return;
	}
	BowBoneRef = FAnimationRuntime::GetComponentSpaceTransformRefPose(Bow->GetRefSkeleton(), BowBone);
	StringRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Bow->GetRefSkeleton(), StringBone).GetLocation();
	StringBow->SetSkinnedAssetAndUpdate(Bow);
	StringBow->SetRelativeTransform(BowBoneRef.Inverse());
	StringBow->SetVisibility(true);
	if (UMeshComponent* Mesh = GetWeaponMesh())
	{
		Mesh->SetVisibility(false);
	}
	UpdateString(false);
}

void AVaesenRangedWeaponActor::UpdateString(bool bInHandNow)
{
	if (!StringBow->GetSkinnedAsset())
	{
		return;
	}
	// The pull, in the bow bone's space (zero at rest, and wherever the animation has no BowString curves).
	FVector Pull = FVector::ZeroVector;
	const AVaesenBaseCharacter* Character = GetOwningCharacter();
	const UAnimInstance* Anim = bInHandNow && Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (Anim)
	{
		Pull = FVector(Anim->GetCurveValue(UVaesenAnimInstance::BowStringCurves[0]), Anim->GetCurveValue(UVaesenAnimInstance::BowStringCurves[1]),
			Anim->GetCurveValue(UVaesenAnimInstance::BowStringCurves[2]));
	}
	StringBow->SetBoneLocationByName(TEXT("String"), StringRest + BowBoneRef.TransformVectorNoScale(Pull), EBoneSpaces::ComponentSpace);
}

bool AVaesenRangedWeaponActor::WantsTick() const
{
	// A bow in the hand: its arrow follows the drawing hand (not on a dedicated server: it's only seen); and until a
	// shown arrow is hidden again.
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	return Super::WantsTick() || (IsInHand() && Item && (Item->NockedArrowMesh || Item->StringBowMesh) && GetNetMode() != NM_DedicatedServer)
		|| (NockedArrow && !NockedArrow->bHiddenInGame);
}

void AVaesenRangedWeaponActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateNockedArrow();
	UpdateString(IsInHand());
}

void AVaesenRangedWeaponActor::UpdateNockedArrow()
{
	const UVaesenRangedWeaponItem* Item = GetRangedItem();
	const AVaesenBaseCharacter* Character = GetOwningCharacter();
	USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	const UAnimInstance* Anim = Body ? Body->GetAnimInstance() : nullptr;
	const bool bCurveShows = Anim && Anim->GetCurveValue(UVaesenAnimInstance::BowArrowCurve) > 0.5f;
	// Loosed: hidden until the animation has put the arrow away too (the next is taken from the quiver).
	if (bArrowLoosed && !bCurveShows)
	{
		bArrowLoosed = false;
	}
	const bool bShown = IsInHand() && Item && Item->NockedArrowMesh && bCurveShows && !bArrowLoosed;
	if (!bShown)
	{
		NockedArrow->SetHiddenInGame(true);
		return;
	}
	if (NockedArrow->GetStaticMesh() != Item->NockedArrowMesh)
	{
		NockedArrow->SetStaticMesh(Item->NockedArrowMesh);
	}
	if (NockedArrow->GetAttachParent() != Body || NockedArrow->GetAttachSocketName() != TEXT("hand_r"))
	{
		NockedArrow->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("hand_r"));
	}
	float Values[6];
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Values[Index] = Anim->GetCurveValue(UVaesenAnimInstance::BowArrowTransformCurves[Index]);
	}
	NockedArrow->SetRelativeLocationAndRotation(FVector(Values[0], Values[1], Values[2]), FRotator(Values[3], Values[4], Values[5]));
	NockedArrow->SetHiddenInGame(false);
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
		if ((!Player || Item->CountAmmoIn(Player->GetInventory()) <= 0) && !UVaesenCombatLibrary::IsFreeFire())
		{
			return EVaesenFireBlock::Empty;
		}
	}
	else if (NeedsCycle())
	{
		return EVaesenFireBlock::NeedsCycle;
	}
	else if (Item->GetLoadedRounds() <= 0 && !UVaesenCombatLibrary::IsFreeFire())
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
		UE_LOG(LogVaesenCombat, Display, TEXT("%s: shot refused (%s)"), *GetNameSafe(Shooter), Item ? *UEnum::GetValueAsString(GetFireBlock()) : TEXT("no weapon"));
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
		if (!AmmoClass && UVaesenCombatLibrary::IsFreeFire())
		{
			// vaesen.combat.freefire: its usual arrow, none taken.
			AmmoClass = Item->AcceptedAmmo;
		}
		else if (!AmmoClass || Player->GetInventory()->RemoveItemsOfClass(AmmoClass, 1) <= 0)
		{
			return false;
		}
	}
	else
	{
		if (Item->GetLoadedRounds() > 0)
		{
			AmmoClass = Item->GetLoadedAmmo();
			Item->SetLoadedRounds(Item->GetLoadedRounds() - 1, AmmoClass);
		}
		else
		{
			// vaesen.combat.freefire, empty: its usual round, none used.
			AmmoClass = Item->AcceptedAmmo;
		}
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
		UE_LOG(LogVaesenCombat, Display, TEXT("%s fires a %s from %s toward %s (aim from %s)"), *GetNameSafe(Shooter), *GetNameSafe(AmmoClass), *Muzzle.ToCompactString(),
			*AimPoint.ToCompactString(), *AimOrigin.ToCompactString());
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
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
	const bool bAiming = ASC && ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming);
	PlayFireCosmetics(Item ? Item->GetFireMontage(bAiming) : nullptr);

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
	// A bow: the arrow has left the string; the one in the drawing hand is gone at once (the release animation would carry
	// it back with the hand a moment longer).
	bArrowLoosed = true;
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
		// The shooter's camera kicks too.
		if (AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwner()); Player && Player->IsLocallyControlled())
		{
			Player->AddCameraKick(Hold.CameraKick, Hold.RecoilRecovery);
		}
	}

	if (Item->CasingEjection == EVaesenCasingEjection::OnFire)
	{
		EjectCasingsUnlessNotified(EVaesenCasingMoment::Fire, FireMontage, 1);
	}
	OnFired();
}

void AVaesenRangedWeaponActor::PlayDryFire(EVaesenFireBlock Why)
{
	PlayWeaponSound(EVaesenWeaponSound::DryFire);
	LastDryFireTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	LastDryFireBlock = Why;
	if (const AVaesenBaseCharacter* Character = GetOwningCharacter())
	{
		if (UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Character->GetMesh()->GetAnimInstance()))
		{
			Anim->PulseDryTrigger();
		}
	}
}

bool AVaesenRangedWeaponActor::GetLastDryFire(double& OutTime, EVaesenFireBlock& OutWhy) const
{
	OutTime = LastDryFireTime;
	OutWhy = LastDryFireBlock;
	return LastDryFireBlock != EVaesenFireBlock::None;
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
	const UAnimMontage* Montage = Moment == EVaesenCasingMoment::Cycle ? Item->GetMontage(&FVaesenWeaponMontages::Cycle) : Item->GetReloadMontage();
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
