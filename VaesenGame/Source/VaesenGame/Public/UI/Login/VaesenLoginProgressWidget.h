#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VaesenLoginProgressWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Modal box shown over the login screen while logging in: one status line ("Authenticating...") that each new
 * message replaces, and a Cancel button. Errors are shown in the same line.
 *
 * Widget Blueprint (e.g. WBP_LoginProgress, parent class VaesenLoginProgressWidget) must contain, by these exact names:
 *   MessageText   (Text)
 *   CancelButton  (Button)
 * Place it inside WBP_Login named "LoginProgress"; it starts hidden.
 */
UCLASS(Abstract)
class VAESENGAME_API UVaesenLoginProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMessage(const FText& Message);

	/** Broadcast when the player presses Cancel. */
	FSimpleMulticastDelegate OnCancel;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

private:
	UFUNCTION()
	void HandleCancelClicked();
};
