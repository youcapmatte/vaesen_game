#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "Types/SlateEnums.h"
#include "VaesenCreateGuildWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaesenGuildCreatedSignature, const FVaesenGuild& /*Guild*/);

/**
 * Modal dialog on the character select screen: create a guild. The signed-in account becomes its first member
 * and guild master. The name must be unique (the API says so if it's taken).
 *
 * Widget Blueprint (e.g. WBP_CreateGuild, parent class VaesenCreateGuildWidget), controls by these exact names:
 *   GuildNameTextBox  (Text Box) - Enter creates
 *   CreateButton      (Button)
 *   CancelButton      (Button)
 *   StatusText        (Text) - validation and server errors
 * Place it inside WBP_CharacterSelect named "CreateGuildDialog"; it starts hidden and hides itself when done.
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built; make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenCreateGuildWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the dialog, empty. */
	void Open();

	void Close();

	/** Broadcast after the guild was created (the dialog has already closed). */
	FVaesenGuildCreatedSignature OnGuildCreated;

	FSimpleMulticastDelegate OnCancelled;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> GuildNameTextBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreateButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleNameChanged(const FText& Text);

	UFUNCTION()
	void HandleNameCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	FString GetGuildName() const;
	void UpdateForm();
	void SetStatus(const FText& Message);

	bool bBusy = false;
	/** Incremented on Open/Close so an answer to an abandoned attempt is ignored. */
	int32 Attempt = 0;
};
