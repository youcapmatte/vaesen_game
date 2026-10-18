#include "UI/CharacterSelect/VaesenCharacterSelectWidget.h"

#include "Components/Button.h"
#include "Components/CircularThrobber.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSessionSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Online/VaesenApiSubsystem.h"
#include "UI/CharacterSelect/VaesenCreateCharacterWidget.h"
#include "UI/Loading/VaesenLoadingScreenSubsystem.h"
#include "UI/Guild/VaesenCreateGuildWidget.h"
#include "UI/Guild/VaesenPickGuildWidget.h"

#define LOCTEXT_NAMESPACE "VaesenCharacterSelect"

namespace VaesenCharacterSelect
{
	/** Last choice is kept in the per-user GameUserSettings.ini so the screen opens where you left off. */
	const TCHAR* ConfigSection = TEXT("Vaesen.CharacterSelect");
	const TCHAR* LastGuildKey = TEXT("LastGuildId");
	const TCHAR* LastCharacterKey = TEXT("LastCharacterId");

	FString Remembered(const TCHAR* Key)
	{
		FString Value;
		GConfig->GetString(ConfigSection, Key, Value, GGameUserSettingsIni);
		return Value;
	}

	void Remember(const TCHAR* Key, const FString& Value)
	{
		GConfig->SetString(ConfigSection, Key, *Value, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	void SetEnabled(UWidget* Widget, bool bEnabled)
	{
		if (Widget)
		{
			Widget->SetIsEnabled(bEnabled);
		}
	}

	bool IsShown(const UWidget* Widget)
	{
		return Widget && Widget->GetVisibility() == ESlateVisibility::Visible;
	}
}

void UVaesenCharacterSelectWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (ChangeGuildButton)
	{
		ChangeGuildButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleChangeGuildClicked);
	}
	if (CreateGuildButton)
	{
		CreateGuildButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleCreateGuildClicked);
	}
	if (PlayerComboBox)
	{
		PlayerComboBox->OnSelectionChanged.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleCharacterSelected);
	}
	if (PlayButton)
	{
		PlayButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandlePlayClicked);
	}
	if (CreateCharacterButton)
	{
		CreateCharacterButton->OnClicked.AddDynamic(this, &UVaesenCharacterSelectWidget::HandleCreateCharacterClicked);
	}
	if (PickGuildDialog)
	{
		PickGuildDialog->OnGuildPicked.AddUObject(this, &UVaesenCharacterSelectWidget::HandleGuildPicked);
		PickGuildDialog->OnCancelled.AddUObject(this, &UVaesenCharacterSelectWidget::HandleDialogClosed);
	}
	if (CreateGuildDialog)
	{
		CreateGuildDialog->OnGuildCreated.AddUObject(this, &UVaesenCharacterSelectWidget::HandleGuildCreated);
		CreateGuildDialog->OnCancelled.AddUObject(this, &UVaesenCharacterSelectWidget::HandleDialogClosed);
	}
	if (CreateCharacterDialog)
	{
		CreateCharacterDialog->OnCharacterCreated.AddUObject(this, &UVaesenCharacterSelectWidget::HandleCharacterCreated);
		CreateCharacterDialog->OnCancelled.AddUObject(this, &UVaesenCharacterSelectWidget::HandleDialogClosed);
	}
}

void UVaesenCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (PickGuildDialog)
	{
		PickGuildDialog->Close();
	}
	if (CreateGuildDialog)
	{
		CreateGuildDialog->Close();
	}
	if (CreateCharacterDialog)
	{
		CreateCharacterDialog->Close();
	}

	PendingLoads = 0;
	if (LoadingThrobber)
	{
		LoadingThrobber->SetVisibility(ESlateVisibility::Collapsed);
	}

	LoadGuilds(FString());

	if (RosterRefreshSeconds > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RosterRefreshTimer, this, &UVaesenCharacterSelectWidget::RefreshRoster, RosterRefreshSeconds, true);
		}
	}
}

void UVaesenCharacterSelectWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RosterRefreshTimer);
		World->GetTimerManager().ClearTimer(EnterRetryTimer);
	}
	Super::NativeDestruct();
}

void UVaesenCharacterSelectWidget::LoadGuilds(const FString& GuildIdToSelect)
{
	Guilds.Reset();
	bGuildsLoaded = false;
	UpdateButtons();

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	const UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>();
	if (!Api || !Session || !Session->IsLoggedIn())
	{
		SetStatus(LOCTEXT("NotLoggedIn", "Not logged in. Start from the login screen (L_Login)."));
		return;
	}

	SetStatus(LOCTEXT("LoadingGuilds", "Loading your guilds..."));
	BeginLoading();
	Api->GetMyGuilds([WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), GuildIdToSelect](bool bSuccess, const TArray<FVaesenGuild>& LoadedGuilds, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		This->EndLoading();

		if (!bSuccess)
		{
			This->SetStatus(FText::FromString(Error));
			return;
		}

		This->Guilds = LoadedGuilds;
		This->bGuildsLoaded = true;
		if (This->Guilds.IsEmpty())
		{
			This->SelectedGuildId.Reset();
			This->Roster.Reset();
			This->bCharactersLoaded = false;
			if (This->GuildNameText)
			{
				This->GuildNameText->SetText(FText::GetEmpty());
			}
			This->SetStatus(LOCTEXT("NoGuilds", "You aren't a member of any guild yet. Create one."));
			This->UpdateButtons();
			return;
		}

		// The requested guild, else the one used last on this machine, else the first.
		const FString Wanted = GuildIdToSelect.IsEmpty() ? VaesenCharacterSelect::Remembered(VaesenCharacterSelect::LastGuildKey) : GuildIdToSelect;
		const FVaesenGuild* Guild = This->Guilds.FindByPredicate([&Wanted](const FVaesenGuild& G) { return G.Id == Wanted; });
		This->SelectGuild(Guild ? *Guild : This->Guilds[0], VaesenCharacterSelect::Remembered(VaesenCharacterSelect::LastCharacterKey));
	});
}

void UVaesenCharacterSelectWidget::SelectGuild(const FVaesenGuild& Guild, const FString& CharacterIdToSelect)
{
	SelectedGuildId = Guild.Id;
	VaesenCharacterSelect::Remember(VaesenCharacterSelect::LastGuildKey, Guild.Id);

	if (GuildNameText)
	{
		GuildNameText->SetText(FText::FromString(Guild.Name));
	}

	if (UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>())
	{
		FVaesenGuildRef SelectedGuild;
		SelectedGuild.Id = Guild.Id;
		SelectedGuild.Name = Guild.Name;
		Session->SetSelectedGuild(SelectedGuild);
	}

	LoadCharacters(CharacterIdToSelect);
}

void UVaesenCharacterSelectWidget::LoadCharacters(const FString& CharacterIdToSelect)
{
	Roster.Reset();
	bCharactersLoaded = false;
	if (PlayerComboBox)
	{
		PlayerComboBox->ClearOptions();
	}
	UpdateButtons();

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (SelectedGuildId.IsEmpty() || !Api)
	{
		return;
	}

	const int32 ThisLoad = ++CharacterLoad;
	SetStatus(LOCTEXT("LoadingCharacters", "Loading characters..."));
	BeginLoading();
	Api->GetGuildRoster(SelectedGuildId,
		[WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), ThisLoad, CharacterIdToSelect](bool bSuccess, const TArray<FVaesenRosterEntry>& LoadedRoster, const FString& Error)
		{
			UVaesenCharacterSelectWidget* This = WeakThis.Get();
			if (!This)
			{
				return;
			}

			// Every request ends its loading, even one whose answer is outdated.
			This->EndLoading();
			if (This->CharacterLoad != ThisLoad)
			{
				return;
			}

			if (!bSuccess)
			{
				This->SetStatus(FText::FromString(Error));
				return;
			}

			This->bCharactersLoaded = true;
			This->SetStatus(LoadedRoster.IsEmpty()
				? LOCTEXT("NoCharacters", "This guild has no characters yet. Create one.")
				: FText::GetEmpty());
			This->ShowRoster(LoadedRoster, CharacterIdToSelect);
		});
}

void UVaesenCharacterSelectWidget::RefreshRoster()
{
	// Don't change the list under the player: not while loading, in a dialog or with the list open.
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api || SelectedGuildId.IsEmpty() || !bCharactersLoaded || IsLoading() || IsDialogOpen() || (PlayerComboBox && PlayerComboBox->IsOpen()))
	{
		return;
	}

	const int32 ThisLoad = CharacterLoad;
	Api->GetGuildRoster(SelectedGuildId,
		[WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), ThisLoad](bool bSuccess, const TArray<FVaesenRosterEntry>& LoadedRoster, const FString& Error)
		{
			UVaesenCharacterSelectWidget* This = WeakThis.Get();
			// Quiet: a failed refresh keeps the current list; a newer load (another guild) wins.
			if (!This || !bSuccess || This->CharacterLoad != ThisLoad || This->IsLoading() || (This->PlayerComboBox && This->PlayerComboBox->IsOpen()))
			{
				return;
			}

			const FVaesenCharacter* Selected = This->GetSelectedCharacter();
			This->ShowRoster(LoadedRoster, Selected ? Selected->Id : FString());
		});
}

void UVaesenCharacterSelectWidget::ShowRoster(const TArray<FVaesenRosterEntry>& LoadedRoster, const FString& CharacterIdToSelect)
{
	Roster = LoadedRoster;

	if (PlayerComboBox)
	{
		int32 IndexToSelect = 0;
		TArray<FString> Labels;
		for (int32 Index = 0; Index < Roster.Num(); ++Index)
		{
			Labels.Add(GetRosterLabel(Roster[Index]));
			if (Roster[Index].Character.Id == CharacterIdToSelect)
			{
				IndexToSelect = Index;
			}
		}

		// Only rebuild when something changed, so a quiet refresh doesn't flicker.
		bool bSame = PlayerComboBox->GetOptionCount() == Labels.Num() && PlayerComboBox->GetSelectedIndex() == IndexToSelect;
		for (int32 Index = 0; bSame && Index < Labels.Num(); ++Index)
		{
			bSame = PlayerComboBox->GetOptionAtIndex(Index) == Labels[Index];
		}
		if (!bSame)
		{
			PlayerComboBox->ClearOptions();
			for (const FString& Label : Labels)
			{
				PlayerComboBox->AddOption(Label);
			}
			if (!Labels.IsEmpty())
			{
				PlayerComboBox->SetSelectedIndex(IndexToSelect);
			}
		}
	}

	UpdateButtons();
}

FString UVaesenCharacterSelectWidget::GetRosterLabel(const FVaesenRosterEntry& Entry) const
{
	if (Entry.IsFree())
	{
		return Entry.Character.Name;
	}
	if (CanPlay(Entry))
	{
		return FText::Format(LOCTEXT("PlayedByYou", "{0} (you)"), FText::FromString(Entry.Character.Name)).ToString();
	}
	const FString Player = Entry.PlayerUsername.IsEmpty() ? LOCTEXT("SomeoneElse", "another member").ToString() : Entry.PlayerUsername;
	return FText::Format(LOCTEXT("PlayedBy", "{0} \u2014 played by {1}"), FText::FromString(Entry.Character.Name), FText::FromString(Player)).ToString();
}

bool UVaesenCharacterSelectWidget::CanPlay(const FVaesenRosterEntry& Entry) const
{
	const UVaesenSessionSubsystem* Session = GetGameInstance()->GetSubsystem<UVaesenSessionSubsystem>();
	return Entry.IsFree() || (Session && Entry.PlayerAccountId == Session->GetAccount().Id);
}

void UVaesenCharacterSelectWidget::HandleCharacterSelected(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (const FVaesenCharacter* Character = GetSelectedCharacter(); Character && SelectionType != ESelectInfo::Direct)
	{
		VaesenCharacterSelect::Remember(VaesenCharacterSelect::LastCharacterKey, Character->Id);
	}
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandleChangeGuildClicked()
{
	if (!PickGuildDialog)
	{
		UE_LOG(LogVaesenApi, Warning, TEXT("%s has no PickGuildDialog."), *GetClass()->GetName());
		return;
	}

	PickGuildDialog->Open(Guilds, SelectedGuildId);
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandleGuildPicked(const FVaesenGuild& Guild)
{
	if (Guild.Id != SelectedGuildId)
	{
		SelectGuild(Guild, FString());
	}
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandleCreateGuildClicked()
{
	if (!CreateGuildDialog)
	{
		UE_LOG(LogVaesenApi, Warning, TEXT("%s has no CreateGuildDialog."), *GetClass()->GetName());
		return;
	}

	CreateGuildDialog->Open();
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandleGuildCreated(const FVaesenGuild& Guild)
{
	SetStatus(FText::Format(LOCTEXT("GuildCreated", "{0} is founded. You are its guild master."), FText::FromString(Guild.Name)));
	LoadGuilds(Guild.Id);
}

void UVaesenCharacterSelectWidget::HandleCreateCharacterClicked()
{
	if (!CreateCharacterDialog)
	{
		UE_LOG(LogVaesenApi, Warning, TEXT("%s has no CreateCharacterDialog."), *GetClass()->GetName());
		return;
	}

	if (!SelectedGuildId.IsEmpty() && bCharactersLoaded)
	{
		CreateCharacterDialog->Open(SelectedGuildId);
		UpdateButtons();
	}
}

void UVaesenCharacterSelectWidget::HandleCharacterCreated(const FVaesenCharacter& Character)
{
	SetStatus(FText::Format(LOCTEXT("CharacterCreated", "{0} has joined the guild."), FText::FromString(Character.Name)));
	VaesenCharacterSelect::Remember(VaesenCharacterSelect::LastCharacterKey, Character.Id);
	LoadCharacters(Character.Id);
}

void UVaesenCharacterSelectWidget::HandleDialogClosed()
{
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::HandlePlayClicked()
{
	const FVaesenCharacter* Character = GetSelectedCharacter();
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Character || !Api || IsLoading())
	{
		return;
	}

	VaesenCharacterSelect::Remember(VaesenCharacterSelect::LastCharacterKey, Character->Id);
	BeginLoading();
	SetStatus(FText::Format(LOCTEXT("Preparing", "Preparing {0}..."), FText::FromString(Character->Name)));

	const FString CharacterId = Character->Id;
	const FString CharacterName = Character->Name;
	Api->PlayCharacter(*Character, [WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), CharacterId, CharacterName](bool bSuccess, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (!bSuccess)
		{
			This->EndLoading();
			This->SetStatus(FText::FromString(Error));
			// Show who beat us to it.
			This->RefreshRoster();
			return;
		}

		// Stay "loading" (throbber up, buttons off) until we travel or fail.
		This->EnterWorld(CharacterId, CharacterName, 1);
	});
}

void UVaesenCharacterSelectWidget::EnterWorld(const FString& CharacterId, const FString& CharacterName, int32 Attempt)
{
	// A new game session takes a few seconds to start; give up after about a minute.
	constexpr int32 MaxAttempts = 30;

	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api)
	{
		EndLoading();
		return;
	}

	Api->EnterWorld(CharacterId, [WeakThis = TWeakObjectPtr<UVaesenCharacterSelectWidget>(this), CharacterId, CharacterName, Attempt](
		bool bSuccess, const FVaesenEnterWorldResponse& Response, const FString& Error)
	{
		UVaesenCharacterSelectWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (!bSuccess)
		{
			This->EndLoading();
			This->SetStatus(FText::FromString(Error));
			This->RefreshRoster();
			return;
		}

		const FText WorldName = FText::FromString(Response.DisplayName.IsEmpty() ? Response.WorldId : Response.DisplayName);
		if (!Response.IsReady())
		{
			if (Attempt >= MaxAttempts)
			{
				This->EndLoading();
				This->SetStatus(FText::Format(LOCTEXT("WorldNotStarting", "{0} didn't open in time. Try again in a moment."), WorldName));
				return;
			}

			This->SetStatus(FText::Format(LOCTEXT("OpeningWorld", "Opening {0}..."), WorldName));
			FTimerDelegate Retry = FTimerDelegate::CreateWeakLambda(This, [This, CharacterId, CharacterName, Attempt]()
			{
				This->EnterWorld(CharacterId, CharacterName, Attempt + 1);
			});
			This->GetWorld()->GetTimerManager().SetTimer(This->EnterRetryTimer, Retry, FMath::Max(1, Response.RetryAfterSeconds), false);
			return;
		}

		This->SetStatus(FText::Format(LOCTEXT("Entering", "Entering {0} as {1}..."), WorldName, FText::FromString(CharacterName)));
		UE_LOG(LogVaesenApi, Log, TEXT("Entering %s (%s) at %s"), *Response.WorldId, *Response.Map, *Response.ConnectUrl);

		// The server knows which character joined from the CharacterId option; the map is preloaded meanwhile.
		const TSoftObjectPtr<UWorld> Map(FSoftObjectPath(Response.Map + TEXT(".") + FPackageName::GetShortName(Response.Map)));
		This->GetGameInstance()->GetSubsystem<UVaesenLoadingScreenSubsystem>()->TravelToServer(
			Response.ConnectUrl, Map, FString::Printf(TEXT("&CharacterId=%s"), *CharacterId));
	});
}

const FVaesenGuild* UVaesenCharacterSelectWidget::GetSelectedGuild() const
{
	return Guilds.FindByPredicate([this](const FVaesenGuild& Guild) { return Guild.Id == SelectedGuildId; });
}

const FVaesenRosterEntry* UVaesenCharacterSelectWidget::GetSelectedEntry() const
{
	const int32 Index = PlayerComboBox ? PlayerComboBox->GetSelectedIndex() : 0;
	return bCharactersLoaded && Roster.IsValidIndex(Index) ? &Roster[Index] : nullptr;
}

const FVaesenCharacter* UVaesenCharacterSelectWidget::GetSelectedCharacter() const
{
	const FVaesenRosterEntry* Entry = GetSelectedEntry();
	return Entry ? &Entry->Character : nullptr;
}

bool UVaesenCharacterSelectWidget::IsDialogOpen() const
{
	return VaesenCharacterSelect::IsShown(PickGuildDialog)
		|| VaesenCharacterSelect::IsShown(CreateGuildDialog)
		|| VaesenCharacterSelect::IsShown(CreateCharacterDialog);
}

void UVaesenCharacterSelectWidget::UpdateButtons()
{
	const bool bIdle = !IsLoading() && !IsDialogOpen();
	VaesenCharacterSelect::SetEnabled(ChangeGuildButton, bIdle && bGuildsLoaded && !Guilds.IsEmpty());
	VaesenCharacterSelect::SetEnabled(CreateGuildButton, bIdle && bGuildsLoaded);
	VaesenCharacterSelect::SetEnabled(PlayerComboBox, bIdle && bCharactersLoaded);
	const FVaesenRosterEntry* Selected = GetSelectedEntry();
	VaesenCharacterSelect::SetEnabled(PlayButton, bIdle && Selected && CanPlay(*Selected));
	VaesenCharacterSelect::SetEnabled(CreateCharacterButton, bIdle && GetSelectedGuild() != nullptr && bCharactersLoaded);
}

void UVaesenCharacterSelectWidget::BeginLoading()
{
	++PendingLoads;
	if (LoadingThrobber)
	{
		LoadingThrobber->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::EndLoading()
{
	PendingLoads = FMath::Max(0, PendingLoads - 1);
	if (LoadingThrobber && !IsLoading())
	{
		LoadingThrobber->SetVisibility(ESlateVisibility::Collapsed);
	}
	UpdateButtons();
}

void UVaesenCharacterSelectWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
