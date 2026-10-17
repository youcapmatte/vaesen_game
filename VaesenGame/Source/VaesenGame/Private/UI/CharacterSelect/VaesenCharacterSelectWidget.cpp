#include "UI/CharacterSelect/VaesenCharacterSelectWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Online/VaesenApiSubsystem.h"

#define LOCTEXT_NAMESPACE "VaesenCharacterSelect"

namespace VaesenCharacterSelect
{
	/** Last choice is kept in the per-user GameUserSettings.ini so restarts start where you left off. */
	const TCHAR* ConfigSection = TEXT("Vaesen.CharacterSelect");
	const TCHAR* LastGuildKey = TEXT("LastGuildId");
	const TCHAR* LastPlayerKey = TEXT("LastPlayerId");
}

void UVaesenCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	GuildComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleGuildSelected);
	PlayerComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenCharacterSelectWidget::HandlePlayerSelected);
	PlayButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandlePlayClicked);
}

void UVaesenCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadGuilds();
}

void UVaesenCharacterSelectWidget::LoadGuilds()
{
	GuildComboBox->ClearOptions();
	PlayerComboBox->ClearOptions();
	UpdatePlayButton();

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api)
	{
		return;
	}
	if (!Api->IsLoggedIn())
	{
		SetStatus(LOCTEXT("NotLoggedIn", "Not logged in. Start from the login screen (L_Login)."));
		return;
	}

	SetStatus(LOCTEXT("Loading", "Loading characters..."));

	Api->GetDevGuilds([WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this)](bool bSuccess, const TArray<FVaesenDevGuild>& LoadedGuilds, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (!bSuccess)
		{
			This->SetStatus(FText::FromString(Error));
			return;
		}

		This->Guilds = LoadedGuilds;
		This->SetStatus(FText::GetEmpty());

		// -DevPlayer wins over the remembered choice.
		FString PlayerIdToSelect;
		FString GuildIdToSelect;
		if (!FParse::Value(FCommandLine::Get(), TEXT("DevPlayer="), PlayerIdToSelect))
		{
			GConfig->GetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastGuildKey, GuildIdToSelect, GGameUserSettingsIni);
			GConfig->GetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastPlayerKey, PlayerIdToSelect, GGameUserSettingsIni);
		}

		int32 GuildIndexToSelect = 0;
		for (int32 GuildIndex = 0; GuildIndex < This->Guilds.Num(); ++GuildIndex)
		{
			const FVaesenDevGuild& Guild = This->Guilds[GuildIndex];
			This->GuildComboBox->AddOption(Guild.Name);

			const bool bHasPlayer = Guild.Players.ContainsByPredicate([&](const FVaesenDevPlayer& Player) { return Player.Id == PlayerIdToSelect; });
			if (Guild.Id == GuildIdToSelect || bHasPlayer)
			{
				GuildIndexToSelect = GuildIndex;
			}
		}

		if (This->Guilds.IsValidIndex(GuildIndexToSelect))
		{
			// Selecting directly (ignored by HandleGuildSelected) so the wanted character can be restored.
			This->GuildComboBox->SetSelectedIndex(GuildIndexToSelect);
			This->PopulatePlayers(PlayerIdToSelect);
		}
	});
}

void UVaesenCharacterSelectWidget::HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct)
	{
		PopulatePlayers(FString());
	}
}

void UVaesenCharacterSelectWidget::HandlePlayerSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdatePlayButton();
}

void UVaesenCharacterSelectWidget::PopulatePlayers(const FString& PlayerIdToSelect)
{
	PlayerComboBox->ClearOptions();

	if (const FVaesenDevGuild* Guild = GetSelectedGuild())
	{
		int32 IndexToSelect = 0;
		for (int32 Index = 0; Index < Guild->Players.Num(); ++Index)
		{
			PlayerComboBox->AddOption(Guild->Players[Index].Name);
			if (Guild->Players[Index].Id == PlayerIdToSelect)
			{
				IndexToSelect = Index;
			}
		}
		if (Guild->Players.Num() > 0)
		{
			PlayerComboBox->SetSelectedIndex(IndexToSelect);
		}
	}

	UpdatePlayButton();
}

void UVaesenCharacterSelectWidget::HandlePlayClicked()
{
	const FVaesenDevGuild* Guild = GetSelectedGuild();
	const FVaesenDevPlayer* Player = GetSelectedPlayer();
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Guild || !Player || !Api || bBusy)
	{
		return;
	}

	GConfig->SetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastGuildKey, *Guild->Id, GGameUserSettingsIni);
	GConfig->SetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastPlayerKey, *Player->Id, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	bBusy = true;
	UpdatePlayButton();
	SetStatus(FText::Format(LOCTEXT("Selecting", "Preparing {0}..."), FText::FromString(Player->Name)));

	Api->SelectDevCharacter(Player->Id, [WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this)](bool bSuccess, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		This->bBusy = false;
		This->UpdatePlayButton();

		if (!bSuccess)
		{
			This->SetStatus(FText::FromString(Error));
			return;
		}

		// Next step (not built yet): join the world via the API and travel to the returned server.
		const FVaesenPlayerSession& Session = This->GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>()->GetSession();
		This->SetStatus(FText::Format(LOCTEXT("Selected", "Playing as {0}. Entering the world isn't built yet."), FText::FromString(Session.PlayerName)));
	});
}

const FVaesenDevGuild* UVaesenCharacterSelectWidget::GetSelectedGuild() const
{
	const int32 Index = GuildComboBox->GetSelectedIndex();
	return Guilds.IsValidIndex(Index) ? &Guilds[Index] : nullptr;
}

const FVaesenDevPlayer* UVaesenCharacterSelectWidget::GetSelectedPlayer() const
{
	const FVaesenDevGuild* Guild = GetSelectedGuild();
	const int32 Index = PlayerComboBox->GetSelectedIndex();
	return Guild && Guild->Players.IsValidIndex(Index) ? &Guild->Players[Index] : nullptr;
}

void UVaesenCharacterSelectWidget::UpdatePlayButton()
{
	PlayButton->SetIsEnabled(!bBusy && GetSelectedPlayer() != nullptr);
}

void UVaesenCharacterSelectWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
