#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/VaesenApiTypes.h"
#include "VaesenPickGuildWidget.generated.h"

class UButton;
class UComboBoxString;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaesenGuildPickedSignature, const FVaesenGuild& /*Guild*/);

/**
 * Modal dialog on the character select screen: choose which of your guilds to play in ("Change Guild").
 *
 * Widget Blueprint (e.g. WBP_PickGuild, parent class VaesenPickGuildWidget), controls by these exact names:
 *   GuildComboBox  (Combo Box (String))
 *   SelectButton   (Button)
 *   CancelButton   (Button)
 *   StatusText     (Text)
 * Place it inside WBP_CharacterSelect named "PickGuildDialog"; it starts hidden and hides itself when done.
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built; make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenPickGuildWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows the dialog with these guilds, the current one preselected. */
	void Open(const TArray<FVaesenGuild>& InGuilds, const FString& CurrentGuildId);

	void Close();

	/** Broadcast when the player picks a guild (the dialog has already closed). */
	FVaesenGuildPickedSignature OnGuildPicked;

	FSimpleMulticastDelegate OnCancelled;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UComboBoxString> GuildComboBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleSelectClicked();

	UFUNCTION()
	void HandleCancelClicked();

	TArray<FVaesenGuild> Guilds;
};
