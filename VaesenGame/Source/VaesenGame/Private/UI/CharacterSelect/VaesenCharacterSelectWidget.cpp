#include "UI/CharacterSelect/VaesenCharacterSelectWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSessionSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/VaesenApiSubsystem.h"
#include "UI/CharacterSelect/VaesenCreateCharacterWidget.h"

#define LOCTEXT_NAMESPACE "VaesenCharacterSelect"

namespace VaesenCharacterSelect
{
	/** Last choice is kept in the per-user GameUserSettings.ini so restarts start where you left off. */
	const TCHAR* ConfigSection = TEXT("Vaesen.CharacterSelect");
	const TCHAR* LastGuildKey = TEXT("LastGuildId");
	const TCHAR* LastCharacterKey = TEXT("LastCharacterId");
}

void UVaesenCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Normally caught by the Widget Blueprint compiler; guards against a stale or broken Blueprint.
	if (!HasRequiredWidgets())
	{
		UE_LOG(LogVaesenApi, Error, TEXT("%s is missing one of its required widgets (see VaesenCharacterSelectWidget.h)."), *GetClass()->GetName());
		return;
	}

	GuildComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleGuildSelected);
	PlayerComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleCharacterSelected);
	PlayButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandlePlayClicked);
	CreateCharacterButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleCreateCharacterClicked);
	CreateCharacterDialog->OnCharacterCreated.AddUObject(this, &UVaesenCharacterSelectWidget::HandleCharacterCreated);
	CreateCharacterDialog->OnCancelled.AddUObject(this, &UVaesenCharacterSelectWidget::HandleCreateCancelled);
}

void UVaesenCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (HasRequiredWidgets())
	{
		CreateCharacterDialog->Close();
		LoadGuilds();
	}
}

bool UVaesenCharacterSelectWidget::HasRequiredWidgets() const
{
	return GuildComboBox && PlayerComboBox && PlayButton && CreateCharacterButton && CreateCharacterDialog;
}

void UVaesenCharacterSelectWidget::LoadGuilds()
{
	Guilds.Reset();
	Characters.Reset();
	bCharactersLoaded = false;
	GuildComboBox->ClearOptions();
	PlayerComboBox->ClearOptions();
	UpdateButtons();

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	const UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>();
	if (!Api || !Session || !Session->IsLoggedIn())
	{
		SetStatus(LOCTEXT("NotLoggedIn", "Not logged in. Start from the login screen (L_Login)."));
		return;
	}

	SetStatus(LOCTEXT("LoadingGuilds", "Loading your guilds..."));
	Api->GetMyGuilds([WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this)](bool bSuccess, const TArray<FVaesenGuild>& LoadedGuilds, const FString& Error)
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
		if (This->Guilds.IsEmpty())
		{
			This->SetStatus(LOCTEXT("NoGuilds", "You aren't a member of any guild yet."));
			return;
		}

		This->SetStatus(FText::GetEmpty());

		FString LastGuildId;
		FString LastCharacterId;
		GConfig->GetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastGuildKey, LastGuildId, GGameUserSettingsIni);
		GConfig->GetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastCharacterKey, LastCharacterId, GGameUserSettingsIni);

		int32 GuildIndexToSelect = 0;
		for (int32 Index = 0; Index < This->Guilds.Num(); ++Index)
		{
			This->GuildComboBox->AddOption(This->Guilds[Index].Name);
			if (This->Guilds[Index].Id == LastGuildId)
			{
				GuildIndexToSelect = Index;
			}
		}

		// Selecting directly (ignored by HandleGuildSelected) so the remembered character can be restored.
		This->GuildComboBox->SetSelectedIndex(GuildIndexToSelect);
		This->LoadCharacters(LastCharacterId);
	});
}

void UVaesenCharacterSelectWidget::HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectionType != ESelectInfo::Direct)
	{
		LoadCharacters(FString());
	}
}

void UVaesenCharacterSelectWidget::LoadCharacters(const FString& CharacterIdToSelect)
{
	Characters.Reset();
	bCharactersLoaded = false;
	PlayerComboBox->ClearOptions();
	UpdateButtons();

	const FVaesenGuild* Guild = GetSelectedGuild();
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Guild || !Api)
	{
		return;
	}

	if (UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>())
	{
		FVaesenGuildRef SelectedGuild;
		SelectedGuild.Id = Guild->Id;
		SelectedGuild.Name = Guild->Name;
		Session->SetSelectedGuild(SelectedGuild);
	}

	const int32 ThisLoad = ++CharacterLoad;
	SetStatus(LOCTEXT("LoadingCharacters", "Loading characters..."));
	Api->GetGuildCharacters(Guild->Id,
		[WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), ThisLoad, CharacterIdToSelect](bool bSuccess, const TArray<FVaesenCharacter>& LoadedCharacters, const FString& Error)
		{
			UVaesenCharacterSelectWidget* This = WeakThis.Get();
			if (!This || This->CharacterLoad != ThisLoad)
			{
				return;
			}

			if (!bSuccess)
			{
				This->SetStatus(FText::FromString(Error));
				return;
			}

			This->Characters = LoadedCharacters;
			This->bCharactersLoaded = true;
			This->SetStatus(This->Characters.IsEmpty()
				? LOCTEXT("NoCharacters", "This guild has no characters yet. Create one.")
				: FText::GetEmpty());

			int32 IndexToSelect = 0;
			for (int32 Index = 0; Index < This->Characters.Num(); ++Index)
			{
				This->PlayerComboBox->AddOption(This->Characters[Index].Name);
				if (This->Characters[Index].Id == CharacterIdToSelect)
				{
					IndexToSelect = Index;
				}
			}
			if (!This->Characters.IsEmpty())
			{
				This->PlayerComboBox->SetSelectedIndex(IndexToSelect);
			}

			This->UpdateButtons();
		});
}

void UVaesenCharacterSelectWidget::HandleCharacterSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandleCreateCharacterClicked()
{
	if (const FVaesenGuild* Guild = GetSelectedGuild(); Guild && bCharactersLoaded)
	{
		CreateCharacterDialog->Open(Guild->Id);
		UpdateButtons();
	}
}

void UVaesenCharacterSelectWidget::HandleCharacterCreated(const FVaesenCharacter& Character)
{
	SetStatus(FText::Format(LOCTEXT("Created", "{0} has joined the guild."), FText::FromString(Character.Name)));
	LoadCharacters(Character.Id);
}

void UVaesenCharacterSelectWidget::HandleCreateCancelled()
{
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandlePlayClicked()
{
	const FVaesenCharacter* Character = GetSelectedCharacter();
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Character || !Api || bBusy)
	{
		return;
	}

	RememberSelection();
	bBusy = true;
	UpdateButtons();
	SetStatus(FText::Format(LOCTEXT("Preparing", "Preparing {0}..."), FText::FromString(Character->Name)));

	const FString CharacterName = Character->Name;
	Api->PlayCharacter(*Character, [WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), CharacterName](bool bSuccess, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		This->bBusy = false;
		This->UpdateButtons();

		// Next step (not built yet): join the world via the API and travel to the returned server.
		This->SetStatus(bSuccess
			? FText::Format(LOCTEXT("Playing", "Playing as {0}. Entering the world isn't built yet."), FText::FromString(CharacterName))
			: FText::FromString(Error));
	});
}

const FVaesenGuild* UVaesenCharacterSelectWidget::GetSelectedGuild() const
{
	const int32 Index = GuildComboBox->GetSelectedIndex();
	return Guilds.IsValidIndex(Index) ? &Guilds[Index] : nullptr;
}

const FVaesenCharacter* UVaesenCharacterSelectWidget::GetSelectedCharacter() const
{
	const int32 Index = PlayerComboBox->GetSelectedIndex();
	return bCharactersLoaded && Characters.IsValidIndex(Index) ? &Characters[Index] : nullptr;
}

void UVaesenCharacterSelectWidget::RememberSelection() const
{
	const FVaesenGuild* Guild = GetSelectedGuild();
	const FVaesenCharacter* Character = GetSelectedCharacter();
	if (Guild && Character)
	{
		GConfig->SetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastGuildKey, *Guild->Id, GGameUserSettingsIni);
		GConfig->SetString(VaesenCharacterSelect::ConfigSection, VaesenCharacterSelect::LastCharacterKey, *Character->Id, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void UVaesenCharacterSelectWidget::UpdateButtons()
{
	const bool bDialogOpen = CreateCharacterDialog->GetVisibility() == ESlateVisibility::Visible;
	PlayButton->SetIsEnabled(!bBusy && !bDialogOpen && GetSelectedCharacter() != nullptr);
	CreateCharacterButton->SetIsEnabled(!bBusy && !bDialogOpen && GetSelectedGuild() != nullptr && bCharactersLoaded);
	GuildComboBox->SetIsEnabled(!bBusy && !bDialogOpen);
	PlayerComboBox->SetIsEnabled(!bBusy && !bDialogOpen);
}

void UVaesenCharacterSelectWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
