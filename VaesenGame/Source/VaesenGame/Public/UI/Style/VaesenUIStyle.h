#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateTypes.h"
#include "VaesenUIStyle.generated.h"

/** What a piece of text is for; each role has its own font and colour in the UI style. */
UENUM(BlueprintType)
enum class EVaesenTextRole : uint8
{
	Title,
	Heading,
	Body,
	Label,
	Button,
	Status
};

/** Font, colour and shadow for one text role. */
USTRUCT(BlueprintType)
struct FVaesenTextStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo Font;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVector2D ShadowOffset = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);
};

/**
 * The look shared by all Vaesen controls (Vaesen Button, Vaesen Text Block, Vaesen Text Box, Vaesen Combo Box).
 * Create a Data Asset of this class (e.g. DA_VaesenUIStyle) and pick it in Project Settings > Game > Vaesen > UI Style;
 * without one, the defaults below are used. Widget Blueprints pick up changes when they are recompiled or reopened.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenUIStyle : public UDataAsset
{
	GENERATED_BODY()

public:
	UVaesenUIStyle();

	/** The style asset chosen in the project settings, else the built-in defaults. */
	static const UVaesenUIStyle* Get();

	const FVaesenTextStyle& GetTextStyle(EVaesenTextRole Role) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Button")
	FButtonStyle Button;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle TitleText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle HeadingText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle BodyText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle LabelText;

	/** Text inside buttons (a Vaesen Text Block with Role = Button). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle ButtonText;

	/** Status and error messages. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FVaesenTextStyle StatusText;

	/** Text boxes, including password boxes. The font and text colour are under Text Style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text Box")
	FEditableTextBoxStyle TextBox;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Box")
	FComboBoxStyle ComboBox;

	/** Rows in the open combo box list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Box")
	FTableRowStyle ComboBoxItem;

	/** Font of the selected item and the list rows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Box")
	FSlateFontInfo ComboBoxFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Box")
	FSlateColor ComboBoxTextColor = FLinearColor::White;
};
