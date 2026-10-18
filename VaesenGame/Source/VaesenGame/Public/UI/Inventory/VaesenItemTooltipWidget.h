#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenItemTooltipWidget.generated.h"

class UTextBlock;
class UVaesenItem;

/**
 * Shown when hovering an item in the inventory.
 *
 * Widget Blueprint (e.g. WBP_ItemTooltip, parent class VaesenItemTooltipWidget), controls by these exact names:
 *   NameText         (Text)  - the item's name
 *   CategoryText     (Text)  - e.g. "Weapon", "Food", "Quest item"
 *   DescriptionText  (Text)  - hidden when empty
 *   StatsText        (Text)  - weight, value, stack, what right-click does
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItem(UVaesenItem* InItem);

	/** What kind of item it is, for the tooltip. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static FText GetCategoryText(const UVaesenItem* Item);

protected:
	/** Fills the controls; override to show more (call the parent). */
	UFUNCTION(BlueprintNativeEvent, Category = "Inventory")
	void UpdateTooltip();

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UVaesenItem> Item;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CategoryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DescriptionText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatsText;
};
