#include "Combat/VaesenTrainingDummy.h"

#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

namespace VaesenDummy
{
	constexpr float NumberLifetime = 2.0f;
	constexpr float NumberRiseSpeed = 40.0f;
}

AVaesenTrainingDummy::AVaesenTrainingDummy()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(35.0f, 90.0f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	// Shots and swings trace Pawn and Visibility: block both.
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	SetRootComponent(Capsule);

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		Body->SetRelativeScale3D(FVector(0.6, 0.6, 1.8));
	}

	HealthText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(Capsule);
	HealthText->SetRelativeLocation(FVector(0.0, 0.0, 115.0));
	HealthText->SetHorizontalAlignment(EHTA_Center);
	HealthText->SetWorldSize(18.0f);
	HealthText->SetTextRenderColor(FColor::White);

	AbilitySystem = CreateDefaultSubobject<UVaesenAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	Attributes = CreateDefaultSubobject<UVaesenAttributeSet>(TEXT("Attributes"));
}

UAbilitySystemComponent* AVaesenTrainingDummy::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AVaesenTrainingDummy::BeginPlay()
{
	Super::BeginPlay();
	AbilitySystem->InitAbilityActorInfo(this, this);
	if (SurfaceMaterial)
	{
		Capsule->SetPhysMaterialOverride(SurfaceMaterial);
	}

	if (HasAuthority())
	{
		AbilitySystem->SetNumericAttributeBase(UVaesenAttributeSet::GetMaxHealthAttribute(), MaxHealth);
		AbilitySystem->SetNumericAttributeBase(UVaesenAttributeSet::GetHealthAttribute(), MaxHealth);
		AbilitySystem->SetNumericAttributeBase(UVaesenAttributeSet::GetProtectionSlashingAttribute(), ProtectionSlashing);
		AbilitySystem->SetNumericAttributeBase(UVaesenAttributeSet::GetProtectionPiercingAttribute(), ProtectionPiercing);
		AbilitySystem->SetNumericAttributeBase(UVaesenAttributeSet::GetProtectionBluntAttribute(), ProtectionBlunt);
		AbilitySystem->OnDamageReported.AddDynamic(this, &AVaesenTrainingDummy::OnDamageReported);
		AbilitySystem->OnOutOfHealth.AddDynamic(this, &AVaesenTrainingDummy::OnOutOfHealth);
	}
}

void AVaesenTrainingDummy::OnDamageReported(const FVaesenDamageReport& Report)
{
	MulticastShowDamage(Report);
	if (!AbilitySystem->IsDead())
	{
		GetWorldTimerManager().SetTimer(ResetTimer, this, &AVaesenTrainingDummy::ResetDummy, ResetDelay, false);
	}
}

void AVaesenTrainingDummy::OnOutOfHealth(AActor* Killer)
{
	UE_LOG(LogVaesenCombat, Log, TEXT("%s died (killed by %s); resetting in %.1f s."), *GetName(), *GetNameSafe(Killer), ResetDelay);
	OnDummyDeath(Killer);
	GetWorldTimerManager().SetTimer(ResetTimer, this, &AVaesenTrainingDummy::ResetDummy, ResetDelay, false);
}

void AVaesenTrainingDummy::ResetDummy()
{
	AbilitySystem->ResetVitals();
}

void AVaesenTrainingDummy::MulticastShowDamage_Implementation(const FVaesenDamageReport& Report)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	// One line per damage type: "Slashing 12 > 7", the outcome on top.
	TArray<FString> Lines;
	if (Report.Outcome != EVaesenHitOutcome::Hit)
	{
		Lines.Add(StaticEnum<EVaesenHitOutcome>()->GetNameStringByValue(static_cast<int64>(Report.Outcome)).ToUpper());
	}
	for (const FVaesenDamageLine& Line : Report.Lines)
	{
		const FString Type = Line.DamageType.GetTagName().ToString().Replace(TEXT("Damage."), TEXT(""));
		Lines.Add(FString::Printf(TEXT("%s %.0f > %.0f"), *Type, Line.Raw, Line.Final));
	}

	UTextRenderComponent* Text = NewObject<UTextRenderComponent>(this);
	Text->SetupAttachment(Capsule);
	Text->RegisterComponent();
	Text->SetUsingAbsoluteRotation(true);
	Text->SetWorldLocation(GetActorLocation() + FVector(0.0, 0.0, 135.0 + 12.0 * FloatingNumbers.Num()));
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetWorldSize(14.0f);
	Text->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
	const FColor Color = Report.Outcome == EVaesenHitOutcome::Parried ? FColor(240, 190, 80)
		: Report.Outcome == EVaesenHitOutcome::Blocked ? FColor(130, 170, 230)
		: Report.TotalFinal <= 0.0f ? FColor(160, 160, 160) : FColor(230, 60, 50);
	Text->SetTextRenderColor(Color);
	FloatingNumbers.Add({ Text, 0.0f });

	if (UVaesenCombatLibrary::IsCombatDebugEnabled())
	{
		DrawDebugSphere(GetWorld(), Report.Location, 5.0f, 8, FColor::Red, false, 2.0f);
	}
}

void AVaesenTrainingDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const float Health = AbilitySystem->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
	const float Max = AbilitySystem->GetNumericAttribute(UVaesenAttributeSet::GetMaxHealthAttribute());
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Health, Max)));

	// Numbers rise, face the viewer and go after a while.
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	FVector ViewLocation = GetActorLocation();
	FRotator ViewRotation;
	if (Viewer)
	{
		Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	const FRotator Facing = (ViewLocation - GetActorLocation()).GetSafeNormal2D().Rotation();
	HealthText->SetWorldRotation(Facing);
	for (int32 Index = FloatingNumbers.Num() - 1; Index >= 0; --Index)
	{
		FFloatingNumber& Number = FloatingNumbers[Index];
		UTextRenderComponent* Text = Number.Text.Get();
		Number.Age += DeltaSeconds;
		if (!Text || Number.Age > VaesenDummy::NumberLifetime)
		{
			if (Text)
			{
				Text->DestroyComponent();
			}
			FloatingNumbers.RemoveAt(Index);
			continue;
		}
		Text->AddWorldOffset(FVector(0.0, 0.0, VaesenDummy::NumberRiseSpeed * DeltaSeconds));
		Text->SetWorldRotation(Facing);
	}
}

void AVaesenTrainingDummy::HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters)
{
	if (EventType == EGameplayCueEvent::Executed && GameplayCueTag.MatchesTag(VaesenTags::GameplayCue_Hit))
	{
		FHitResult Hit;
		Hit.ImpactPoint = Parameters.Location;
		Hit.ImpactNormal = Parameters.Normal;
		Hit.PhysMaterial = SurfaceMaterial;
		const FGameplayTag DamageType = Parameters.AggregatedSourceTags.Num() > 0 ? Parameters.AggregatedSourceTags.First() : FGameplayTag();
		UE_LOG(LogVaesenCombat, Verbose, TEXT("%s hit cue at %.3f s"), *GetName(), GetWorld()->GetTimeSeconds());
		UVaesenCombatLibrary::PlayImpactEffects(this, DamageType, Hit, Parameters.AggregatedTargetTags.HasTagExact(VaesenTags::Data_Projectile)
			? EVaesenImpactDelivery::Shot : EVaesenImpactDelivery::ByHand);
	}
	IGameplayCueInterface::HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
}
