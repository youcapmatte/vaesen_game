#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenEquipmentPanelWidget.generated.h"

class AVaesenCharacterPreview;
class UImage;
class UPanelWidget;
class UTextBlock;
class UVaesenEquipmentComponent;
class UVaesenEquipmentSlotWidget;

/**
 * The player's equipment as a paper doll, like The Witcher 3: the character itself, live in an idle pose and wearing
 * what's equipped (AVaesenCharacterPreview), with the armor slots down one side (Head, Chest, Hands, Legs, Feet) and
 * the rest down the other (Back, Main Hand, Off Hand, Talisman, Bag). Drag on the character to turn it. Dropping an
 * item on the panel or the character (not on a slot) puts it on in its own slot.
 *
 * The preview is spawned on this player's machine while the panel is on screen and removed with it.
 *
 * Widget Blueprint (e.g. WBP_EquipmentPanel, parent class VaesenEquipmentPanelWidget), controls by these exact names:
 *   TitleText     (Text)                 - e.g. "Equipment"
 *   LeftSlots     (any panel, e.g. Vertical Box) - filled with the armor slots
 *   RightSlots    (any panel)            - filled with the other slots
 *   PreviewImage  (Image)                - shows the character
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UVaesenEquipmentSlotWidget> SlotWidgetClass;

	/** The studio the character is shown in (make a Blueprint of it to tune lights and framing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<AVaesenCharacterPreview> PreviewClass;

	/** Degrees the character turns per pixel dragged. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	float TurnSpeed = 0.5f;

	/** Gap between slots in a column. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	float SlotSpacing = 6.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquipmentComponent> Equipment;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> LeftSlots;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> RightSlots;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PreviewImage;

private:
	/** Slots and the character, after something was put on or taken off. */
	UFUNCTION()
	void Refresh();

	void SpawnPreview();
	void DestroyPreview();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVaesenEquipmentSlotWidget>> SlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<AVaesenCharacterPreview> Preview;

	bool bTurning = false;
};
