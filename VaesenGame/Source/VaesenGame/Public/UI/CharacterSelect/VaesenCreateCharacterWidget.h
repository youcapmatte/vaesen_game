#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenCreateCharacterWidget.generated.h"

class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaesenCharacterCreatedSignature, const FVaesenCharacter& /*Character*/);

/**
 * Modal dialog on the character select screen: create a character in the selected guild.
 * First and last name are required; the character name ("{first} {last}") is shown as you type and must be unique.
 * The class comes from the API's list. Randomize picks a first and a last name independently from UVaesenSettings.
 *
 * Widget Blueprint (e.g. WBP_CreateCharacter, parent class VaesenCreateCharacterWidget) must contain, by these exact names:
 *   FirstNameTextBox    (Text Box)
 *   LastNameTextBox     (Text Box)
 *   CharacterNameText   (Text)              - shows "{first} {last}"
 *   ClassComboBox       (Combo Box (String))
 *   RandomizeButton     (Button)
 *   CreateButton        (Button)
 *   CancelButton        (Button)
 *   StatusText          (Text) - optional, shows validation and server errors
 * Place it inside WBP_CharacterSelect named "CreateCharacterDialog"; it starts hidden and hides itself when done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenCreateCharacterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the dialog, empty, for creating a character in the guild InGuildId. */
	void Open(const FString& InGuildId);

	/** Hides the dialog. */
	void Close();

	/** Broadcast after the character was created (the dialog has already closed). */
	FVaesenCharacterCreatedSignature OnCharacterCreated;

	/** Broadcast when the player cancels. */
	FSimpleMulticastDelegate OnCancelled;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> FirstNameTextBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> LastNameTextBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CharacterNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UComboBoxString> ClassComboBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RandomizeButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CreateButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleNameChanged(const FText& Text);

	UFUNCTION()
	void HandleRandomizeClicked();

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	bool HasRequiredWidgets() const;
	void LoadClasses();
	FString GetFirstName() const;
	FString GetLastName() const;
	FString GetCharacterName() const;
	void UpdateForm();
	void SetBusy(bool bInBusy);
	void SetStatus(const FText& Message);

	FString GuildId;
	TArray<FString> Classes;
	bool bBusy = false;
	/** Incremented on Open/Close so answers to an abandoned attempt are ignored. */
	int32 Attempt = 0;
};
