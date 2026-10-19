#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenDropZoneWidget.generated.h"

class UBorder;

/**
 * The "Drop" box in the inventory screen: drag an item onto it (from an inventory, or off the body) to drop it on the
 * ground in front of the character. Plays the inventory's DropSound; the item falls to the ground as a pickup.
 *
 * Widget Blueprint (e.g. WBP_DropZone, parent class VaesenDropZoneWidget), controls by these exact names:
 *   ZoneBorder  (Border)  - tinted while something is dragged over it
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenDropZoneWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FLinearColor NormalColor = FLinearColor(0.03f, 0.025f, 0.02f, 0.85f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FLinearColor HoveredColor = FLinearColor(0.30f, 0.12f, 0.06f, 0.95f);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ZoneBorder;
};
