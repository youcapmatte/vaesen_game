#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Appearance/VaesenAppearancePresets.h"
#include "Online/VaesenApiTypes.h"
#include "VaesenCharacterCreatorWidget.generated.h"

class AVaesenCharacterCreatorScene;
class UButton;
class USlider;
class UTextBlock;
class UVerticalBox;
class UEditableTextBox;
class UVaesenCharacterCreatorWidget;

/** The creator closed: with the new character, or cancelled (bCreated false). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FVaesenCreatorFinishedSignature, bool /*bCreated*/, const FVaesenCharacter& /*Character*/);

/** What a control in the creator panel does. */
UENUM()
enum class EVaesenCreatorControl : uint8
{
	Next,
	Back,
	Cancel,
	Randomise,
	Sex,
	Class,
	Preset,
	NoFacialHair,
	Swatch,
	Slider,
	Age,
	SkinTone,
	SkinWarmth,
	HairMelanin,
	HairRedness,
	HairGrey,
	HairDye,
};

/** One control's callback target (UMG's click and value events carry no payload). */
UCLASS()
class VAESENGAME_API UVaesenCreatorControlHandler : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UVaesenCharacterCreatorWidget> Owner;
	EVaesenCreatorControl Control = EVaesenCreatorControl::Next;
	EVaesenAppearancePresetKind Kind = EVaesenAppearancePresetKind::Face;
	FName Id;

	UPROPERTY()
	TObjectPtr<UButton> Button;

	UPROPERTY()
	TObjectPtr<USlider> Slider;

	UFUNCTION()
	void Clicked();

	UFUNCTION()
	void ValueChanged(float Value);
};

/**
 * The character creator's panel: one page per step (sex and class; body and skin; base face, sculpt sliders and age;
 * eyes and hair; name), Back / Next / Randomise, turning the preview by dragging anywhere outside the panel, and zooming
 * with the mouse wheel.
 * Drives the scene's UVaesenCharacterCreator and shows its state. A Widget Blueprint can subclass it with its own
 * design; without one it builds a plain layout in code.
 */
UCLASS()
class VAESENGAME_API UVaesenCharacterCreatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** The guild Create adds the character to; none: Create only checks the character (test levels). */
	void SetGuild(const FString& InGuildId) { GuildId = InGuildId; }

	FVaesenCreatorFinishedSignature OnFinished;

	/** Shows and drives this scene's creator (open it first). */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetScene(AVaesenCharacterCreatorScene* InScene);

	/** The classes offered on the first step (the API's list). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	TArray<FString> Classes = { TEXT("Academic"), TEXT("Doctor"), TEXT("Hunter"), TEXT("Occultist"), TEXT("Officer"),
		TEXT("Priest"), TEXT("Private Detective"), TEXT("Servant"), TEXT("Vagabond"), TEXT("Writer") };

	/** Turntable degrees per pixel of mouse drag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	float DragDegreesPerPixel = 0.5f;

	/** Zoom per mouse wheel notch (the whole range, body to face, is 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	float ZoomPerWheelNotch = 0.12f;

	/** Called by the panel's controls. */
	void HandleControl(const UVaesenCreatorControlHandler& Handler, float Value);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	UPROPERTY()
	TObjectPtr<AVaesenCharacterCreatorScene> Scene;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Page;

	UPROPERTY()
	TObjectPtr<UButton> BackButton;

	UPROPERTY()
	TObjectPtr<UButton> NextButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> NextText;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> FirstNameBox;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> LastNameBox;

	/** The current page's controls (kept alive, refreshed after every change). */
	UPROPERTY()
	TArray<TObjectPtr<UVaesenCreatorControlHandler>> Handlers;

	/** The fixed controls (Back, Next, Randomise). */
	UPROPERTY()
	TArray<TObjectPtr<UVaesenCreatorControlHandler>> FrameHandlers;

	/** What the page was built for: rebuilt when either changes, refreshed otherwise (so a slider drag isn't cut). */
	FString GuildId;
	bool bCreating = false;
	/** The last thing the panel said (Create's answer, a confirmation); shown until the player changes something. */
	FString Message;
	void ShowMessage(const FString& InMessage);
	uint8 BuiltStep = 0xff;
	FName BuiltSex;
	int32 RandomSeed = 0;
	bool bDragging = false;

	UFUNCTION()
	void HandleCreatorChanged();

	UFUNCTION()
	void HandleNameChanged(const FText& Text);

	void BuildFrame();
	void BuildPage();
	void Refresh();
	void CreateCharacter();

	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color = FLinearColor::White);
	UVaesenCreatorControlHandler* MakeHandler(EVaesenCreatorControl Control, FName Id = NAME_None,
		EVaesenAppearancePresetKind Kind = EVaesenAppearancePresetKind::Face, bool bFrame = false);
	UButton* MakeButton(const FString& Label, UVaesenCreatorControlHandler* Handler, UTextBlock** OutText = nullptr);
	void AddHeading(const FString& Text);
	/** A grid of option buttons, two per row. */
	void AddOptions(EVaesenAppearancePresetKind Kind, EVaesenCreatorControl Control = EVaesenCreatorControl::Preset);
	void AddButtonGrid(const TArray<TPair<FString, UVaesenCreatorControlHandler*>>& Buttons);
	void AddSlider(const FString& Label, UVaesenCreatorControlHandler* Handler, float Min, float Max);
	UEditableTextBox* AddTextField(const FString& Label, const FString& Value);

	bool IsSelected(const UVaesenCreatorControlHandler& Handler) const;
	float CurrentValue(const UVaesenCreatorControlHandler& Handler) const;
	static FString Pretty(FName Id);
};
