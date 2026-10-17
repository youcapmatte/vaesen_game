#include "UI/Guild/VaesenCreateGuildWidget.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Online/VaesenApiSubsystem.h"

#define LOCTEXT_NAMESPACE "VaesenCreateGuild"

void UVaesenCreateGuildWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (GuildNameTextBox)
	{
		GuildNameTextBox->OnTextChanged.AddDynamic(this, &UVaesenCreateGuildWidget::HandleNameChanged);
		GuildNameTextBox->OnTextCommitted.AddDynamic(this, &UVaesenCreateGuildWidget::HandleNameCommitted);
	}
	if (CreateButton)
	{
		CreateButton->OnClicked.AddDynamic(this, &UVaesenCreateGuildWidget::HandleCreateClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVaesenCreateGuildWidget::HandleCancelClicked);
	}
}

void UVaesenCreateGuildWidget::Open()
{
	++Attempt;
	bBusy = false;
	if (GuildNameTextBox)
	{
		GuildNameTextBox->SetText(FText::GetEmpty());
		GuildNameTextBox->SetKeyboardFocus();
	}
	SetStatus(FText::GetEmpty());
	UpdateForm();
	SetVisibility(ESlateVisibility::Visible);
}

void UVaesenCreateGuildWidget::Close()
{
	++Attempt;
	SetVisibility(ESlateVisibility::Collapsed);
}

FString UVaesenCreateGuildWidget::GetGuildName() const
{
	return GuildNameTextBox ? GuildNameTextBox->GetText().ToString().TrimStartAndEnd() : FString();
}

void UVaesenCreateGuildWidget::HandleNameChanged(const FText& Text)
{
	SetStatus(FText::GetEmpty());
	UpdateForm();
}

void UVaesenCreateGuildWidget::HandleNameCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		HandleCreateClicked();
	}
}

void UVaesenCreateGuildWidget::HandleCreateClicked()
{
	UVaesenApiSubsystem* Api = GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>();
	const FString Name = GetGuildName();
	if (!Api || bBusy)
	{
		return;
	}
	if (Name.IsEmpty())
	{
		SetStatus(LOCTEXT("NameRequired", "Enter a name for the guild."));
		return;
	}

	bBusy = true;
	UpdateForm();
	SetStatus(LOCTEXT("Creating", "Creating the guild..."));

	const int32 ThisAttempt = Attempt;
	Api->CreateGuild(Name, [WeakThis = TWeakObjectPtr<UVaesenCreateGuildWidget>(this), ThisAttempt](bool bSuccess, const FVaesenGuild& Guild, const FString& Error)
	{
		UVaesenCreateGuildWidget* This = WeakThis.Get();
		if (!This || This->Attempt != ThisAttempt)
		{
			return;
		}

		This->bBusy = false;
		This->UpdateForm();
		if (!bSuccess)
		{
			// E.g. "That guild name is taken." or the API's validation message.
			This->SetStatus(FText::FromString(Error));
			return;
		}

		This->Close();
		This->OnGuildCreated.Broadcast(Guild);
	});
}

void UVaesenCreateGuildWidget::HandleCancelClicked()
{
	Close();
	OnCancelled.Broadcast();
}

void UVaesenCreateGuildWidget::UpdateForm()
{
	if (CreateButton)
	{
		CreateButton->SetIsEnabled(!bBusy && !GetGuildName().IsEmpty());
	}
	if (GuildNameTextBox)
	{
		GuildNameTextBox->SetIsEnabled(!bBusy);
	}
}

void UVaesenCreateGuildWidget::SetStatus(const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
	}
}

#undef LOCTEXT_NAMESPACE
