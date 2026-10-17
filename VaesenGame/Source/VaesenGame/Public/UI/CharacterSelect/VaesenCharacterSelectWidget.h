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

/**
 * Character select screen: choose one of your guilds, then one of its characters, and press Play.
 * "Create Character" opens the create-character dialog once a guild is selected and its characters are loaded.
 * Guilds and characters come from the API (the signed-in account's guilds).
 *
 * Widget Blueprint (e.g. WBP_CharacterSelect, parent class VaesenCharacterSelectWidget) must contain, by these exact names:
 *   GuildComboBox           (Combo Box (String))
 *   PlayerComboBox          (Combo Box (String)) - the guild's characters
 *   PlayButton              (Button)
 *   CreateCharacterButton   (Button)
 *   CreateCharacterDialog   (WBP_CreateCharacter, a VaesenCreateCharacterWidget) - hidden until opened
 *   StatusText              (Text) - optional
 * Remembers the last guild/character picked on this machine.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> GuildComboBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> PlayerComboBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CreateCharacterButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVaesenCreateCharacterWidget> CreateCharacterDialog;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleCharacterSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleCreateCharacterClicked();

	void HandleCharacterCreated(const FVaesenCharacter& Character);
	void HandleCreateCancelled();

	bool HasRequiredWidgets() const;
	void LoadGuilds();
	void LoadCharacters(const FString& CharacterIdToSelect);
	const FVaesenGuild* GetSelectedGuild() const;
	const FVaesenCharacter* GetSelectedCharacter() const;
	void RememberSelection() const;
	void UpdateButtons();
	void SetStatus(const FText& Message);

	TArray<FVaesenGuild> Guilds;
	TArray<FVaesenCharacter> Characters;

	/** True once the selected guild's characters have loaded. */
	bool bCharactersLoaded = false;
	bool bBusy = false;
	/** Incremented per character load, so a slow answer for a previously selected guild is ignored. */
	int32 CharacterLoad = 0;
};
