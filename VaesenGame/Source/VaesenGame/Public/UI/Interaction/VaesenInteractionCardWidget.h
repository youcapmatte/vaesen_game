#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenInteractionCardWidget.generated.h"

class UProgressBar;
class UWidget;
class UTextBlock;
class UVaesenInteractionComponent;

/**
 * The card shown at an interactable the player looks at (by UVaesenInteractionComponent, a screen-space widget
 * component). Its content comes from the component, which the actor fills in.
 *
 * Widget Blueprint (e.g. WBP_InteractionCard, parent class VaesenInteractionCardWidget), controls by these exact names:
 *   NameText             (Text)          - InteractableName
 *   KeyText              (Text)          - the interact key, e.g. "E" (in a key-cap box); without it, ActionText
 *                                          gets the key in brackets: "[E] Pick up"
 *   ActionText           (Text)          - e.g. "Pick up"; hidden (with the key) when the action is empty
 *   ActionRow            (any widget)    - optional: the row holding key and action, hidden with them
 *   DescriptionText      (Text)          - hidden when empty
 *   QuantityText         (Text)          - e.g. "x3"; hidden when the quantity is 0
 *   InteractionProgress  (Progress Bar)  - fills while the key is held; only for interactions that take time
 * Override UpdateCard to show more (call the parent to keep the above).
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built, so each feature can be tried on its
 * own; whatever is missing is skipped. Make them BindWidget when the UI is done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenInteractionCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows this component's content. */
	void SetInteractionComponent(UVaesenInteractionComponent* Component);

	/** Fills the controls from OwningInteractionComponent. */
	UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
	void UpdateCard();

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UVaesenInteractionComponent> OwningInteractionComponent;

	/** The interact key as shown on the card. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText KeyLabel = INVTEXT("E");

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ActionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ActionRow;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> InteractionProgress;
};
