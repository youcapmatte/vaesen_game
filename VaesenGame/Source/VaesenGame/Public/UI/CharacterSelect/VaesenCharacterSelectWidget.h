#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenCharacterSelectWidget.generated.h"

class UButton;
class UCircularThrobber;
class UComboBoxString;
class UTextBlock;
class UVaesenCreateCharacterWidget;
class UVaesenCreateGuildWidget;
class UVaesenPickGuildWidget;
class UWidget;
class AVaesenCharacterCreatorScene;
class UVaesenCharacterCreatorWidget;

/**
 * Character select screen. Opens on the guild you used last (or your first guild), lists its characters and plays
 * the chosen one. Change Guild opens the pick-guild dialog, Create Guild the create-guild dialog (you become its
 * guild master), Create Character the character creator (once the guild's characters are loaded).
 *
 * With an AVaesenCharacterCreatorScene in the level, the scene is the view: it shows the selected character's look, and
 * Create Character opens the creator there (this widget hides until the creator closes; a new character is selected).
 * Without one, Create Character opens CreateCharacterDialog (a name and a class, no look).
 * Guilds and characters come from the API (the signed-in account's guilds).
 *
 * Widget Blueprint (e.g. WBP_CharacterSelect, parent class VaesenCharacterSelectWidget), controls by these exact names:
 *   GuildNameText           (Text)                 - the selected guild's name
 *   ChangeGuildButton       (Button)               - opens PickGuildDialog
 *   CreateGuildButton       (Button)               - opens CreateGuildDialog
 *   PlayerComboBox          (Combo Box (String))   - the guild's characters, with who is playing each one
 *   PlayButton              (Button)
 *   CreateCharacterButton   (Button)               - opens CreateCharacterDialog
 *   PickGuildDialog         (WBP_PickGuild, a VaesenPickGuildWidget)                 - hidden until opened
 *   CreateGuildDialog       (WBP_CreateGuild, a VaesenCreateGuildWidget)             - hidden until opened
 *   CreateCharacterDialog   (WBP_CreateCharacter, a VaesenCreateCharacterWidget)     - hidden until opened
 *   StatusText              (Text)
 *   LoadingThrobber         (Circular Throbber)   - shown while the screen loads something; buttons are disabled meanwhile
 * Remembers the last guild and character picked on this machine. A character another member is playing can't be
 * played; the list refreshes itself every RosterRefreshSeconds (until the API can push changes).
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built, so each feature can be tried on its
 * own; whatever is missing is skipped. Make them BindWidget when the UI is done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** How often the character list refreshes quietly, to show who started or stopped playing. 0 = never. */
	UPROPERTY(EditAnywhere, Category = "Vaesen", meta = (ClampMin = "0", Units = "s"))
	float RosterRefreshSeconds = 15.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GuildNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ChangeGuildButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateGuildButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> PlayerComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateCharacterButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenPickGuildWidget> PickGuildDialog;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenCreateGuildWidget> CreateGuildDialog;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenCreateCharacterWidget> CreateCharacterDialog;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCircularThrobber> LoadingThrobber;

private:
	UFUNCTION()
	void HandleChangeGuildClicked();

	UFUNCTION()
	void HandleCreateGuildClicked();

	UFUNCTION()
	void HandleCharacterSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleCreateCharacterClicked();

	void HandleGuildPicked(const FVaesenGuild& Guild);
	void HandleGuildCreated(const FVaesenGuild& Guild);
	void HandleCharacterCreated(const FVaesenCharacter& Character);
	void HandleDialogClosed();

	/** Loads the account's guilds and selects GuildIdToSelect, else the remembered guild, else the first. */
	void LoadGuilds(const FString& GuildIdToSelect);
	void SelectGuild(const FVaesenGuild& Guild, const FString& CharacterIdToSelect);
	void LoadCharacters(const FString& CharacterIdToSelect);
	/** After Play: asks the API where to go, waits while the world starts, then travels there. */
	void EnterWorld(const FString& CharacterId, const FString& CharacterName, int32 Attempt);

	/** Reloads the roster in the background (no throbber), keeping the selection. Skipped while busy. */
	void RefreshRoster();
	void ShowRoster(const TArray<FVaesenRosterEntry>& LoadedRoster, const FString& CharacterIdToSelect);
	FString GetRosterLabel(const FVaesenRosterEntry& Entry) const;
	bool CanPlay(const FVaesenRosterEntry& Entry) const;
	const FVaesenGuild* GetSelectedGuild() const;
	const FVaesenRosterEntry* GetSelectedEntry() const;
	const FVaesenCharacter* GetSelectedCharacter() const;

	/** Shows the selected character's look in the scene (nobody if it has none). */
	void PreviewSelected();
	void OpenCreator();
	void HandleCreatorFinished(bool bCreated, const FVaesenCharacter& Character);

	UPROPERTY(Transient)
	TObjectPtr<AVaesenCharacterCreatorScene> CreatorScene;

	UPROPERTY(Transient)
	TObjectPtr<UVaesenCharacterCreatorWidget> CreatorWidget;
	bool IsDialogOpen() const;
	/** Counts a request the screen waits for: shows the throbber and disables the buttons until EndLoading. */
	void BeginLoading();
	void EndLoading();
	bool IsLoading() const { return PendingLoads > 0; }

	void UpdateButtons();
	void SetStatus(const FText& Message);

	TArray<FVaesenGuild> Guilds;
	FString SelectedGuildId;
	TArray<FVaesenRosterEntry> Roster;
	FTimerHandle RosterRefreshTimer;
	FTimerHandle EnterRetryTimer;

	/** Shown once the characters have loaded, e.g. why entering the world failed. */
	FText PendingStatus;

	/** True once the selected guild's characters have loaded. */
	bool bCharactersLoaded = false;
	bool bGuildsLoaded = false;
	/** Requests in flight (guilds, characters, play). */
	int32 PendingLoads = 0;
	/** Incremented per character load, so a slow answer for a previously selected guild is ignored. */
	int32 CharacterLoad = 0;
};
