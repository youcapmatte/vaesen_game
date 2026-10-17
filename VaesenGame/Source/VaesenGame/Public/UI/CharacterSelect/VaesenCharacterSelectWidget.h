#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenCharacterSelectWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;
class UVaesenCreateCharacterWidget;
class UVaesenCreateGuildWidget;
class UVaesenPickGuildWidget;
class UWidget;

/**
 * Character select screen. Opens on the guild you used last (or your first guild), lists its characters and plays
 * the chosen one. Change Guild opens the pick-guild dialog, Create Guild the create-guild dialog (you become its
 * guild master), Create Character the create-character dialog (once the guild's characters are loaded).
 * Guilds and characters come from the API (the signed-in account's guilds).
 *
 * Widget Blueprint (e.g. WBP_CharacterSelect, parent class VaesenCharacterSelectWidget), controls by these exact names:
 *   GuildNameText           (Text)                 - the selected guild's name
 *   ChangeGuildButton       (Button)               - opens PickGuildDialog
 *   CreateGuildButton       (Button)               - opens CreateGuildDialog
 *   PlayerComboBox          (Combo Box (String))   - the guild's characters
 *   PlayButton              (Button)
 *   CreateCharacterButton   (Button)               - opens CreateCharacterDialog
 *   PickGuildDialog         (WBP_PickGuild, a VaesenPickGuildWidget)                 - hidden until opened
 *   CreateGuildDialog       (WBP_CreateGuild, a VaesenCreateGuildWidget)             - hidden until opened
 *   CreateCharacterDialog   (WBP_CreateCharacter, a VaesenCreateCharacterWidget)     - hidden until opened
 *   StatusText              (Text)
 * Remembers the last guild and character picked on this machine.
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
	const FVaesenGuild* GetSelectedGuild() const;
	const FVaesenCharacter* GetSelectedCharacter() const;
	bool IsDialogOpen() const;
	void UpdateButtons();
	void SetStatus(const FText& Message);

	TArray<FVaesenGuild> Guilds;
	FString SelectedGuildId;
	TArray<FVaesenCharacter> Characters;

	/** True once the selected guild's characters have loaded. */
	bool bCharactersLoaded = false;
	bool bGuildsLoaded = false;
	bool bBusy = false;
	/** Incremented per character load, so a slow answer for a previously selected guild is ignored. */
	int32 CharacterLoad = 0;
};
