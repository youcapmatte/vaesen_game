#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenLoginWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;

/**
 * Login screen (development login): choose a guild, then one of its players, and log in without a password.
 * On success the Character Select map opens.
 *
 * Widget Blueprint (e.g. WBP_Login, parent class VaesenLoginWidget) must contain, by these exact names:
 *   GuildComboBox   (Combo Box (String))
 *   PlayerComboBox  (Combo Box (String))
 *   LoginButton     (Button)
 *   StatusText      (Text) - optional
 * Remembers the last guild/player picked on this machine.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenLoginWidget : public UUserWidget
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
	TObjectPtr<UButton> LoginButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleGuildSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandlePlayerSelected(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	void HandleLoginClicked();

	void LoadGuilds();
	void PopulatePlayers(const FString& PlayerIdToSelect);
	const FVaesenDevGuild* GetSelectedGuild() const;
	const FVaesenDevPlayer* GetSelectedPlayer() const;
	void UpdateLoginButton();
	void SetStatus(const FText& Message);

	TArray<FVaesenDevGuild> Guilds;
	bool bLoggingIn = false;
};
