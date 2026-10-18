#pragma once

#include "CoreMinimal.h"
#include "Items/VaesenEquippableItem.h"
#include "VaesenToolItem.generated.h"

/** Something held and used but not used up: lantern, spyglass, compass, lockpicks, crowbar. Off hand by default. */
UCLASS(Abstract)
class VAESENGAME_API UVaesenToolItem : public UVaesenEquippableItem
{
	GENERATED_BODY()

public:
	UVaesenToolItem();

	/** What it uses up while working, if anything (e.g. lamp oil for a lantern). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool")
	TSubclassOf<UVaesenItem> RequiredItem;

	/** How long one RequiredItem lasts while in use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tool", meta = (ClampMin = "0", Units = "s"))
	float SecondsPerRequiredItem = 600.0f;
};
