#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenInventoryScreenWidget.generated.h"

class UTextBlock;
class UVaesenInventoryComponent;
class UVaesenInventoryPanelWidget;

/**
 * The inventory screen (opened with Tab by AVaesenPlayerController): the player's inventory, and beside it a
 * container they're looting (chest, corpse...). Dropping an item outside both panels throws it on the ground.
 * Tab, I or Escape close it; the container side closes by itself when the player walks away.
 *
 * Widget Blueprint (e.g. WBP_InventoryScreen, parent class VaesenInventoryScreenWidget), controls by these exact names:
 *   PlayerPanel     (WBP_InventoryPanel)  - the player's inventory
 *   ContainerPanel  (WBP_InventoryPanel)  - the container; collapsed when there is none
 *   HintText        (Text)                - optional controls reminder
 * The root should be a full-screen widget that's hit-testable (e.g. a lightly tinted Border), to catch drops.
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built. Make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The player's inventory, and the container beside it (or null). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetInventories(UVaesenInventoryComponent* PlayerInventory, UVaesenInventoryComponent* Container);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenInventoryPanelWidget> PlayerPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenInventoryPanelWidget> ContainerPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HintText;

private:
	TWeakObjectPtr<UVaesenInventoryComponent> PlayerInventoryShown;
	TWeakObjectPtr<UVaesenInventoryComponent> ContainerShown;
};
