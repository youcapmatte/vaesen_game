#include "UI/Login/VaesenLoginWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Online/VaesenApiSubsystem.h"
#include "Sound/SoundBase.h"
#include "UI/Login/VaesenLoginProgressWidget.h"

namespace VaesenLogin
{
	/** Kept in the per-user GameUserSettings.ini so restarts start with the username filled in. Never the password. */
	const TCHAR* ConfigSection = TEXT("Vaesen.Login");
	const TCHAR* LastUsernameKey = TEXT("LastUsername");
}

void UVaesenLoginWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Normally caught by the Widget Blueprint compiler; guards against a stale or broken Blueprint.
	if (!HasRequiredWidgets())
	{
		UE_LOG(LogVaesenApi, Error, TEXT("%s is missing UsernameTextBox, PasswordTextBox, LoginButton or LoginProgress."), *GetClass()->GetName());
		return;
	}

	LoginButton->OnClicked.AddDynamic(this, &UVaesenLoginWidget::HandleLoginClicked);
	PasswordTextBox->OnTextCommitted.AddDynamic(this, &UVaesenLoginWidget::HandlePasswordCommitted);
	LoginProgress->OnCancel.AddUObject(this, &UVaesenLoginWidget::HandleLoginCancelled);
}

void UVaesenLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!HasRequiredWidgets())
	{
		return;
	}

	FString LastUsername;
	GConfig->GetString(VaesenLogin::ConfigSection, VaesenLogin::LastUsernameKey, LastUsername, GGameUserSettingsIni);
	UsernameTextBox->SetText(FText::FromString(LastUsername));
	PasswordTextBox->SetText(FText::GetEmpty());
	LoginProgress->SetVisibility(ESlateVisibility::Collapsed);
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

bool UVaesenLoginWidget::HasRequiredWidgets() const
{
	return UsernameTextBox && PasswordTextBox && LoginButton && LoginProgress;
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
	if (!Api || bLoggingIn || !HasRequiredWidgets())
	{
		return;
	}

	if (USoundBase* ClickSound = UVaesenSettings::Get()->LoginButtonSound.LoadSynchronous())
	{
		// Persist across the level change, or opening Character Select would cut the sound off.
		UGameplayStatics::SpawnSound2D(this, ClickSound, 1.f, 1.f, 0.f, nullptr, /*bPersistAcrossLevelTransition*/ true);
	}

	const FString Username = UsernameTextBox->GetText().ToString().TrimStartAndEnd();
	const FString Password = PasswordTextBox->GetText().ToString();

	bLoggingIn = true;
	SetFormEnabled(false);
	SetStatus(FText::GetEmpty());
	LoginProgress->SetMessage(FText::GetEmpty());
	LoginProgress->SetVisibility(ESlateVisibility::Visible);

	const TWeakObjectPtr<UVaesenLoginWidget> WeakThis(this);
	Api->Login(Username, Password,
		[WeakThis](const FText& Message)
		{
			if (UVaesenLoginWidget* This = WeakThis.Get())
			{
				This->LoginProgress->SetMessage(Message);
			}
		},
		[WeakThis, Username](bool bSuccess, const FString& Error)
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

			// Keep the box up with the error; Cancel returns to the form.
			This->LoginProgress->SetMessage(FText::FromString(Error));
		});
}

void UVaesenLoginWidget::HandleLoginCancelled()
{
	if (UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>())
	{
		Api->CancelLogin();
	}

	bLoggingIn = false;
	LoginProgress->SetVisibility(ESlateVisibility::Collapsed);
	SetFormEnabled(true);
	PasswordTextBox->SetKeyboardFocus();
}

void UVaesenLoginWidget::SetFormEnabled(bool bEnabled)
{
	UsernameTextBox->SetIsEnabled(bEnabled);
	PasswordTextBox->SetIsEnabled(bEnabled);
	LoginButton->SetIsEnabled(bEnabled);
}

void UVaesenLoginWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

