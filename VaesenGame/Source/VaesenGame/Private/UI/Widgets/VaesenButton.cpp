#include "UI/Widgets/VaesenButton.h"

#include "UI/Style/VaesenUIStyle.h"

#define LOCTEXT_NAMESPACE "VaesenWidgets"

TSharedRef<SWidget> UVaesenButton::RebuildWidget()
{
	if (bUseVaesenStyle)
	{
		const UVaesenUIStyle* Style = UVaesenUIStyle::Get();
		SetStyle(Style->Button);
	}
	return Super::RebuildWidget();
}

#if WITH_EDITOR
const FText UVaesenButton::GetPaletteCategory()
{
	return LOCTEXT("Category", "Vaesen");
}
#endif

#undef LOCTEXT_NAMESPACE
