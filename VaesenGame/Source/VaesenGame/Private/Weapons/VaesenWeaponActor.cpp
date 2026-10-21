#include "Weapons/VaesenWeaponActor.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

namespace VaesenWeaponActor
{
	const FName Grip(TEXT("Grip"));
	const FName TraceStart(TEXT("TraceStart"));
	const FName TraceEnd(TEXT("TraceEnd"));
}

AVaesenWeaponActor::AVaesenWeaponActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	// Where it hangs follows the equipment's drawn state on each machine, not replicated movement.
	SetReplicatingMovement(false);
	bNetUseOwnerRelevancy = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
	StaticMesh->SetupAttachment(Root);
	StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMesh->SetGenerateOverlapEvents(false);
	StaticMesh->SetCanEverAffectNavigation(false);

	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(Root);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkeletalMesh->SetGenerateOverlapEvents(false);
	SkeletalMesh->SetCanEverAffectNavigation(false);
}

void AVaesenWeaponActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVaesenWeaponActor, WeaponItem);
}

void AVaesenWeaponActor::SetWeaponItem(UVaesenWeaponItem* Item)
{
	WeaponItem = Item;
	ApplyWeaponMesh();
}

AVaesenBaseCharacter* AVaesenWeaponActor::GetOwningCharacter() const
{
	return Cast<AVaesenBaseCharacter>(GetOwner());
}

void AVaesenWeaponActor::BeginPlay()
{
	Super::BeginPlay();
	OnRep_WeaponItem();
}

void AVaesenWeaponActor::OnRep_WeaponItem()
{
	ApplyWeaponMesh();
	if (const AVaesenBaseCharacter* Character = GetOwningCharacter())
	{
		if (UVaesenEquipmentComponent* Equipment = Character->GetEquipment())
		{
			// Straight to the right place, even before the item knows this actor (an item only ever goes in its own slot).
			if (WeaponItem)
			{
				AttachToOwner(Equipment->IsInHand(WeaponItem->Slot));
			}
			Equipment->RefreshWeaponAttachments();
		}
	}
}

void AVaesenWeaponActor::ApplyWeaponMesh()
{
	const UVaesenWeaponItem* Item = WeaponItem;
	USkeletalMesh* Skeletal = Item ? Item->WeaponSkeletalMesh.Get() : nullptr;
	UStaticMesh* Static = Item && !Skeletal ? Item->GetHeldStaticMesh() : nullptr;

	SkeletalMesh->SetSkeletalMeshAsset(Skeletal);
	SkeletalMesh->SetVisibility(Skeletal != nullptr);
	StaticMesh->SetStaticMesh(Static);
	StaticMesh->SetVisibility(Static != nullptr);

	UMeshComponent* Mesh = GetWeaponMesh();
	if (!Mesh)
	{
		return;
	}

	// A grey-box stand-in (the pickup mesh) is shown at the item's pickup size; a real weapon mesh at its own.
	const FVector Scale = Item && !Item->WeaponMesh && !Skeletal ? Item->PickupScale : FVector::OneVector;

	// Hold the mesh so its Grip socket is at the actor's origin: the hand socket then holds it by the grip.
	Mesh->SetRelativeTransform(FTransform::Identity);
	if (!Mesh->DoesSocketExist(VaesenWeaponActor::Grip))
	{
		Mesh->SetRelativeScale3D(Scale);
		return;
	}
	const FTransform GripLocal = Mesh->GetSocketTransform(VaesenWeaponActor::Grip, RTS_Component);
	const FQuat Rotation = GripLocal.GetRotation().Inverse();
	const FVector Location = -(Rotation.RotateVector(Scale * GripLocal.GetLocation()));
	Mesh->SetRelativeTransform(FTransform(Rotation, Location, Scale));
}

UMeshComponent* AVaesenWeaponActor::GetWeaponMesh() const
{
	if (SkeletalMesh->GetSkeletalMeshAsset())
	{
		return SkeletalMesh;
	}
	return StaticMesh->GetStaticMesh() ? StaticMesh.Get() : nullptr;
}

bool AVaesenWeaponActor::GetSocketTransform(FName Socket, FTransform& OutTransform) const
{
	const UMeshComponent* Mesh = GetWeaponMesh();
	if (Mesh && Mesh->DoesSocketExist(Socket))
	{
		OutTransform = Mesh->GetSocketTransform(Socket);
		return true;
	}
	OutTransform = GetActorTransform();
	return false;
}

bool AVaesenWeaponActor::ResolveAttachment(bool bToHand, FName& OutSocket, FTransform& OutOffset) const
{
	const AVaesenBaseCharacter* Character = GetOwningCharacter();
	const USkeletalMeshComponent* Body = Character ? Character->GetMesh() : nullptr;
	if (!Body || !WeaponItem)
	{
		return false;
	}

	const UVaesenWeaponCategoryDefaults* Defaults = UVaesenWeaponCategoryDefaults::Get();
	OutOffset = FTransform::Identity;
	if (bToHand)
	{
		OutSocket = WeaponItem->IsHeldInOffHand() ? Defaults->OffHandSocket : Defaults->MainHandSocket;
		if (!Body->DoesSocketExist(OutSocket))
		{
			// No weapon socket on the skeleton yet: the hand bone itself.
			OutSocket = WeaponItem->IsHeldInOffHand() ? FName(TEXT("hand_l")) : FName(TEXT("hand_r"));
		}
		return true;
	}

	OutSocket = WeaponItem->SheathSocket;
	OutOffset = WeaponItem->SheathOffset;
	if (!Body->DoesSocketExist(OutSocket))
	{
		const FVaesenSheathLocation* Location = Defaults->FindSheathLocation(OutSocket);
		OutSocket = Location ? Location->FallbackBone : FName(TEXT("spine_03"));
		OutOffset = Location ? OutOffset * Location->FallbackOffset : OutOffset;
	}
	return true;
}

FTransform AVaesenWeaponActor::GetSheathedTransform() const
{
	FName Socket;
	FTransform Offset;
	const AVaesenBaseCharacter* Character = GetOwningCharacter();
	if (!Character || !ResolveAttachment(false, Socket, Offset))
	{
		return GetActorTransform();
	}
	return Offset * Character->GetMesh()->GetSocketTransform(Socket);
}

void AVaesenWeaponActor::AttachToOwner(bool bNowInHand)
{
	AVaesenBaseCharacter* Character = GetOwningCharacter();
	FName Socket;
	FTransform Offset;
	if (!Character || !ResolveAttachment(bNowInHand, Socket, Offset))
	{
		return;
	}

	AttachToComponent(Character->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
	SetActorRelativeTransform(Offset);

	if (bInHand != bNowInHand)
	{
		bInHand = bNowInHand;
		OnAttachedToOwner(bInHand);
	}
}

void AVaesenWeaponActor::PlayWeaponSound(EVaesenWeaponSound Sound) const
{
	const UWorld* World = GetWorld();
	if (!WeaponItem || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Played = WeaponItem->GetSound(Sound);
	if (!Played)
	{
		return;
	}
	const UVaesenWeaponSoundSet* Set = WeaponItem->GetSoundSetFor(Sound);
	USoundAttenuation* Attenuation = Set ? (UVaesenWeaponSoundSet::IsGunshot(Sound) ? Set->GunshotAttenuation : Set->ShortRangeAttenuation).Get() : nullptr;
	UGameplayStatics::SpawnSoundAttached(Played, Root, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true,
		1.0f, 1.0f, 0.0f, Attenuation, Set ? Set->Concurrency.Get() : nullptr);
}

void AVaesenWeaponActor::MulticastPlaySound_Implementation(EVaesenWeaponSound Sound)
{
	PlayWeaponSound(Sound);
}

void AVaesenWeaponActor::MulticastDebugLine_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End, FColor Color, float Duration)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		DrawDebugLine(GetWorld(), Start, End, Color, false, Duration, 0, 1.5f);
	}
}

// --- Melee hit window ---

void AVaesenWeaponActor::GetStrikeLine(FVector& OutStart, FVector& OutEnd) const
{
	const UMeshComponent* Mesh = GetWeaponMesh();
	if (Mesh && Mesh->DoesSocketExist(VaesenWeaponActor::TraceStart) && Mesh->DoesSocketExist(VaesenWeaponActor::TraceEnd))
	{
		OutStart = Mesh->GetSocketLocation(VaesenWeaponActor::TraceStart);
		OutEnd = Mesh->GetSocketLocation(VaesenWeaponActor::TraceEnd);
		return;
	}

	// No sockets: the mesh's length out from the grip (along its longest side), or the attack's reach out in front.
	if (Mesh)
	{
		const FBoxSphereBounds Local = Mesh->CalcLocalBounds();
		const FVector Extent = Local.BoxExtent;
		const FVector Axis = Extent.X >= Extent.Y && Extent.X >= Extent.Z ? FVector::XAxisVector : Extent.Y >= Extent.Z ? FVector::YAxisVector : FVector::ZAxisVector;
		const FTransform& MeshTransform = Mesh->GetComponentTransform();
		const FVector Far = Local.Origin + Axis * FVector::DotProduct(Extent, Axis);
		const FVector Near = Local.Origin - Axis * FVector::DotProduct(Extent, Axis);
		OutStart = GetActorLocation();
		const FVector FarWorld = MeshTransform.TransformPosition(Far);
		const FVector NearWorld = MeshTransform.TransformPosition(Near);
		OutEnd = FVector::DistSquared(FarWorld, OutStart) >= FVector::DistSquared(NearWorld, OutStart) ? FarWorld : NearWorld;
		return;
	}
	const AActor* Character = GetOwner();
	OutStart = GetActorLocation();
	OutEnd = OutStart + (Character ? Character->GetActorForwardVector() : GetActorForwardVector()) * ActiveAttack.Reach * 0.6f;
}

void AVaesenWeaponActor::BeginHitWindow(const FVaesenMeleeAttack& Attack)
{
	if (!HasAuthority())
	{
		return;
	}
	ActiveAttack = Attack;
	StruckThisSwing.Reset();
	bHitWindowOpen = true;
	GetStrikeLine(LastStrikeStart, LastStrikeEnd);
	SetActorTickEnabled(true);
	SweepHitWindow();
}

void AVaesenWeaponActor::EndHitWindow()
{
	if (bHitWindowOpen)
	{
		SweepHitWindow();
	}
	bHitWindowOpen = false;
	SetActorTickEnabled(false);
}

void AVaesenWeaponActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bHitWindowOpen)
	{
		SweepHitWindow();
	}
}

void AVaesenWeaponActor::SweepHitWindow()
{
	FVector Start, End;
	GetStrikeLine(Start, End);

	// Fill in between last frame's blade and this one, so a fast swing past a thin target still hits.
	const float Moved = FMath::Max(FVector::Dist(Start, LastStrikeStart), FVector::Dist(End, LastStrikeEnd));
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Moved / FMath::Max(StrikeRadius * 1.5f, 1.0f)), 1, MaxSubSteps);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenMeleeSweep), false, this);
	Params.AddIgnoredActor(GetOwner());
	Params.bReturnPhysicalMaterial = true;
	const bool bDebug = UVaesenCombatLibrary::IsCombatDebugEnabled();

	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const float Alpha = static_cast<float>(Step) / Steps;
		const FVector StepStart = FMath::Lerp(LastStrikeStart, Start, Alpha);
		const FVector StepEnd = FMath::Lerp(LastStrikeEnd, End, Alpha);

		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByChannel(Hits, StepStart, StepEnd, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(StrikeRadius), Params);
		if (bDebug)
		{
			MulticastDebugLine(StepStart, StepEnd, Hits.Num() ? FColor::Red : FColor::Green, 1.0f);
		}
		for (const FHitResult& Hit : Hits)
		{
			StrikeTarget(Hit);
		}
	}

	LastStrikeStart = Start;
	LastStrikeEnd = End;
}

void AVaesenWeaponActor::StrikeTarget(const FHitResult& Hit)
{
	AActor* Target = Hit.GetActor();
	if (!Target || Target == GetOwner() || StruckThisSwing.Contains(Target))
	{
		return;
	}
	StruckThisSwing.Add(Target);

	if (!UVaesenCombatLibrary::ApplyDamagePacket(GetOwner(), Target, ActiveAttack.Damage, Hit, WeaponItem, ActiveAttack.bCanBeBlocked, false, ActiveAttack.Knockback))
	{
		// A wall, a table: no damage, but it should still sound like a blade on wood.
		MulticastImpact(Hit, UVaesenCombatLibrary::GetMainDamageType(ActiveAttack.Damage), false);
	}
	if (WeaponItem)
	{
		WeaponItem->ApplyWear(ActiveAttack.DurabilityCostPerHit);
	}
}

void AVaesenWeaponActor::MulticastImpact_Implementation(const FHitResult& Hit, FGameplayTag DamageType, bool bBullet)
{
	UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit, bBullet);
}
