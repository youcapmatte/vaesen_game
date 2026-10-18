#include "Interaction/VaesenInteractableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "VaesenInteractable"

UVaesenInteractableComponent::UVaesenInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InteractionText = LOCTEXT("DefaultInteraction", "Interact");
}

void UVaesenInteractableComponent::SetHighlighted(bool bInHighlighted)
{
	bHighlighted = bInHighlighted;

	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives(Owner);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		Primitive->SetRenderCustomDepth(bHighlighted);
		Primitive->SetCustomDepthStencilValue(bHighlighted ? HighlightStencilValue : 0);
	}
}

void UVaesenInteractableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bHighlighted)
	{
		SetHighlighted(false);
	}
	Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
