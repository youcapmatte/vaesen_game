#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenCharacterSelectWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;

/**
 * Character select screen (development data): choose a guild, then one of its characters, and press Play.
 *
 * Widget Blueprint (e.g. WBP_CharacterSelect, parent class VaesenCharacterSelectWidget) must contain, by these exact names:
 *   GuildComboBox   (Combo Box (String))
 *   PlayerComboBox  (Combo Box (String))
 *   PlayButton      (Button)
 *   StatusText      (Text) - optional
 * Remembers the last guild/character picked on this machine; -DevPlayer=<id> preselects that character.
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

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePlayerSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePlayClicked();

	bool HasRequiredWidgets() const;
	void LoadGuilds();
	void PopulatePlayers(const FString& PlayerIdToSelect);
	const FVaesenDevGuild* GetSelectedGuild() const;
	const FVaesenDevPlayer* GetSelectedPlayer() const;
	void UpdatePlayButton();
	void SetStatus(const FText& Message);

	TArray<FVaesenDevGuild> Guilds;
	bool bBusy = false;
};
