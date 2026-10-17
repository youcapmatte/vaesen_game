#include "UI/Style/VaesenUIStyle.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Core/VaesenSettings.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

namespace VaesenUIStyle
{
	// Dark, slightly warm panels with a brass outline: 19th-century, candle-lit.
	const FLinearColor Parchment(0.93f, 0.88f, 0.78f);
	const FLinearColor Brass(0.62f, 0.50f, 0.32f);
	const FLinearColor BrassBright(0.85f, 0.70f, 0.45f);
	const FLinearColor Panel(0.06f, 0.05f, 0.04f, 0.85f);
	const FLinearColor PanelHovered(0.12f, 0.10f, 0.07f, 0.95f);
	const FLinearColor PanelPressed(0.03f, 0.025f, 0.02f, 1.0f);
	const FLinearColor PanelDisabled(0.05f, 0.05f, 0.05f, 0.5f);
	const FLinearColor Muted(0.45f, 0.43f, 0.40f);
	constexpr float Radius = 6.0f;

	FSlateRoundedBoxBrush Box(const FLinearColor& Fill, const FLinearColor& Outline, float OutlineWidth = 1.0f)
	{
		return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
	}

	FVaesenTextStyle Text(UObject* FontObject, int32 Size, FName Typeface, const FLinearColor& Color)
	{
		FVaesenTextStyle Style;
		Style.Font = FSlateFontInfo(FontObject, Size, Typeface);
		Style.Color = Color;
		return Style;
	}
}

UVaesenUIStyle::UVaesenUIStyle()
{
	using namespace VaesenUIStyle;

	// Widgets don't exist on a dedicated server, and the engine font and widget defaults may not be loaded there.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	static ConstructorHelpers::FObjectFinder<UFont> RobotoFont(*UWidget::GetDefaultFontName());
	UObject* Font = RobotoFont.Object;

	TitleText = Text(Font, 40, TEXT("Bold"), Parchment);
	HeadingText = Text(Font, 26, TEXT("Bold"), Parchment);
	BodyText = Text(Font, 18, TEXT("Regular"), FLinearColor(0.85f, 0.82f, 0.76f));
	LabelText = Text(Font, 16, TEXT("Regular"), FLinearColor(0.70f, 0.66f, 0.58f));
	ButtonText = Text(Font, 18, TEXT("Bold"), Parchment);
	StatusText = Text(Font, 16, TEXT("Italic"), BrassBright);

	Button = GetDefault<UButton>()->GetStyle();
	Button.Normal = Box(Panel, Brass);
	Button.Hovered = Box(PanelHovered, BrassBright);
	Button.Pressed = Box(PanelPressed, BrassBright);
	Button.Disabled = Box(PanelDisabled, Muted);
	Button.NormalForeground = Parchment;
	Button.HoveredForeground = FLinearColor::White;
	Button.PressedForeground = BrassBright;
	Button.DisabledForeground = Muted;
	Button.NormalPadding = FMargin(16.0f, 8.0f);
	Button.PressedPadding = FMargin(16.0f, 9.0f, 16.0f, 7.0f);

	TextBox = GetDefault<UEditableTextBox>()->WidgetStyle;
	TextBox.BackgroundImageNormal = Box(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), Brass);
	TextBox.BackgroundImageHovered = Box(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f), BrassBright);
	TextBox.BackgroundImageFocused = Box(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f), BrassBright);
	TextBox.BackgroundImageReadOnly = Box(PanelDisabled, Muted);
	TextBox.BackgroundColor = FLinearColor::White;
	TextBox.Padding = FMargin(10.0f, 6.0f);
	TextBox.TextStyle.SetFont(BodyText.Font);
	TextBox.TextStyle.SetColorAndOpacity(FLinearColor::White);
	TextBox.ForegroundColor = FLinearColor::White;
	TextBox.FocusedForegroundColor = FLinearColor::White;
	TextBox.ReadOnlyForegroundColor = Muted;

	const UComboBoxString* DefaultComboBox = GetDefault<UComboBoxString>();
	ComboBox = DefaultComboBox->GetWidgetStyle();
	ComboBox.ComboButtonStyle.ButtonStyle = Button;
	ComboBox.ComboButtonStyle.ButtonStyle.NormalPadding = FMargin(10.0f, 6.0f);
	ComboBox.ComboButtonStyle.ButtonStyle.PressedPadding = FMargin(10.0f, 7.0f, 10.0f, 5.0f);
	ComboBox.ComboButtonStyle.MenuBorderBrush = Box(FLinearColor(0.04f, 0.035f, 0.03f, 0.97f), Brass);
	ComboBox.ComboButtonStyle.MenuBorderPadding = FMargin(2.0f);

	ComboBoxItem = DefaultComboBox->GetItemStyle();
	ComboBoxItem.EvenRowBackgroundBrush = FSlateColorBrush(FLinearColor::Transparent);
	ComboBoxItem.OddRowBackgroundBrush = FSlateColorBrush(FLinearColor::Transparent);
	ComboBoxItem.EvenRowBackgroundHoveredBrush = FSlateColorBrush(PanelHovered);
	ComboBoxItem.OddRowBackgroundHoveredBrush = FSlateColorBrush(PanelHovered);
	ComboBoxItem.ActiveBrush = FSlateColorBrush(FLinearColor(0.30f, 0.22f, 0.12f));
	ComboBoxItem.ActiveHoveredBrush = FSlateColorBrush(FLinearColor(0.38f, 0.28f, 0.15f));
	ComboBoxItem.InactiveBrush = ComboBoxItem.ActiveBrush;
	ComboBoxItem.InactiveHoveredBrush = ComboBoxItem.ActiveHoveredBrush;
	ComboBoxItem.TextColor = Parchment;
	ComboBoxItem.SelectedTextColor = FLinearColor::White;

	ComboBoxFont = BodyText.Font;
	ComboBoxTextColor = Parchment;
}

const UVaesenUIStyle* UVaesenUIStyle::Get()
{
	if (const UVaesenUIStyle* Style = UVaesenSettings::Get()->UIStyle.LoadSynchronous())
	{
		return Style;
	}
	return GetDefault<UVaesenUIStyle>();
}

const FVaesenTextStyle& UVaesenUIStyle::GetTextStyle(EVaesenTextRole Role) const
{
	switch (Role)
	{
	case EVaesenTextRole::Title:   return TitleText;
	case EVaesenTextRole::Heading: return HeadingText;
	case EVaesenTextRole::Label:   return LabelText;
	case EVaesenTextRole::Button:  return ButtonText;
	case EVaesenTextRole::Status:  return StatusText;
	default:                       return BodyText;
	}
}
