#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "VaesenLoginWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVaesenLoginProgressWidget;

/**
 * Login screen: username/email and password. Pressing Login shows the progress box with the login steps;
 * on success the Character Select map opens, Cancel returns to the form.
 * The login itself is fake for now (see UVaesenApiSubsystem::Login).
 *
 * Widget Blueprint (e.g. WBP_Login, parent class VaesenLoginWidget) controls by these exact names:
 *   UsernameTextBox  (Text Box)
 *   PasswordTextBox  (Text Box, "Is Password" ticked)
 *   LoginButton      (Button)
 *   LoginProgress    (WBP_LoginProgress, a VaesenLoginProgressWidget) - hidden until Login is pressed
 *   StatusText       (Text) - optional
 * Pressing Enter in the password box logs in. The last username is remembered on this machine.
 *
 * TEMPORARY: all bindings are BindWidgetOptional while the UI is being built; make them BindWidget when it's done.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenLoginWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> UsernameTextBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> PasswordTextBox;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoginButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UVaesenLoginProgressWidget> LoginProgress;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UFUNCTION()
	void HandleLoginClicked();

	UFUNCTION()
	void HandlePasswordCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void HandleLoginCancelled();
	bool HasRequiredWidgets() const;
	void SetFormEnabled(bool bEnabled);
	void SetStatus(const FText& Message);

	bool bLoggingIn = false;
};
