#include "UI/Widgets/VaesenEditableTextBox.h"

#include "UI/Style/VaesenUIStyle.h"

#define LOCTEXT_NAMESPACE "VaesenWidgets"

TSharedRef<SWidget> UVaesenEditableTextBox::RebuildWidget()
{
	if (bUseVaesenStyle)
	{
		const UVaesenUIStyle* Style = UVaesenUIStyle::Get();
		WidgetStyle = Style->TextBox;
	}
	return Super::RebuildWidget();
}

#if WITH_EDITOR
const FText UVaesenEditableTextBox::GetPaletteCategory()
{
	return LOCTEXT("Category", "Vaesen");
}
#endif

#undef LOCTEXT_NAMESPACE
