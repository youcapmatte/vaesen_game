#include "UI/Login/VaesenLoginWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/VaesenApiSubsystem.h"

#define LOCTEXT_NAMESPACE "VaesenLogin"

namespace VaesenLogin
{
	/** Last choice is kept in the per-user GameUserSettings.ini so restarts start where you left off. */
	const TCHAR* ConfigSection = TEXT("Vaesen.DevLogin");
	const TCHAR* LastGuildKey = TEXT("LastGuildId");
	const TCHAR* LastPlayerKey = TEXT("LastPlayerId");
}

void UVaesenLoginWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	GuildComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenLoginWidget::HandleGuildSelected);
	PlayerComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenLoginWidget::HandlePlayerSelected);
	LoginButton->OnClicked.AddDynamic(this, &UVaesenLoginWidget::HandleLoginClicked);
}

void UVaesenLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadGuilds();
}

void UVaesenLoginWidget::LoadGuilds()
{
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api)
	{
		return;
	}

	GuildComboBox->ClearOptions();
	PlayerComboBox->ClearOptions();
	UpdateLoginButton();
	SetStatus(LOCTEXT("Loading", "Loading..."));

	Api->GetDevGuilds([WeakThis = TWeakObjectPtr<UVaesenLoginWidget>(this)](bool bSuccess, const TArray<FVaesenDevGuild>& LoadedGuilds, const FString& Error)
	{
		UVaesenLoginWidget* This = WeakThis.Get();
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

		FString LastGuildId;
		FString LastPlayerId;
		GConfig->GetString(VaesenLogin::ConfigSection, VaesenLogin::LastGuildKey, LastGuildId, GGameUserSettingsIni);
		GConfig->GetString(VaesenLogin::ConfigSection, VaesenLogin::LastPlayerKey, LastPlayerId, GGameUserSettingsIni);

		int32 GuildIndexToSelect = 0;
		for (int32 Index = 0; Index < This->Guilds.Num(); ++Index)
		{
			This->GuildComboBox->AddOption(This->Guilds[Index].Name);
			if (This->Guilds[Index].Id == LastGuildId)
			{
				GuildIndexToSelect = Index;
			}
		}

		if (This->Guilds.IsValidIndex(GuildIndexToSelect))
		{
			// Selecting directly (not via HandleGuildSelected) so the remembered player can be restored.
			This->GuildComboBox->SetSelectedIndex(GuildIndexToSelect);
			This->PopulatePlayers(LastPlayerId);
		}
	});
}

void UVaesenLoginWidget::HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct)
	{
		PopulatePlayers(FString());
	}
}

void UVaesenLoginWidget::HandlePlayerSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateLoginButton();
}

void UVaesenLoginWidget::PopulatePlayers(const FString& PlayerIdToSelect)
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

	UpdateLoginButton();
}

void UVaesenLoginWidget::HandleLoginClicked()
{
	const FVaesenDevGuild* Guild = GetSelectedGuild();
	const FVaesenDevPlayer* Player = GetSelectedPlayer();
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Guild || !Player || !Api || bLoggingIn)
	{
		return;
	}

	GConfig->SetString(VaesenLogin::ConfigSection, VaesenLogin::LastGuildKey, *Guild->Id, GGameUserSettingsIni);
	GConfig->SetString(VaesenLogin::ConfigSection, VaesenLogin::LastPlayerKey, *Player->Id, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	bLoggingIn = true;
	UpdateLoginButton();
	SetStatus(FText::Format(LOCTEXT("LoggingIn", "Logging in as {0}..."), FText::FromString(Player->Name)));

	Api->DevLogin(Player->Id, [WeakThis = TWeakObjectPtr<UVaesenLoginWidget>(this)](bool bSuccess, const FString& Error)
	{
		UVaesenLoginWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (bSuccess)
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(This, UVaesenSettings::Get()->CharacterSelectMap);
			return;
		}

		This->bLoggingIn = false;
		This->UpdateLoginButton();
		This->SetStatus(FText::FromString(Error));
	});
}

const FVaesenDevGuild* UVaesenLoginWidget::GetSelectedGuild() const
{
	const int32 Index = GuildComboBox->GetSelectedIndex();
	return Guilds.IsValidIndex(Index) ? &Guilds[Index] : nullptr;
}

const FVaesenDevPlayer* UVaesenLoginWidget::GetSelectedPlayer() const
{
	const FVaesenDevGuild* Guild = GetSelectedGuild();
	const int32 Index = PlayerComboBox->GetSelectedIndex();
	return Guild && Guild->Players.IsValidIndex(Index) ? &Guild->Players[Index] : nullptr;
}

void UVaesenLoginWidget::UpdateLoginButton()
{
	LoginButton->SetIsEnabled(!bLoggingIn && GetSelectedPlayer() != nullptr);
}

void UVaesenLoginWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
