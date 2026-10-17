#include "UI/Login/VaesenLoginProgressWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UVaesenLoginProgressWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UVaesenLoginProgressWidget::HandleCancelClicked);
	}
}

void UVaesenLoginProgressWidget::SetMessage(const FText& Message)
{
	if (MessageText)
	{
		MessageText->SetText(Message);
	}
}

void UVaesenLoginProgressWidget::HandleCancelClicked()
{
	OnCancel.Broadcast();
}
