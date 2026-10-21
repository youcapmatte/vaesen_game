#include "Interaction/VaesenInteractorComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "TimerManager.h"

UVaesenInteractorComponent::UVaesenInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// For the server RPCs.
	SetIsReplicatedByDefault(true);
}

AVaesenPlayerCharacter* UVaesenInteractorComponent::GetCharacter() const
{
	return Cast<AVaesenPlayerCharacter>(GetOwner());
}

void UVaesenInteractorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AVaesenPlayerCharacter* Character = GetCharacter();
	if (!Character)
	{
		return;
	}

	if (Character->IsLocallyControlled())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - InteractionData.LastInteractionCheckTime >= CheckInterval)
		{
			InteractionData.LastInteractionCheckTime = Now;
			PerformInteractionCheck();
		}
		return;
	}

	// Server, for a remote player: stop if they've moved out of reach (or it's been disabled) mid-interaction.
	if (GetOwner()->HasAuthority() && InteractionData.bInteractHeld)
	{
		const UVaesenInteractionComponent* Interactable = InteractionData.ViewedInteractionComponent;
		if (!Interactable || !Interactable->CanInteract(Character))
		{
			EndInteract();
		}
	}
}

void UVaesenInteractorComponent::PerformInteractionCheck()
{
	AVaesenPlayerCharacter* Character = GetCharacter();
	const AController* Controller = Character ? Character->GetController() : nullptr;
	if (!Controller)
	{
		CouldntFindInteractable();
		return;
	}

	// Along the camera's view, from level with the character (the camera may be behind it) to TraceDistance ahead.
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector Direction = ViewRotation.Vector();
	const FVector Eyes = Character->GetPawnViewLocation();
	const FVector Start = ViewLocation + Direction * FMath::Max(0.0, FVector::DotProduct(Eyes - ViewLocation, Direction));
	// TraceDistance in front of the character whatever the pitch: a steep view gets a longer trace (up to twice).
	const double Flat = FVector(Direction.X, Direction.Y, 0.0).Size();
	const FVector End = Start + Direction * (TraceDistance / FMath::Max(Flat, 0.5));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenInteraction), false, Character);
	FHitResult Hit;
	UVaesenInteractionComponent* Found = nullptr;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(TraceRadius), Params))
	{
		if (const AActor* HitActor = Hit.GetActor())
		{
			UVaesenInteractionComponent* Interactable = HitActor->FindComponentByClass<UVaesenInteractionComponent>();
			if (Interactable && Interactable->CanInteract(Character))
			{
				Found = Interactable;
			}
		}
	}

	// Not aiming at anything: the nearest thing in front, e.g. on the floor.
	if (!Found && bFocusNearbyWhenNotAiming)
	{
		Found = FindNearbyInteractable(Eyes, Direction);
	}

#if ENABLE_DRAW_DEBUG
	const AVaesenBaseCharacter* Owner = Cast<AVaesenBaseCharacter>(GetOwner());
	const bool bWeaponDrawn = Owner && Owner->GetEquipment() && Owner->GetEquipment()->GetDrawnWeapon();
	if (bDrawDebugTrace && !bWeaponDrawn)
	{
		// The whole swept tube: green if nothing was hit, red up to the hit (and grey beyond it).
		const bool bHit = Hit.bBlockingHit;
		const FVector TraceEnd = bHit ? Hit.Location : End;
		const FVector Segment = TraceEnd - Start;
		const FQuat Along = FRotationMatrix::MakeFromZ(Segment.IsNearlyZero() ? Direction : Segment.GetSafeNormal()).ToQuat();
		DrawDebugCapsule(GetWorld(), Start + Segment * 0.5, Segment.Size() * 0.5 + TraceRadius, TraceRadius, Along,
			bHit ? FColor::Red : FColor::Green, false, 0.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugLine(GetWorld(), TraceEnd, End, FColor(128, 128, 128), false, 0.0f, 0, 0.5f);
			DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Red, false, 0.0f);
		}
		if (Found && Found->GetOwner())
		{
			const FBox Bounds = Found->GetOwner()->GetComponentsBoundingBox(false);
			DrawDebugBox(GetWorld(), Bounds.GetCenter(), Bounds.GetExtent(), FColor::Yellow, false, 0.0f, 0, 1.0f);
		}
	}
#endif

	if (!Found)
	{
		CouldntFindInteractable();
	}
	else if (Found != InteractionData.ViewedInteractionComponent)
	{
		FoundNewInteractable(Found);
	}
}

UVaesenInteractionComponent* UVaesenInteractorComponent::FindNearbyInteractable(const FVector& Eyes, const FVector& ViewDirection) const
{
	const AVaesenPlayerCharacter* Character = GetCharacter();
	const FVector Forward = FVector(ViewDirection.X, ViewDirection.Y, 0.0).GetSafeNormal();
	if (!Character || Forward.IsNearlyZero())
	{
		return nullptr;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenInteractionNearby), false, Character);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, Eyes, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(TraceDistance), Params);

	const float MinCosine = FMath::Cos(FMath::DegreesToRadians(NearbyAngle));
	UVaesenInteractionComponent* Best = nullptr;
	float BestScore = -1.0f;
	TSet<const AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Actor = Overlap.GetActor();
		if (!Actor || Seen.Contains(Actor))
		{
			continue;
		}
		Seen.Add(Actor);

		UVaesenInteractionComponent* Interactable = Actor->FindComponentByClass<UVaesenInteractionComponent>();
		if (!Interactable || !Interactable->CanInteract(Character))
		{
			continue;
		}

		// In front, within the angle (sideways only: looking up or down doesn't matter).
		const FVector Target = Actor->GetComponentsBoundingBox(false).GetCenter();
		const FVector Flat = FVector(Target.X - Eyes.X, Target.Y - Eyes.Y, 0.0).GetSafeNormal();
		const float Cosine = FVector::DotProduct(Forward, Flat);
		if (Cosine < MinCosine)
		{
			continue;
		}

		// In plain sight: nothing else in the way.
		FHitResult Blocker;
		if (GetWorld()->LineTraceSingleByChannel(Blocker, Eyes, Target, ECC_Visibility, Params) && Blocker.GetActor() != Actor)
		{
			continue;
		}

		// Straight ahead beats off to the side; near beats far.
		const float Score = Cosine - Interactable->GetDistanceTo(Character) / FMath::Max(TraceDistance, 1.0f) * 0.25f;
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Interactable;
		}
	}
	return Best;
}

void UVaesenInteractorComponent::FoundNewInteractable(UVaesenInteractionComponent* Interactable)
{
	// Looking at something else ends what we were doing.
	CouldntFindInteractable();

	InteractionData.ViewedInteractionComponent = Interactable;
	Interactable->BeginFocus(GetCharacter());
}

void UVaesenInteractorComponent::CouldntFindInteractable()
{
	UVaesenInteractionComponent* Previous = InteractionData.ViewedInteractionComponent;
	if (!Previous)
	{
		return;
	}

	// Looking away ends the interaction; looking back needs a new press.
	if (InteractionData.bInteractHeld)
	{
		EndInteract();
	}
	Previous->EndFocus(GetCharacter());
	InteractionData.ViewedInteractionComponent = nullptr;
}

void UVaesenInteractorComponent::BeginInteract()
{
	if (!GetOwner()->HasAuthority())
	{
		UVaesenInteractionComponent* Interactable = InteractionData.ViewedInteractionComponent;
		ServerBeginInteract(Interactable ? Interactable->GetOwner() : nullptr);
	}

	InteractionData.bInteractHeld = true;
	StartInteracting();
}

void UVaesenInteractorComponent::ServerBeginInteract_Implementation(AActor* InteractableActor)
{
	// Trust the client about what it's looking at, but check the rules (reach, enabled, taken) here.
	InteractionData.ViewedInteractionComponent = InteractableActor ? InteractableActor->FindComponentByClass<UVaesenInteractionComponent>() : nullptr;
	InteractionData.bInteractHeld = true;
	StartInteracting();
}

void UVaesenInteractorComponent::StartInteracting()
{
	AVaesenPlayerCharacter* Character = GetCharacter();
	UVaesenInteractionComponent* Interactable = InteractionData.ViewedInteractionComponent;
	if (!Character || !Interactable || !Interactable->CanInteract(Character))
	{
		return;
	}

	Interactable->BeginInteract(Character);
	if (FMath::IsNearlyZero(Interactable->InteractionTime))
	{
		Interact();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(InteractTimer, this, &UVaesenInteractorComponent::Interact, Interactable->InteractionTime, false);
	}
}

void UVaesenInteractorComponent::Interact()
{
	GetWorld()->GetTimerManager().ClearTimer(InteractTimer);

	// Does something on the server only; here it just ends the hold.
	if (UVaesenInteractionComponent* Interactable = InteractionData.ViewedInteractionComponent)
	{
		Interactable->Interact(GetCharacter());
	}
}

void UVaesenInteractorComponent::EndInteract()
{
	if (!GetOwner()->HasAuthority())
	{
		ServerEndInteract();
	}

	InteractionData.bInteractHeld = false;
	GetWorld()->GetTimerManager().ClearTimer(InteractTimer);

	if (UVaesenInteractionComponent* Interactable = InteractionData.ViewedInteractionComponent)
	{
		Interactable->EndInteract(GetCharacter());
	}
}

void UVaesenInteractorComponent::ServerEndInteract_Implementation()
{
	EndInteract();
}

bool UVaesenInteractorComponent::IsInteracting() const
{
	return GetWorld()->GetTimerManager().IsTimerActive(InteractTimer);
}

float UVaesenInteractorComponent::GetRemainingInteractTime() const
{
	return IsInteracting() ? GetWorld()->GetTimerManager().GetTimerRemaining(InteractTimer) : 0.0f;
}

float UVaesenInteractorComponent::GetInteractionProgress() const
{
	if (!IsInteracting())
	{
		return 0.0f;
	}
	const FTimerManager& Timers = GetWorld()->GetTimerManager();
	const float Rate = Timers.GetTimerRate(InteractTimer);
	return Rate > 0.0f ? FMath::Clamp(Timers.GetTimerElapsed(InteractTimer) / Rate, 0.0f, 1.0f) : 0.0f;
}
