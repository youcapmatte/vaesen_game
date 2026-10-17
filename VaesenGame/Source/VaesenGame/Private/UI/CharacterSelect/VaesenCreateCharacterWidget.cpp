#include "UI/CharacterSelect/VaesenCreateCharacterWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSettings.h"
#include "Online/VaesenApiSubsystem.h"

#define LOCTEXT_NAMESPACE "VaesenCreateCharacter"

void UVaesenCreateCharacterWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Normally caught by the Widget Blueprint compiler; guards against a stale or broken Blueprint.
	if (!HasRequiredWidgets())
	{
		UE_LOG(LogVaesenApi, Warning, TEXT("%s needs FirstNameTextBox, LastNameTextBox and CreateButton to create characters."), *GetClass()->GetName());
	}

	if (FirstNameTextBox)
	{
		FirstNameTextBox->OnTextChanged.AddDynamic(this, &UVaesenCreateCharacterWidget::HandleNameChanged);
	}
	if (LastNameTextBox)
	{
		LastNameTextBox->OnTextChanged.AddDynamic(this, &UVaesenCreateCharacterWidget::HandleNameChanged);
	}
	if (RandomizeButton)
	{
		RandomizeButton->OnClicked.AddDynamic(this, &UVaesenCreateCharacterWidget::HandleRandomizeClicked);
	}
	if (CreateButton)
	{
		CreateButton->OnClicked.AddDynamic(this, &UVaesenCreateCharacterWidget::HandleCreateClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVaesenCreateCharacterWidget::HandleCancelClicked);
	}
}

bool UVaesenCreateCharacterWidget::HasRequiredWidgets() const
{
	return FirstNameTextBox && LastNameTextBox && CreateButton;
}

void UVaesenCreateCharacterWidget::Open(const FString& InGuildId)
{
	if (!HasRequiredWidgets())
	{
		return;
	}

	++Attempt;
	GuildId = InGuildId;
	bBusy = false;
	FirstNameTextBox->SetText(FText::GetEmpty());
	LastNameTextBox->SetText(FText::GetEmpty());
	SetStatus(FText::GetEmpty());
	SetVisibility(ESlateVisibility::Visible);
	UpdateForm();
	FirstNameTextBox->SetKeyboardFocus();

	if (Classes.IsEmpty())
	{
		LoadClasses();
	}
}

void UVaesenCreateCharacterWidget::Close()
{
	++Attempt;
	SetVisibility(ESlateVisibility::Collapsed);
}

void UVaesenCreateCharacterWidget::LoadClasses()
{
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api)
	{
		return;
	}

	Api->GetCharacterClasses([WeakThis = TWeakObjectPtr<UVaesenCreateCharacterWidget>(this)](bool bSuccess, const TArray<FString>& LoadedClasses, const FString& Error)
	{
		UVaesenCreateCharacterWidget* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		if (!bSuccess)
		{
			This->SetStatus(FText::FromString(Error));
			return;
		}

		This->Classes = LoadedClasses;
		if (This->ClassComboBox)
		{
			This->ClassComboBox->ClearOptions();
			for (const FString& Class : This->Classes)
			{
				This->ClassComboBox->AddOption(Class);
			}
			if (!This->Classes.IsEmpty())
			{
				This->ClassComboBox->SetSelectedIndex(0);
			}
		}
		This->UpdateForm();
	});
}

FString UVaesenCreateCharacterWidget::GetFirstName() const
{
	return FirstNameTextBox ? FirstNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
}

FString UVaesenCreateCharacterWidget::GetLastName() const
{
	return LastNameTextBox ? LastNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
}

FString UVaesenCreateCharacterWidget::GetCharacterName() const
{
	return FString::Printf(TEXT("%s %s"), *GetFirstName(), *GetLastName()).TrimStartAndEnd();
}

void UVaesenCreateCharacterWidget::HandleNameChanged(const FText& Text)
{
	SetStatus(FText::GetEmpty());
	UpdateForm();
}

void UVaesenCreateCharacterWidget::UpdateForm()
{
	if (CharacterNameText)
	{
		CharacterNameText->SetText(FText::FromString(GetCharacterName()));
	}

	// Class is only required when there's a class picker (it's optional in the API).
	const bool bClassChosen = !ClassComboBox || !ClassComboBox->GetSelectedOption().IsEmpty();
	const bool bValid = !GetFirstName().IsEmpty() && !GetLastName().IsEmpty() && bClassChosen;
	for (UWidget* Widget : TArray<UWidget*>{ RandomizeButton, FirstNameTextBox, LastNameTextBox, ClassComboBox })
	{
		if (Widget)
		{
			Widget->SetIsEnabled(!bBusy);
		}
	}
	if (CreateButton)
	{
		CreateButton->SetIsEnabled(!bBusy && bValid);
	}
}

void UVaesenCreateCharacterWidget::HandleRandomizeClicked()
{
	const UVaesenSettings* Settings = UVaesenSettings::Get();
	if (FirstNameTextBox && !Settings->RandomFirstNames.IsEmpty())
	{
		FirstNameTextBox->SetText(FText::FromString(Settings->RandomFirstNames[FMath::RandRange(0, Settings->RandomFirstNames.Num() - 1)]));
	}
	if (LastNameTextBox && !Settings->RandomLastNames.IsEmpty())
	{
		LastNameTextBox->SetText(FText::FromString(Settings->RandomLastNames[FMath::RandRange(0, Settings->RandomLastNames.Num() - 1)]));
	}
	SetStatus(FText::GetEmpty());
	UpdateForm();
}

void UVaesenCreateCharacterWidget::HandleCreateClicked()
{
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	if (!Api || bBusy)
	{
		return;
	}

	const FString FirstName = GetFirstName();
	const FString LastName = GetLastName();
	if (FirstName.IsEmpty() || LastName.IsEmpty())
	{
		SetStatus(LOCTEXT("NamesRequired", "Enter both a first and a last name."));
		return;
	}

	const FString CharacterName = GetCharacterName();
	const FString Class = ClassComboBox ? ClassComboBox->GetSelectedOption() : FString();
	const int32 ThisAttempt = Attempt;
	SetBusy(true);
	SetStatus(LOCTEXT("Checking", "Checking the name..."));

	// Check the name first for a clear message; the API still refuses a duplicate if someone takes it in between.
	Api->IsCharacterNameTaken(CharacterName,
		[WeakThis = TWeakObjectPtr<UVaesenCreateCharacterWidget>(this), Api, ThisAttempt, CharacterName, FirstName, LastName, Class](bool bSuccess, bool bTaken, const FString& Error)
		{
			UVaesenCreateCharacterWidget* This = WeakThis.Get();
			if (!This || This->Attempt != ThisAttempt)
			{
				return;
			}

			if (!bSuccess || bTaken)
			{
				This->SetBusy(false);
				This->SetStatus(bTaken
					? FText::Format(LOCTEXT("NameTaken", "A character named {0} already exists."), FText::FromString(CharacterName))
					: FText::FromString(Error));
				return;
			}

			This->SetStatus(LOCTEXT("Creating", "Creating the character..."));
			Api->CreateCharacter(This->GuildId, FirstName, LastName, Class,
				[WeakThis, ThisAttempt](bool bCreated, const FVaesenCharacter& Character, const FString& CreateError)
				{
					UVaesenCreateCharacterWidget* Dialog = WeakThis.Get();
					if (!Dialog || Dialog->Attempt != ThisAttempt)
					{
						return;
					}

					Dialog->SetBusy(false);
					if (!bCreated)
					{
						Dialog->SetStatus(FText::FromString(CreateError));
						return;
					}

					Dialog->Close();
					Dialog->OnCharacterCreated.Broadcast(Character);
				});
		});
}

void UVaesenCreateCharacterWidget::HandleCancelClicked()
{
	Close();
	OnCancelled.Broadcast();
}

void UVaesenCreateCharacterWidget::SetBusy(bool bInBusy)
{
	bBusy = bInBusy;
	UpdateForm();
}

void UVaesenCreateCharacterWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
