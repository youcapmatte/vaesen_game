#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "VaesenButton.generated.h"

/**
 * A Button in the Vaesen look. Put a Vaesen Text Block (Role = Button) inside it for the label.
 * Takes its look from the Vaesen UI style (Project Settings > Game > Vaesen > UI Style); untick Use Vaesen Style
 * to style this one by hand. Can stand in for a UButton anywhere, including C++ bindings.
 */
UCLASS(meta = (DisplayName = "Vaesen Button"))
class VAESENGAME_API UVaesenButton : public UButton
{
	GENERATED_BODY()

public:
	/** Apply the Vaesen UI style; untick to keep this control's own style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vaesen")
	bool bUseVaesenStyle = true;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
};
