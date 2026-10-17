#include "UI/Widgets/VaesenComboBox.h"

#include "UI/Style/VaesenUIStyle.h"

#define LOCTEXT_NAMESPACE "VaesenWidgets"

TSharedRef<SWidget> UVaesenComboBox::RebuildWidget()
{
	if (bUseVaesenStyle)
	{
		const UVaesenUIStyle* Style = UVaesenUIStyle::Get();
		SetWidgetStyle(Style->ComboBox);
		SetItemStyle(Style->ComboBoxItem);
		// Font and foreground colour can only be set before the Slate widget is built, which is now.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Font = Style->ComboBoxFont;
		ForegroundColor = Style->ComboBoxTextColor;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
	return Super::RebuildWidget();
}

#if WITH_EDITOR
const FText UVaesenComboBox::GetPaletteCategory()
{
	return LOCTEXT("Category", "Vaesen");
}
#endif

#undef LOCTEXT_NAMESPACE
