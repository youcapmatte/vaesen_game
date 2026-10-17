#include "UI/Loading/VaesenLoadingScreenWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

void UVaesenLoadingScreenWidget::SetBackground(UTexture2D* Texture)
{
	if (BackgroundImage)
	{
		BackgroundImage->SetBrushFromTexture(Texture);
		BackgroundImage->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UVaesenLoadingScreenWidget::SetMapName(const FText& MapName)
{
	if (MapNameText)
	{
		MapNameText->SetText(MapName);
	}
}

void UVaesenLoadingScreenWidget::SetTip(const FText& Tip)
{
	if (TipText)
	{
		TipText->SetText(Tip);
	}
}

void UVaesenLoadingScreenWidget::SetStatus(const FText& Status)
{
	if (StatusText)
	{
		StatusText->SetText(Status);
	}
}

void UVaesenLoadingScreenWidget::SetProgress(float Progress)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	}
}
