#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "VaesenComboBox.generated.h"

/**
 * A Combo Box (String) in the Vaesen look, for the box and its list.
 * Takes its look from the Vaesen UI style (Project Settings > Game > Vaesen > UI Style); untick Use Vaesen Style
 * to style this one by hand. Can stand in for a UComboBoxString anywhere, including C++ bindings.
 */
UCLASS(meta = (DisplayName = "Vaesen Combo Box"))
class VAESENGAME_API UVaesenComboBox : public UComboBoxString
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
