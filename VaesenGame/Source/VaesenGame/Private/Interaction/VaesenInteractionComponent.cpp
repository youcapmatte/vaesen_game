#include "Interaction/VaesenInteractionComponent.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Core/VaesenSettings.h"
#include "GameFramework/Actor.h"
#include "Interaction/VaesenInteractorComponent.h"
#include "UI/Interaction/VaesenInteractionCardWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenInteraction, Log, All);

UVaesenInteractionComponent::UVaesenInteractionComponent()
{
	// Card: flat on the screen at the component's location, bottom centre on it, hidden until looked at.
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FIntPoint(200, 150));
	SetPivot(FVector2D(0.5f, 1.0f));
	SetHiddenInGame(true);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

void UVaesenInteractionComponent::BeginPlay()
{
	// The default card, unless the actor picked its own. Before Super, which creates the widget.
	if (!GetWidgetClass())
	{
		SetWidgetClass(UVaesenSettings::Get()->InteractionCardWidget.LoadSynchronous());
	}
	Super::BeginPlay();
	RefreshCard();
}

void UVaesenInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Nobody can keep interacting with something that's gone.
	for (AVaesenPlayerCharacter* Character : TArray<AVaesenPlayerCharacter*>(Interactors))
	{
		EndInteract(Character);
	}
	if (AVaesenPlayerCharacter* Character = FocusingCharacter.Get())
	{
		EndFocus(Character);
	}
	Super::EndPlay(EndPlayReason);
}

void UVaesenInteractionComponent::SetInteractableName(const FText& NewName)
{
	InteractableName = NewName;
	RefreshCard();
}

void UVaesenInteractionComponent::SetActionText(const FText& NewActionText)
{
	ActionText = NewActionText;
	RefreshCard();
}

void UVaesenInteractionComponent::SetDescription(const FText& NewDescription)
{
	Description = NewDescription;
	RefreshCard();
}

void UVaesenInteractionComponent::SetQuantity(int32 NewQuantity)
{
	Quantity = NewQuantity;
	RefreshCard();
}

void UVaesenInteractionComponent::SetInteractionEnabled(bool bEnabled)
{
	bInteractionEnabled = bEnabled;
	// Interactors notice on their next check (CanInteract fails) and let go of it.
}

float UVaesenInteractionComponent::GetDistanceTo(const AVaesenPlayerCharacter* Character) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Character)
	{
		return TNumericLimits<float>::Max();
	}

	const FVector Eyes = Character->GetPawnViewLocation();
	const FBox ActorBounds = Owner->GetComponentsBoundingBox(false);
	return ActorBounds.IsValid ? FMath::Sqrt(ActorBounds.ComputeSquaredDistanceToPoint(Eyes)) : FVector::Dist(Eyes, Owner->GetActorLocation());
}

bool UVaesenInteractionComponent::CanInteract(const AVaesenPlayerCharacter* Character) const
{
	if (!Character || !bInteractionEnabled || !IsActive())
	{
		return false;
	}
	if (!bAllowMultipleInteractors && Interactors.Num() > 0 && !Interactors.Contains(Character))
	{
		return false;
	}
	return GetDistanceTo(Character) <= InteractionDistance;
}

float UVaesenInteractionComponent::GetInteractionProgress() const
{
	const AVaesenPlayerCharacter* Character = FocusingCharacter.Get();
	const UVaesenInteractorComponent* Interactor = Character ? Character->GetInteractor() : nullptr;
	return Interactor && Interactor->GetViewedInteractionComponent() == this ? Interactor->GetInteractionProgress() : 0.0f;
}

void UVaesenInteractionComponent::RefreshCard()
{
	if (UVaesenInteractionCardWidget* Card = Cast<UVaesenInteractionCardWidget>(GetUserWidgetObject()))
	{
		Card->SetInteractionComponent(this);
	}
}

void UVaesenInteractionComponent::BeginFocus(AVaesenPlayerCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	FocusingCharacter = Character;
	if (bPlaceCardAboveOwner && GetOwner())
	{
		const FBox OwnerBounds = GetOwner()->GetComponentsBoundingBox(false);
		if (OwnerBounds.IsValid)
		{
			const FVector Center = OwnerBounds.GetCenter();
			SetWorldLocation(FVector(Center.X, Center.Y, OwnerBounds.Max.Z + CardHeightAboveOwner));
		}
	}
	SetHiddenInGame(false);
	SetHighlighted(true);
	RefreshCard();
	OnBeginFocus.Broadcast(Character);
}

void UVaesenInteractionComponent::EndFocus(AVaesenPlayerCharacter* Character)
{
	if (FocusingCharacter.Get() != Character)
	{
		return;
	}

	FocusingCharacter.Reset();
	SetHiddenInGame(true);
	SetHighlighted(false);
	OnEndFocus.Broadcast(Character);
}

void UVaesenInteractionComponent::BeginInteract(AVaesenPlayerCharacter* Character)
{
	if (!CanInteract(Character))
	{
		return;
	}

	Interactors.AddUnique(Character);
	OnBeginInteract.Broadcast(Character);
}

void UVaesenInteractionComponent::EndInteract(AVaesenPlayerCharacter* Character)
{
	if (Interactors.Remove(Character) > 0)
	{
		OnEndInteract.Broadcast(Character);
	}
}

void UVaesenInteractionComponent::Interact(AVaesenPlayerCharacter* Character)
{
	// The server decides; the player's own machine only shows the hold.
	if (!Character || !GetOwner() || !GetOwner()->HasAuthority() || !Interactors.Contains(Character))
	{
		return;
	}

	UE_LOG(LogVaesenInteraction, Display, TEXT("%s interacted with %s."), *Character->GetName(), *GetOwner()->GetName());
	OnInteract.Broadcast(Character);
}

void UVaesenInteractionComponent::SetHighlighted(bool bHighlighted)
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		// Not the card itself (or any other widget).
		if (Primitive->IsA<UWidgetComponent>())
		{
			continue;
		}
		Primitive->SetRenderCustomDepth(bHighlighted);
		Primitive->SetCustomDepthStencilValue(bHighlighted ? HighlightStencilValue : 0);
	}
}
