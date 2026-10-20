#include "Combat/VaesenTrainingDummyAttacker.h"

#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AVaesenTrainingDummyAttacker::AVaesenTrainingDummyAttacker()
{
	ClubPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ClubPivot"));
	ClubPivot->SetupAttachment(Capsule);
	ClubPivot->SetRelativeLocation(FVector(20.0, 30.0, 40.0));

	Club = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Club"));
	Club->SetupAttachment(ClubPivot);
	Club->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Club->SetStaticMesh(Cube.Object);
	}
	// A club about 80 cm long, sticking forward from the pivot.
	Club->SetRelativeLocation(FVector(40.0, 0.0, 0.0));
	Club->SetRelativeScale3D(FVector(0.8, 0.08, 0.08));

	SwingDamage.Entries.Emplace(VaesenTags::Damage_Blunt, 10.0f);
}

void AVaesenTrainingDummyAttacker::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(SwingTimer, this, &AVaesenTrainingDummyAttacker::StartSwing, SwingInterval, true, SwingInterval);
	}
}

void AVaesenTrainingDummyAttacker::StartSwing()
{
	// Off balance after a parry (or "dead" until it resets): this swing is skipped.
	if (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Staggered) || AbilitySystem->IsDead())
	{
		return;
	}

	// Face the nearest player in reach of its awareness.
	const APawn* Nearest = nullptr;
	float Best = AwarenessRadius * AwarenessRadius;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		const float Distance = Pawn ? FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation()) : Best;
		if (Distance < Best)
		{
			Best = Distance;
			Nearest = Pawn;
		}
	}
	if (Nearest)
	{
		SetActorRotation(FRotator(0.0, (Nearest->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0));
	}

	MulticastSwingPhase(1);
	GetWorldTimerManager().SetTimer(StrikeTimer, this, &AVaesenTrainingDummyAttacker::Strike, WindupTime, false);
}

void AVaesenTrainingDummyAttacker::Strike()
{
	if (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Staggered))
	{
		MulticastSwingPhase(0);
		return;
	}
	MulticastSwingPhase(2);

	const FVector Start = GetActorLocation() + FVector(0.0, 0.0, 30.0);
	const FVector End = Start + GetActorForwardVector() * Reach;
	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenDummySwing), false, this);
	GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.0f), Params);
	if (UVaesenCombatLibrary::IsCombatDebugEnabled())
	{
		DrawDebugLine(GetWorld(), Start, End, Hits.Num() ? FColor::Red : FColor::Green, false, 1.0f, 0, 2.0f);
	}

	TSet<AActor*> Struck;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (Target && Target != this && !Struck.Contains(Target) && !Target->IsA<AVaesenTrainingDummy>())
		{
			Struck.Add(Target);
			UVaesenCombatLibrary::ApplyDamagePacket(this, Target, SwingDamage, Hit, nullptr, bSwingCanBeBlocked, false);
		}
	}
}

void AVaesenTrainingDummyAttacker::MulticastSwingPhase_Implementation(uint8 Phase)
{
	TargetClubPitch = Phase == 1 ? 80.0f : Phase == 2 ? -40.0f : 0.0f;
	USoundBase* Sound = Phase == 1 ? WindupSound.Get() : Phase == 2 ? SwingSound.Get() : nullptr;
	if (Sound && GetNetMode() != NM_DedicatedServer)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
	if (Phase == 2)
	{
		// Back to rest shortly after the strike.
		FTimerHandle Rest;
		GetWorldTimerManager().SetTimer(Rest, FTimerDelegate::CreateWeakLambda(this, [this]() { TargetClubPitch = 0.0f; }), 0.4f, false);
	}
}

void AVaesenTrainingDummyAttacker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// Raising is slow (readable), the strike fast.
	const float Speed = TargetClubPitch < ClubPitch ? 25.0f : 6.0f;
	ClubPitch = FMath::FInterpTo(ClubPitch, TargetClubPitch, DeltaSeconds, Speed);
	ClubPivot->SetRelativeRotation(FRotator(ClubPitch, 0.0f, 0.0f));
}
