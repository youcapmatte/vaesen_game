#include "UI/Interaction/VaesenInteractionCardWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Interaction/VaesenInteractionComponent.h"

#define LOCTEXT_NAMESPACE "VaesenInteractionCard"

namespace
{
	void SetLine(UTextBlock* Line, const FText& Text)
	{
		if (Line)
		{
			Line->SetText(Text);
			Line->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
}

void UVaesenInteractionCardWidget::SetInteractionComponent(UVaesenInteractionComponent* Component)
{
	OwningInteractionComponent = Component;
	UpdateCard();
}

void UVaesenInteractionCardWidget::UpdateCard_Implementation()
{
	const UVaesenInteractionComponent* Component = OwningInteractionComponent;
	if (!Component)
	{
		return;
	}

	SetLine(NameText, Component->InteractableName);
	// The key in its own box when there is one, else in brackets before the action.
	const bool bHasAction = !Component->ActionText.IsEmpty();
	if (KeyText)
	{
		SetLine(KeyText, bHasAction ? KeyLabel : FText::GetEmpty());
		SetLine(ActionText, Component->ActionText);
	}
	else
	{
		SetLine(ActionText, bHasAction ? FText::Format(INVTEXT("[{0}] {1}"), KeyLabel, Component->ActionText) : FText::GetEmpty());
	}
	if (ActionRow)
	{
		ActionRow->SetVisibility(bHasAction ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetLine(DescriptionText, Component->Description);
	SetLine(QuantityText, Component->Quantity > 0 ? FText::Format(LOCTEXT("Quantity", "x{0}"), FText::AsNumber(Component->Quantity)) : FText::GetEmpty());

	if (InteractionProgress)
	{
		InteractionProgress->SetPercent(0.0f);
		InteractionProgress->SetVisibility(Component->InteractionTime > KINDA_SMALL_NUMBER ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UVaesenInteractionCardWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (InteractionProgress && OwningInteractionComponent)
	{
		InteractionProgress->SetPercent(OwningInteractionComponent->GetInteractionProgress());
	}
}

#undef LOCTEXT_NAMESPACE
