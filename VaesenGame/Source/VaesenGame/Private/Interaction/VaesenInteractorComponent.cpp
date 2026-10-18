#include "Interaction/VaesenInteractorComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Interaction/VaesenInteractionComponent.h"
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
	const FVector End = Start + Direction * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenInteraction), false, Character);
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(TraceRadius), Params))
	{
		if (const AActor* HitActor = Hit.GetActor())
		{
			UVaesenInteractionComponent* Interactable = HitActor->FindComponentByClass<UVaesenInteractionComponent>();
			if (Interactable && Interactable->CanInteract(Character))
			{
				// Still the same one: nothing to do.
				if (Interactable != InteractionData.ViewedInteractionComponent)
				{
					FoundNewInteractable(Interactable);
				}
				return;
			}
		}
	}

	CouldntFindInteractable();
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
