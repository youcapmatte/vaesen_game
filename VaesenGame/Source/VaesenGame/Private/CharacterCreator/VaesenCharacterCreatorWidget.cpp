#include "CharacterCreator/VaesenCharacterCreatorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterCreator/VaesenCharacterCreator.h"
#include "CharacterCreator/VaesenCharacterCreatorScene.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Online/VaesenApiSubsystem.h"

namespace VaesenCreatorWidget
{
	const FLinearColor PanelColor(0.02f, 0.02f, 0.025f, 0.78f);
	const FLinearColor HeadingColor(0.85f, 0.72f, 0.45f);
	const FLinearColor MutedColor(0.7f, 0.7f, 0.7f);
	const FLinearColor ButtonColor(0.35f, 0.35f, 0.38f);
	const FLinearColor SelectedColor(0.85f, 0.62f, 0.2f);
	const TCHAR* StepNames[] = { TEXT("Origins"), TEXT("Body and skin"), TEXT("Face"), TEXT("Eyes and hair"), TEXT("Name") };
	constexpr int32 StepCount = UE_ARRAY_COUNT(StepNames);
}

void UVaesenCreatorControlHandler::Clicked()
{
	if (Owner.IsValid())
	{
		Owner->HandleControl(*this, 0.f);
	}
}

void UVaesenCreatorControlHandler::ValueChanged(float Value)
{
	if (Owner.IsValid())
	{
		Owner->HandleControl(*this, Value);
	}
}

void UVaesenCharacterCreatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Receives the drags outside the panel (they turn the preview).
	SetVisibility(ESlateVisibility::Visible);
	if (!WidgetTree->RootWidget)
	{
		BuildFrame();
	}
}

void UVaesenCharacterCreatorWidget::NativeDestruct()
{
	if (Scene && Scene->GetCreator())
	{
		Scene->GetCreator()->OnChanged.RemoveDynamic(this, &UVaesenCharacterCreatorWidget::HandleCreatorChanged);
	}
	Super::NativeDestruct();
}

void UVaesenCharacterCreatorWidget::SetScene(AVaesenCharacterCreatorScene* InScene)
{
	if (Scene && Scene->GetCreator())
	{
		Scene->GetCreator()->OnChanged.RemoveDynamic(this, &UVaesenCharacterCreatorWidget::HandleCreatorChanged);
	}
	Scene = InScene;
	if (Scene && Scene->GetCreator())
	{
		Scene->GetCreator()->OnChanged.AddDynamic(this, &UVaesenCharacterCreatorWidget::HandleCreatorChanged);
	}
	BuiltStep = 0xff;
	HandleCreatorChanged();
}

void UVaesenCharacterCreatorWidget::HandleCreatorChanged()
{
	const UVaesenCharacterCreator* const Creator = Scene ? Scene->GetCreator() : nullptr;
	if (!Creator || !Page)
	{
		return;
	}
	Message.Reset();
	if ((uint8)Creator->GetStep() != BuiltStep || Creator->GetAppearance().Sex != BuiltSex)
	{
		BuildPage();
	}
	Refresh();
}

// Layout.

UTextBlock* UVaesenCharacterCreatorWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color)
{
	UTextBlock* const Block = WidgetTree->ConstructWidget<UTextBlock>();
	Block->SetText(FText::FromString(Text));
	FSlateFontInfo Font = Block->GetFont();
	Font.Size = Size;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetAutoWrapText(true);
	return Block;
}

UVaesenCreatorControlHandler* UVaesenCharacterCreatorWidget::MakeHandler(EVaesenCreatorControl Control, FName Id,
	EVaesenAppearancePresetKind Kind, bool bFrame)
{
	UVaesenCreatorControlHandler* const Handler = NewObject<UVaesenCreatorControlHandler>(this);
	Handler->Owner = this;
	Handler->Control = Control;
	Handler->Id = Id;
	Handler->Kind = Kind;
	(bFrame ? FrameHandlers : Handlers).Add(Handler);
	return Handler;
}

UButton* UVaesenCharacterCreatorWidget::MakeButton(const FString& Label, UVaesenCreatorControlHandler* Handler, UTextBlock** OutText)
{
	UButton* const Button = WidgetTree->ConstructWidget<UButton>();
	UTextBlock* const Text = MakeText(Label, 13);
	Text->SetJustification(ETextJustify::Center);
	Button->AddChild(Text);
	Button->SetBackgroundColor(VaesenCreatorWidget::ButtonColor);
	Button->OnClicked.AddDynamic(Handler, &UVaesenCreatorControlHandler::Clicked);
	Handler->Button = Button;
	if (OutText)
	{
		*OutText = Text;
	}
	return Button;
}

void UVaesenCharacterCreatorWidget::BuildFrame()
{
	using namespace VaesenCreatorWidget;

	UCanvasPanel* const Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	// The panel: right side, full height.
	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(PanelColor);
	Panel->SetPadding(FMargin(20.f));
	UCanvasPanelSlot* const PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
	PanelSlot->SetOffsets(FMargin(-470.f, 30.f, 440.f, 30.f));

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	TitleText = MakeText(TEXT(""), 22, HeadingColor);
	Column->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* const ScrollSlot = Column->AddChildToVerticalBox(Scroll);
	ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Page = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(Page);

	StatusText = MakeText(TEXT(""), 12, MutedColor);
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 10.f));

	UHorizontalBox* const Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	Column->AddChildToVerticalBox(Buttons);
	auto AddFrameButton = [this, Buttons](UButton* Button)
	{
		UHorizontalBoxSlot* const ButtonSlot = Buttons->AddChildToHorizontalBox(Button);
		ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ButtonSlot->SetPadding(FMargin(3.f, 0.f));
	};
	AddFrameButton(MakeButton(TEXT("Cancel"), MakeHandler(EVaesenCreatorControl::Cancel, NAME_None, EVaesenAppearancePresetKind::Face, true)));
	BackButton = MakeButton(TEXT("Back"), MakeHandler(EVaesenCreatorControl::Back, NAME_None, EVaesenAppearancePresetKind::Face, true));
	AddFrameButton(BackButton);
	AddFrameButton(MakeButton(TEXT("Randomise"), MakeHandler(EVaesenCreatorControl::Randomise, NAME_None, EVaesenAppearancePresetKind::Face, true)));
	UTextBlock* NextLabel = nullptr;
	NextButton = MakeButton(TEXT("Next"), MakeHandler(EVaesenCreatorControl::Next, NAME_None, EVaesenAppearancePresetKind::Face, true), &NextLabel);
	NextText = NextLabel;
	AddFrameButton(NextButton);

	Column->AddChildToVerticalBox(MakeText(TEXT("Drag beside the panel to turn the character; mouse wheel to zoom."), 11, MutedColor))
		->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
}

void UVaesenCharacterCreatorWidget::AddHeading(const FString& Text)
{
	Page->AddChildToVerticalBox(MakeText(Text, 15, VaesenCreatorWidget::HeadingColor))->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));
}

void UVaesenCharacterCreatorWidget::AddButtonGrid(const TArray<TPair<FString, UVaesenCreatorControlHandler*>>& Buttons)
{
	UUniformGridPanel* const Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Grid->SetSlotPadding(FMargin(3.f));
	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		Grid->AddChildToUniformGrid(MakeButton(Buttons[Index].Key, Buttons[Index].Value), Index / 2, Index % 2);
	}
	Page->AddChildToVerticalBox(Grid);
}

void UVaesenCharacterCreatorWidget::AddOptions(EVaesenAppearancePresetKind Kind, EVaesenCreatorControl Control)
{
	TArray<TPair<FString, UVaesenCreatorControlHandler*>> Buttons;
	for (const FName Id : Scene->GetCreator()->GetOptions(Kind))
	{
		Buttons.Add({ Pretty(Id), MakeHandler(Control, Id, Kind) });
	}
	if (Kind == EVaesenAppearancePresetKind::FacialHair)
	{
		Buttons.Insert({ TEXT("None"), MakeHandler(EVaesenCreatorControl::NoFacialHair) }, 0);
	}
	if (Buttons.IsEmpty())
	{
		Page->AddChildToVerticalBox(MakeText(TEXT("None in the presets yet."), 12, VaesenCreatorWidget::MutedColor));
		return;
	}
	AddButtonGrid(Buttons);
}

void UVaesenCharacterCreatorWidget::AddSlider(const FString& Label, UVaesenCreatorControlHandler* Handler, float Min, float Max)
{
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	USizeBox* const LabelBox = WidgetTree->ConstructWidget<USizeBox>();
	LabelBox->SetWidthOverride(150.f);
	LabelBox->SetContent(MakeText(Label, 13));
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

	USlider* const Slider = WidgetTree->ConstructWidget<USlider>();
	Slider->SetMinValue(Min);
	Slider->SetMaxValue(Max);
	Slider->SetStepSize(0.01f);
	Slider->SetValue(CurrentValue(*Handler));
	Slider->OnValueChanged.AddDynamic(Handler, &UVaesenCreatorControlHandler::ValueChanged);
	Handler->Slider = Slider;
	UHorizontalBoxSlot* const SliderSlot = Row->AddChildToHorizontalBox(Slider);
	SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	SliderSlot->SetVerticalAlignment(VAlign_Center);

	Page->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 4.f));
}

UEditableTextBox* UVaesenCharacterCreatorWidget::AddTextField(const FString& Label, const FString& Value)
{
	Page->AddChildToVerticalBox(MakeText(Label, 13))->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));
	UEditableTextBox* const Box = WidgetTree->ConstructWidget<UEditableTextBox>();
	Box->SetText(FText::FromString(Value));
	Box->OnTextChanged.AddDynamic(this, &UVaesenCharacterCreatorWidget::HandleNameChanged);
	Page->AddChildToVerticalBox(Box);
	return Box;
}

void UVaesenCharacterCreatorWidget::BuildPage()
{
	using K = EVaesenAppearancePresetKind;
	using C = EVaesenCreatorControl;

	UVaesenCharacterCreator* const Creator = Scene->GetCreator();
	Page->ClearChildren();
	Handlers.Reset();
	FirstNameBox = nullptr;
	LastNameBox = nullptr;
	BuiltStep = (uint8)Creator->GetStep();
	BuiltSex = Creator->GetAppearance().Sex;

	switch (Creator->GetStep())
	{
	case EVaesenCreatorStep::Basics:
	{
		AddHeading(TEXT("Sex"));
		TArray<TPair<FString, UVaesenCreatorControlHandler*>> Sexes;
		for (const FName Sex : { FName(TEXT("female")), FName(TEXT("male")) })
		{
			const bool bHasFaces = !UVaesenAppearanceLibrary::PresetsFor(Scene->Presets, K::Face, Sex, true).IsEmpty();
			Sexes.Add({ Pretty(Sex) + (bHasFaces ? TEXT("") : TEXT(" (no faces yet)")), MakeHandler(C::Sex, Sex) });
		}
		AddButtonGrid(Sexes);
		AddHeading(TEXT("Class"));
		TArray<TPair<FString, UVaesenCreatorControlHandler*>> Buttons;
		for (const FString& Class : Classes)
		{
			Buttons.Add({ Class, MakeHandler(C::Class, FName(*Class)) });
		}
		AddButtonGrid(Buttons);
		break;
	}
	case EVaesenCreatorStep::BodyAndSkin:
		AddHeading(TEXT("Body"));
		AddOptions(K::Body);
		AddHeading(TEXT("Skin"));
		AddOptions(K::SkinTone, C::Swatch);
		AddSlider(TEXT("Tone"), MakeHandler(C::SkinTone), 0.f, 1.f);
		AddSlider(TEXT("Warmth"), MakeHandler(C::SkinWarmth), 0.f, 1.f);
		break;
	case EVaesenCreatorStep::Face:
		AddHeading(TEXT("Base face"));
		AddOptions(K::Face);
		AddHeading(TEXT("Shape"));
		for (const FName Slider : Creator->GetOptions(K::Slider))
		{
			if (Slider != TEXT("age"))
			{
				AddSlider(Pretty(Slider), MakeHandler(C::Slider, Slider, K::Slider), -1.f, 1.f);
			}
		}
		AddHeading(TEXT("Age"));
		AddSlider(TEXT("Age"), MakeHandler(C::Age), 0.f, 1.f);
		break;
	case EVaesenCreatorStep::EyesAndHair:
		AddHeading(TEXT("Eyes"));
		AddOptions(K::EyeColor);
		AddHeading(TEXT("Hairstyle"));
		AddOptions(K::HairStyle);
		AddHeading(TEXT("Hair colour"));
		AddOptions(K::HairColor, C::Swatch);
		AddSlider(TEXT("Darkness"), MakeHandler(C::HairMelanin), 0.f, 1.f);
		AddSlider(TEXT("Redness"), MakeHandler(C::HairRedness), 0.f, 1.f);
		AddSlider(TEXT("Grey"), MakeHandler(C::HairGrey), 0.f, 1.f);
		AddSlider(TEXT("Dye"), MakeHandler(C::HairDye), 0.f, 1.f);
		AddHeading(TEXT("Eyebrows"));
		AddOptions(K::Eyebrows);
		AddHeading(TEXT("Facial hair"));
		AddOptions(K::FacialHair);
		break;
	case EVaesenCreatorStep::NameAndReview:
	{
		AddHeading(TEXT("Name"));
		const FString Name = Creator->GetCharacterName();
		FString First, Last;
		if (!Name.Split(TEXT(" "), &First, &Last))
		{
			First = Name;
		}
		FirstNameBox = AddTextField(TEXT("First name"), First);
		LastNameBox = AddTextField(TEXT("Last name"), Last);
		AddHeading(TEXT("Summary"));
		const FVaesenAppearance& Look = Creator->GetAppearance();
		Page->AddChildToVerticalBox(MakeText(FString::Printf(TEXT("%s %s\nFace: %s\nHair: %s\nAge: %d%%"),
			*Pretty(Look.Sex), *Creator->GetClass(), *Pretty(Look.FacePresetId), *Pretty(Look.HairStyleId), FMath::RoundToInt(Look.Age * 100.f)),
			13, VaesenCreatorWidget::MutedColor));
		break;
	}
	}
}

void UVaesenCharacterCreatorWidget::Refresh()
{
	using namespace VaesenCreatorWidget;

	const UVaesenCharacterCreator* const Creator = Scene->GetCreator();
	const int32 Step = (int32)Creator->GetStep();
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d   %s"), Step + 1, StepCount, StepNames[FMath::Clamp(Step, 0, StepCount - 1)])));

	for (const UVaesenCreatorControlHandler* Handler : Handlers)
	{
		if (Handler->Button)
		{
			Handler->Button->SetBackgroundColor(IsSelected(*Handler) ? SelectedColor : ButtonColor);
		}
		if (Handler->Slider && !FMath::IsNearlyEqual(Handler->Slider->GetValue(), CurrentValue(*Handler), 0.001f))
		{
			Handler->Slider->SetValue(CurrentValue(*Handler));
		}
	}

	const bool bLast = Creator->GetStep() == EVaesenCreatorStep::NameAndReview;
	BackButton->SetIsEnabled(Creator->CanGoBack());
	NextButton->SetIsEnabled(bLast ? Creator->CanCreate() : Creator->CanGoNext());
	NextText->SetText(FText::FromString(bLast ? TEXT("Create") : TEXT("Next")));

	FString Status;
	if (!bLast && !Creator->CanGoNext())
	{
		Status = Creator->GetStep() == EVaesenCreatorStep::Basics ? TEXT("Choose a class to continue.") : TEXT("Choose an option of each kind to continue.");
	}
	else if (bLast && !Creator->CanCreate())
	{
		Status = TEXT("Enter a first and a last name (letters, with spaces, apostrophes or hyphens between words).");
	}
	StatusText->SetText(FText::FromString(Status.IsEmpty() ? Message : Status));
}

void UVaesenCharacterCreatorWidget::ShowMessage(const FString& InMessage)
{
	Message = InMessage;
	StatusText->SetText(FText::FromString(Message));
}

// Input.

void UVaesenCharacterCreatorWidget::HandleControl(const UVaesenCreatorControlHandler& Handler, float Value)
{
	using C = EVaesenCreatorControl;

	UVaesenCharacterCreator* const Creator = Scene ? Scene->GetCreator() : nullptr;
	if (!Creator)
	{
		return;
	}
	const FVaesenAppearance& Look = Creator->GetAppearance();
	const bool bOnLastStep = Creator->GetStep() == EVaesenCreatorStep::NameAndReview;
	switch (Handler.Control)
	{
	case C::Next: Creator->Next(); break;
	case C::Back: Creator->Back(); break;
	case C::Cancel:
		if (!bCreating)
		{
			OnFinished.Broadcast(false, FVaesenCharacter());
		}
		return;
	case C::Randomise: Creator->Randomise(++RandomSeed); break;
	case C::Sex:
	{
		const EVaesenSexChange Result = Creator->SetSex(Handler.Id, /*bConfirmed*/ Message.StartsWith(TEXT("Changing")));
		if (Result == EVaesenSexChange::NeedsConfirmation)
		{
			ShowMessage(TEXT("Changing sex resets the body and face. Click again to confirm."));
		}
		else if (Result == EVaesenSexChange::Invalid)
		{
			ShowMessage(TEXT("No faces of that sex in the presets yet."));
		}
		break;
	}
	case C::Class: Creator->SetClass(Handler.Id.ToString()); break;
	case C::Preset: Creator->SetPreset(Handler.Kind, Handler.Id); break;
	case C::NoFacialHair: Creator->SetPreset(EVaesenAppearancePresetKind::FacialHair, NAME_None); break;
	case C::Swatch: Creator->ApplySwatch(Handler.Id); break;
	case C::Slider: Creator->SetSlider(Handler.Id, Value); break;
	case C::Age: Creator->SetAge(Value); break;
	case C::SkinTone: Creator->SetSkinTone(FVector2f(Value, Look.SkinTone.Y)); break;
	case C::SkinWarmth: Creator->SetSkinTone(FVector2f(Look.SkinTone.X, Value)); break;
	case C::HairMelanin: Creator->SetHairColor(Value, Look.HairRedness, Look.HairDye); break;
	case C::HairRedness: Creator->SetHairColor(Look.HairMelanin, Value, Look.HairDye); break;
	case C::HairGrey: Creator->SetHairGrey(Value); break;
	case C::HairDye: Creator->SetHairColor(Look.HairMelanin, Look.HairRedness, Value); break;
	}
	if (Handler.Control == C::Next && bOnLastStep)
	{
		// Next on the last page is Create.
		CreateCharacter();
	}
}

void UVaesenCharacterCreatorWidget::CreateCharacter()
{
	UVaesenCharacterCreator* const Creator = Scene ? Scene->GetCreator() : nullptr;
	if (!Creator || bCreating || !Creator->CanCreate())
	{
		return;
	}
	UVaesenApiSubsystem* const Api = GetGameInstance() ? GetGameInstance()->GetSubsystem<UVaesenApiSubsystem>() : nullptr;
	if (!Api || GuildId.IsEmpty())
	{
		ShowMessage(TEXT("Ready. Creating needs a guild, which this test level doesn't have."));
		return;
	}
	bCreating = true;
	NextButton->SetIsEnabled(false);
	ShowMessage(TEXT("Creating..."));
	Creator->Create(Api, GuildId, [WeakThis = TWeakObjectPtr<UVaesenCharacterCreatorWidget>(this)](bool bSuccess, const FVaesenCharacter& Character, const FString& Error)
	{
		UVaesenCharacterCreatorWidget* const This = WeakThis.Get();
		if (!This)
		{
			return;
		}
		This->bCreating = false;
		if (bSuccess)
		{
			This->OnFinished.Broadcast(true, Character);
			return;
		}
		This->Refresh();
		This->ShowMessage(Error.IsEmpty() ? TEXT("The character couldn't be created.") : FString::Printf(TEXT("Couldn't create the character: %s"), *Error));
	});
}

void UVaesenCharacterCreatorWidget::HandleNameChanged(const FText& Text)
{
	if (Scene && Scene->GetCreator() && FirstNameBox && LastNameBox)
	{
		Scene->GetCreator()->SetName(FirstNameBox->GetText().ToString(), LastNameBox->GetText().ToString());
	}
}

FReply UVaesenCharacterCreatorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UVaesenCharacterCreatorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UVaesenCharacterCreatorWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && Scene)
	{
		Scene->AddTurntableYaw(-InMouseEvent.GetCursorDelta().X * DragDegreesPerPixel);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UVaesenCharacterCreatorWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Scene)
	{
		Scene->AddZoom(InMouseEvent.GetWheelDelta() * ZoomPerWheelNotch);
		return FReply::Handled();
	}
	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

// State.

bool UVaesenCharacterCreatorWidget::IsSelected(const UVaesenCreatorControlHandler& Handler) const
{
	using K = EVaesenAppearancePresetKind;

	const UVaesenCharacterCreator* const Creator = Scene->GetCreator();
	const FVaesenAppearance& Look = Creator->GetAppearance();
	switch (Handler.Control)
	{
	case EVaesenCreatorControl::Sex: return Look.Sex == Handler.Id;
	case EVaesenCreatorControl::Class: return Creator->GetClass().Equals(Handler.Id.ToString(), ESearchCase::IgnoreCase);
	case EVaesenCreatorControl::NoFacialHair: return Look.FacialHairId.IsNone();
	case EVaesenCreatorControl::Preset:
		switch (Handler.Kind)
		{
		case K::Body: return Look.BodyPresetId == Handler.Id;
		case K::Face: return Look.FacePresetId == Handler.Id;
		case K::HairStyle: return Look.HairStyleId == Handler.Id;
		case K::Eyebrows: return Look.EyebrowStyleId == Handler.Id;
		case K::FacialHair: return Look.FacialHairId == Handler.Id;
		case K::EyeColor: return Look.EyeColorId == Handler.Id;
		default: return false;
		}
	default:
		return false;
	}
}

float UVaesenCharacterCreatorWidget::CurrentValue(const UVaesenCreatorControlHandler& Handler) const
{
	const FVaesenAppearance& Look = Scene->GetCreator()->GetAppearance();
	switch (Handler.Control)
	{
	case EVaesenCreatorControl::Slider: return Look.GetSlider(Handler.Id);
	case EVaesenCreatorControl::Age: return Look.Age;
	case EVaesenCreatorControl::SkinTone: return Look.SkinTone.X;
	case EVaesenCreatorControl::SkinWarmth: return Look.SkinTone.Y;
	case EVaesenCreatorControl::HairMelanin: return Look.HairMelanin;
	case EVaesenCreatorControl::HairRedness: return Look.HairRedness;
	case EVaesenCreatorControl::HairGrey: return Look.HairGrey;
	case EVaesenCreatorControl::HairDye: return Look.HairDye;
	default: return 0.f;
	}
}

FString UVaesenCharacterCreatorWidget::Pretty(FName Id)
{
	FString Text = Id.IsNone() ? FString(TEXT("None")) : Id.ToString().Replace(TEXT("_"), TEXT(" "));
	bool bWordStart = true;
	for (TCHAR& Char : Text)
	{
		Char = bWordStart ? FChar::ToUpper(Char) : Char;
		bWordStart = Char == TEXT(' ');
	}
	return Text;
}
