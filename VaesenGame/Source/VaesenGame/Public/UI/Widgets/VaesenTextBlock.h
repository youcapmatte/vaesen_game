#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "UI/Style/VaesenUIStyle.h"
#include "VaesenTextBlock.generated.h"

/**
 * A Text Block in the Vaesen look; Role picks the font and colour (Title, Heading, Body, Label, Button, Status).
 * Takes its look from the Vaesen UI style (Project Settings > Game > Vaesen > UI Style); untick Use Vaesen Style
 * to style this one by hand. Can stand in for a UTextBlock anywhere, including C++ bindings.
 */
UCLASS(meta = (DisplayName = "Vaesen Text Block"))
class VAESENGAME_API UVaesenTextBlock : public UTextBlock
{
	GENERATED_BODY()

public:
	/** Apply the Vaesen UI style; untick to keep this control's own style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vaesen")
	bool bUseVaesenStyle = true;

	/** Which text style from the Vaesen UI style to use. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vaesen", meta = (EditCondition = "bUseVaesenStyle"))
	EVaesenTextRole Role = EVaesenTextRole::Body;

	/** Change the role at runtime, e.g. Status for an error message. */
	UFUNCTION(BlueprintCallable, Category = "Vaesen")
	void SetRole(EVaesenTextRole InRole);

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void ApplyRole();
};
