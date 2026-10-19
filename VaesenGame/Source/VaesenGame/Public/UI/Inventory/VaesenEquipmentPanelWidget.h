#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenEquipmentPanelWidget.generated.h"

class UTextBlock;
class UUniformGridPanel;
class UVaesenEquipmentComponent;
class UVaesenEquipmentSlotWidget;

/**
 * The player's equipment: one slot per equipment slot, two per row (Head/Back, Chest/Hands, Legs/Feet, Main
 * Hand/Off Hand, Talisman/Bag). Dropping an item on the panel (not on a slot) puts it on in its own slot.
 *
 * Widget Blueprint (e.g. WBP_EquipmentPanel, parent class VaesenEquipmentPanelWidget), controls by these exact names:
 *   TitleText  (Text)               - e.g. "Equipment"
 *   SlotGrid   (Uniform Grid Panel) - filled with SlotWidgetClass
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenEquipmentPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void SetEquipment(UVaesenEquipmentComponent* InEquipment);

protected:
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UVaesenEquipmentSlotWidget> SlotWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquipmentComponent> Equipment;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> SlotGrid;

private:
	UFUNCTION()
	void Refresh();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaesenEquipmentSlotWidget>> SlotWidgets;
};
