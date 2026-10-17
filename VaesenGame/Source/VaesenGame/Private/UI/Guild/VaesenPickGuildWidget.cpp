#include "UI/Guild/VaesenPickGuildWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "VaesenPickGuild"

void UVaesenPickGuildWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (SelectButton)
	{
		SelectButton->OnClicked.AddDynamic(this, &UVaesenPickGuildWidget::HandleSelectClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVaesenPickGuildWidget::HandleCancelClicked);
	}
}

void UVaesenPickGuildWidget::Open(const TArray<FVaesenGuild>& InGuilds, const FString& CurrentGuildId)
{
	Guilds = InGuilds;

	if (GuildComboBox)
	{
		GuildComboBox->ClearOptions();
		int32 IndexToSelect = 0;
		for (int32 Index = 0; Index < Guilds.Num(); ++Index)
		{
			GuildComboBox->AddOption(Guilds[Index].Name);
			if (Guilds[Index].Id == CurrentGuildId)
			{
				IndexToSelect = Index;
			}
		}
		if (!Guilds.IsEmpty())
		{
			GuildComboBox->SetSelectedIndex(IndexToSelect);
		}
	}

	if (SelectButton)
	{
		SelectButton->SetIsEnabled(!Guilds.IsEmpty());
	}
	if (StatusText)
	{
		StatusText->SetText(Guilds.IsEmpty() ? LOCTEXT("NoGuilds", "You aren't a member of any guild yet.") : FText::GetEmpty());
	}

	SetVisibility(ESlateVisibility::Visible);
}

void UVaesenPickGuildWidget::Close()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

void UVaesenPickGuildWidget::HandleSelectClicked()
{
	const int32 Index = GuildComboBox ? GuildComboBox->GetSelectedIndex() : INDEX_NONE;
	if (!Guilds.IsValidIndex(Index))
	{
		return;
	}

	const FVaesenGuild Picked = Guilds[Index];
	Close();
	OnGuildPicked.Broadcast(Picked);
}

void UVaesenPickGuildWidget::HandleCancelClicked()
{
	Close();
	OnCancelled.Broadcast();
}

#undef LOCTEXT_NAMESPACE
