#include "UI/Widgets/VaesenTextBlock.h"

#include "UI/Style/VaesenUIStyle.h"

#define LOCTEXT_NAMESPACE "VaesenWidgets"

TSharedRef<SWidget> UVaesenTextBlock::RebuildWidget()
{
	if (bUseVaesenStyle)
	{
		ApplyRole();
	}
	return Super::RebuildWidget();
}

void UVaesenTextBlock::SetRole(EVaesenTextRole InRole)
{
	Role = InRole;
	if (bUseVaesenStyle)
	{
		ApplyRole();
	}
}

void UVaesenTextBlock::ApplyRole()
{
	const FVaesenTextStyle& TextStyle = UVaesenUIStyle::Get()->GetTextStyle(Role);
	SetFont(TextStyle.Font);
	SetColorAndOpacity(TextStyle.Color);
	SetShadowOffset(TextStyle.ShadowOffset);
	SetShadowColorAndOpacity(TextStyle.ShadowColor);
}

#if WITH_EDITOR
const FText UVaesenTextBlock::GetPaletteCategory()
{
	return LOCTEXT("Category", "Vaesen");
}
#endif

#undef LOCTEXT_NAMESPACE
