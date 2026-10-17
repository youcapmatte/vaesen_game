#include "UI/Login/VaesenLoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/VaesenApiSubsystem.h"

#define LOCTEXT_NAMESPACE "VaesenLogin"

namespace VaesenLogin
{
	/** Kept in the per-user GameUserSettings.ini so restarts start with the username filled in. Never the password. */
	const TCHAR* ConfigSection = TEXT("Vaesen.Login");
	const TCHAR* LastUsernameKey = TEXT("LastUsername");
}

void UVaesenLoginWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	LoginButton->OnClicked.AddDynamic(this, &UVaesenLoginWidget::HandleLoginClicked);
	PasswordTextBox->OnTextCommitted.AddDynamic(this, &UVaesenLoginWidget::HandlePasswordCommitted);
}

void UVaesenLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	FString LastUsername;
	GConfig->GetString(VaesenLogin::ConfigSection, VaesenLogin::LastUsernameKey, LastUsername, GGameUserSettingsIni);
	UsernameTextBox->SetText(FText::FromString(LastUsername));
	PasswordTextBox->SetText(FText::GetEmpty());
	SetStatus(FText::GetEmpty());

	// Start typing where it's needed.
	if (LastUsername.IsEmpty())
	{
		UsernameTextBox->SetKeyboardFocus();
	}
	else
	{
		PasswordTextBox->SetKeyboardFocus();
	}
}

void UVaesenLoginWidget::HandlePasswordCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleLoginClicked();
	}
}

void UVaesenLoginWidget::HandleLoginClicked()
{
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api || bLoggingIn)
	{
		return;
	}

	const FString Username = UsernameTextBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordTextBox->GetText().ToString();

	bLoggingIn = true;
	LoginButton->SetIsEnabled(false);
	SetStatus(LOCTEXT("LoggingIn", "Logging in..."));

	Api->Login(Username, Password, [WeakThis = TWeakObjectPtr<UVaesenLoginWidget>(this), Username](bool bSuccess, const FString& Error)
	{
		UVaesenLoginWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (bSuccess)
		{
			GConfig->SetString(VaesenLogin::ConfigSection, VaesenLogin::LastUsernameKey, *Username, GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
			UGameplayStatics::OpenLevelBySoftObjectPtr(This, UVaesenSettings::Get()->CharacterSelectMap);
			return;
		}

		This->bLoggingIn = false;
		This->LoginButton->SetIsEnabled(true);
		This->SetStatus(FText::FromString(Error));
	});
}

void UVaesenLoginWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
