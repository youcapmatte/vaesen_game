#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "VaesenPlayerController.generated.h"

class UInputMappingContext;
class UVaesenInventoryComponent;
class UVaesenInventoryScreenWidget;

/**
 * Player controller used in the worlds. Adds the default Enhanced Input mapping for the local player, and shows the
 * inventory screen (Project Settings > Vaesen > Inventory Screen Widget): the mouse is free and the view stays
 * still while it's open, but the character can still walk.
 */
UCLASS()
class VAESENGAME_API AVaesenPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Opens the player's inventory, with Container (e.g. a chest) beside it if given. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OpenInventory(UVaesenInventoryComponent* Container = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ToggleInventory();

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsInventoryOpen() const;

protected:
	virtual void BeginPlay() override;

	/** Gameplay input mapping; assign in the Blueprint subclass (e.g. IMC_Default). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

private:
	UPROPERTY(Transient)
	TObjectPtr<UVaesenInventoryScreenWidget> InventoryScreen;
};
