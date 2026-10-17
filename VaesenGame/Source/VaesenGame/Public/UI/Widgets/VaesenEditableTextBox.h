#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "VaesenEditableTextBox.generated.h"

/**
 * A Text Box in the Vaesen look (also for passwords: tick Is Password as usual).
 * Takes its look from the Vaesen UI style (Project Settings > Game > Vaesen > UI Style); untick Use Vaesen Style
 * to style this one by hand. Can stand in for a UEditableTextBox anywhere, including C++ bindings.
 */
UCLASS(meta = (DisplayName = "Vaesen Text Box"))
class VAESENGAME_API UVaesenEditableTextBox : public UEditableTextBox
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
