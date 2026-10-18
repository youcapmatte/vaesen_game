#include "VaesenCreateWidgetsCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CircularThrobber.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SkeletalMesh.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Game/VaesenGameModeBase.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/VaesenPlayerController.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/CharacterSelect/VaesenCreateCharacterWidget.h"
#include "UI/Guild/VaesenCreateGuildWidget.h"
#include "UI/Guild/VaesenPickGuildWidget.h"
#include "UI/Loading/VaesenLoadingScreenWidget.h"
#include "UI/Widgets/VaesenButton.h"
#include "UI/Widgets/VaesenComboBox.h"
#include "UI/Widgets/VaesenEditableTextBox.h"
#include "UI/Widgets/VaesenTextBlock.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenCreateWidgets, Log, All);

#define LOCTEXT_NAMESPACE "VaesenCreateWidgets"

namespace VaesenCreateWidgets
{
	const TCHAR* PickGuildPath = TEXT("/Game/Vaesen/UI/Guild/WBP_PickGuild");
	const TCHAR* CreateGuildPath = TEXT("/Game/Vaesen/UI/Guild/WBP_CreateGuild");
	const TCHAR* CreateCharacterPath = TEXT("/Game/Vaesen/UI/CharacterSelect/WBP_CreateCharacter");
	const TCHAR* LoadingScreenPath = TEXT("/Game/Vaesen/UI/Loading/WBP_LoadingScreen");
	const TCHAR* CharacterSelectPath = TEXT("/Game/Vaesen/UI/CharacterSelect/WBP_CharacterSelect");

	const FLinearColor Brass(0.62f, 0.50f, 0.32f);
	const FLinearColor BrassBright(0.85f, 0.70f, 0.45f);

	/** Builds widgets into one blueprint's tree. Named widgets are variables (what BindWidget binds to). */
	struct FBuilder
	{
		UWidgetTree* Tree;

		template <typename T>
		T* Make(FName Name = NAME_None, UClass* Class = T::StaticClass()) const
		{
			T* Widget = Tree->ConstructWidget<T>(Class, Name);
			Widget->bIsVariable = !Name.IsNone();
			return Widget;
		}

		UVaesenTextBlock* Text(FName Name, const FText& Value, EVaesenTextRole Role) const
		{
			UVaesenTextBlock* Widget = Make<UVaesenTextBlock>(Name);
			Widget->Role = Role;
			Widget->SetText(Value);
			return Widget;
		}

		UVaesenButton* Button(FName Name, const FText& Label) const
		{
			UVaesenButton* Widget = Make<UVaesenButton>(Name);
			UVaesenTextBlock* LabelText = Text(NAME_None, Label, EVaesenTextRole::Button);
			if (UButtonSlot* Slot = Cast<UButtonSlot>(Widget->AddChild(LabelText)))
			{
				Slot->SetHorizontalAlignment(HAlign_Center);
				Slot->SetVerticalAlignment(VAlign_Center);
			}
			return Widget;
		}

		UVaesenEditableTextBox* TextBox(FName Name, const FText& Hint) const
		{
			UVaesenEditableTextBox* Widget = Make<UVaesenEditableTextBox>(Name);
			Widget->SetHintText(Hint);
			return Widget;
		}

		static UVerticalBoxSlot* AddRow(UVerticalBox* Box, UWidget* Child, float Bottom = 10.0f)
		{
			UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child);
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, Bottom));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			return Slot;
		}

		/** A labelled field: small label above the control. */
		void AddField(UVerticalBox* Box, const FText& Label, UWidget* Field) const
		{
			AddRow(Box, Text(NAME_None, Label, EVaesenTextRole::Label), 4.0f);
			AddRow(Box, Field, 14.0f);
		}

		/** Right-aligned buttons. */
		UHorizontalBox* ButtonRow(TArray<UWidget*> Buttons) const
		{
			UHorizontalBox* Row = Make<UHorizontalBox>();
			UHorizontalBoxSlot* SpacerSlot = Row->AddChildToHorizontalBox(Make<USpacer>());
			SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			for (UWidget* ButtonWidget : Buttons)
			{
				UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(ButtonWidget);
				Slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			}
			return Row;
		}

		/** Full-screen dimmed backdrop with a centred panel; returns the panel's content box. */
		UVerticalBox* DialogFrame(float Width) const
		{
			UBorder* Backdrop = Make<UBorder>(TEXT("Backdrop"));
			Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
			Backdrop->SetPadding(FMargin(0.0f));
			Backdrop->SetHorizontalAlignment(HAlign_Center);
			Backdrop->SetVerticalAlignment(VAlign_Center);
			Backdrop->bIsVariable = false;
			Tree->RootWidget = Backdrop;

			USizeBox* Size = Make<USizeBox>();
			Size->SetWidthOverride(Width);
			Backdrop->SetContent(Size);

			UBorder* Panel = Make<UBorder>(TEXT("Panel"));
			Panel->bIsVariable = false;
			Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.06f, 0.05f, 0.04f, 0.97f), 8.0f, Brass, 1.0f));
			Panel->SetPadding(FMargin(28.0f, 24.0f));
			Size->SetContent(Panel);

			UVerticalBox* Content = Make<UVerticalBox>(TEXT("Content"));
			Content->bIsVariable = false;
			Panel->SetContent(Content);
			return Content;
		}

		UVaesenTextBlock* StatusLine() const
		{
			UVaesenTextBlock* Status = Text(TEXT("StatusText"), FText::GetEmpty(), EVaesenTextRole::Status);
			Status->SetAutoWrapText(true);
			return Status;
		}
	};

	bool AssetExists(const FString& PackagePath)
	{
		return FPackageName::DoesPackageExist(PackagePath);
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, Args);
		UE_LOG(LogVaesenCreateWidgets, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *FileName);
		return bSaved;
	}

	/** Creates an empty widget blueprint, or null when the asset already exists. */
	UWidgetBlueprint* CreateBlueprint(const FString& PackagePath, UClass* ParentClass)
	{
		if (AssetExists(PackagePath))
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("%s already exists; leaving it alone."), *PackagePath);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UWidgetBlueprint* Blueprint = CastChecked<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(FPackageName::GetShortName(PackagePath)), BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
		FAssetRegistryModule::AssetCreated(Blueprint);
		return Blueprint;
	}

	bool Finish(UBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogVaesenCreateWidgets, Error, TEXT("%s failed to compile."), *Blueprint->GetPathName());
		}
		return Save(Blueprint);
	}

	/** Creates an empty (data-only) Blueprint, or null when the asset already exists. */
	UBlueprint* CreateActorBlueprint(const FString& PackagePath, UClass* ParentClass)
	{
		if (AssetExists(PackagePath))
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("%s already exists; leaving it alone."), *PackagePath);
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackagePath);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(FPackageName::GetShortName(PackagePath)), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}

	UObject* LoadAsset(const TCHAR* ObjectPath)
	{
		UObject* Asset = LoadObject<UObject>(nullptr, ObjectPath);
		if (!Asset)
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("%s not found."), ObjectPath);
		}
		return Asset;
	}

	/** Sets an object or class property (by name, so protected ones too) on a class default object. */
	void SetDefault(UObject* Defaults, const TCHAR* PropertyName, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), PropertyName))
		{
			Property->SetObjectPropertyValue_InContainer(Defaults, Value);
		}
		else
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("%s has no property %s."), *Defaults->GetClass()->GetName(), PropertyName);
		}
	}

	UClass* LoadBlueprintClass(const FString& PackagePath)
	{
		const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	const TCHAR* PlayerCharacterPath = TEXT("/Game/Vaesen/Blueprints/Characters/BP_VaesenPlayerCharacter");
	const TCHAR* PlayerControllerPath = TEXT("/Game/Vaesen/Blueprints/Player/BP_VaesenPlayerController");
	const TCHAR* GameModePath = TEXT("/Game/Vaesen/Blueprints/Game/BP_VaesenGameMode");

	/**
	 * Data-only Blueprints (defaults, no graph logic) for playing in the world: the character with the UE mannequin
	 * (Content/Characters/Mannequins) and the template input (Content/ThirdPerson/Input), its controller, and the game
	 * mode that uses both (the project's default game mode, so every world map gets it).
	 */
	void CreatePlayerBlueprints()
	{
		if (UBlueprint* Blueprint = CreateActorBlueprint(PlayerCharacterPath, AVaesenPlayerCharacter::StaticClass()))
		{
			AVaesenPlayerCharacter* Defaults = CastChecked<AVaesenPlayerCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
			USkeletalMeshComponent* Mesh = Defaults->GetMesh();
			if (USkeletalMesh* Manny = Cast<USkeletalMesh>(LoadAsset(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny"))))
			{
				Mesh->SetSkeletalMeshAsset(Manny);
			}
			// The mannequin's origin is at its feet and it faces +Y; the capsule's origin is its centre (half-height 96) and it faces +X.
			Mesh->SetRelativeLocationAndRotation(FVector(0.0, 0.0, -96.0), FRotator(0.0, -90.0, 0.0));
			Mesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			Mesh->AnimClass = Cast<UClass>(LoadAsset(TEXT("/Game/Characters/Mannequins/Animations/ABP_Manny.ABP_Manny_C")));

			SetDefault(Defaults, TEXT("MoveAction"), LoadAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Move.IA_Move")));
			SetDefault(Defaults, TEXT("LookAction"), LoadAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Look.IA_Look")));
			SetDefault(Defaults, TEXT("JumpAction"), LoadAsset(TEXT("/Game/ThirdPerson/Input/Actions/IA_Jump.IA_Jump")));
			Finish(Blueprint);
		}

		if (UBlueprint* Blueprint = CreateActorBlueprint(PlayerControllerPath, AVaesenPlayerController::StaticClass()))
		{
			SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("DefaultMappingContext"),
				LoadAsset(TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default")));
			Finish(Blueprint);
		}

		if (UBlueprint* Blueprint = CreateActorBlueprint(GameModePath, AVaesenGameModeBase::StaticClass()))
		{
			AVaesenGameModeBase* Defaults = CastChecked<AVaesenGameModeBase>(Blueprint->GeneratedClass->GetDefaultObject());
			if (UClass* Character = LoadBlueprintClass(PlayerCharacterPath))
			{
				Defaults->DefaultPawnClass = Character;
			}
			if (UClass* Controller = LoadBlueprintClass(PlayerControllerPath))
			{
				Defaults->PlayerControllerClass = Controller;
			}
			Finish(Blueprint);
		}
	}

	/**
	 * Camera zoom: IA_Zoom (1D axis), mapped to the mouse wheel in IMC_Default and set on BP_VaesenPlayerCharacter.
	 * Each part is only added when missing.
	 */
	void SetupZoomInput()
	{
		const TCHAR* ZoomPath = TEXT("/Game/Vaesen/Input/IA_Zoom");
		UInputAction* Zoom = LoadObject<UInputAction>(nullptr, TEXT("/Game/Vaesen/Input/IA_Zoom.IA_Zoom"));
		if (!Zoom)
		{
			UPackage* Package = CreatePackage(ZoomPath);
			Zoom = NewObject<UInputAction>(Package, TEXT("IA_Zoom"), RF_Public | RF_Standalone);
			Zoom->ValueType = EInputActionValueType::Axis1D;
			FAssetRegistryModule::AssetCreated(Zoom);
			Save(Zoom);
		}

		if (UInputMappingContext* Mappings = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default")))
		{
			const bool bMapped = Mappings->GetMappings().ContainsByPredicate([Zoom](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Zoom;
			});
			if (!bMapped)
			{
				// Wheel up = +1 = zoom in.
				Mappings->MapKey(Zoom, EKeys::MouseWheelAxis);
				Save(Mappings);
			}
		}

		if (UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter"))))
		{
			UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), TEXT("ZoomAction"));
			if (Property && !Property->GetObjectPropertyValue_InContainer(Defaults))
			{
				SetDefault(Defaults, TEXT("ZoomAction"), Zoom);
				Finish(Character);
			}
		}
	}

	UClass* LoadWidgetClass(const FString& PackagePath)
	{
		const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
		const UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	void CreatePickGuild()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(PickGuildPath, UVaesenPickGuildWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}

		const FBuilder B{ Blueprint->WidgetTree };
		UVerticalBox* Content = B.DialogFrame(480.0f);
		FBuilder::AddRow(Content, B.Text(NAME_None, LOCTEXT("PickGuildTitle", "Change guild"), EVaesenTextRole::Heading), 18.0f);
		B.AddField(Content, LOCTEXT("GuildLabel", "Guild"), B.Make<UVaesenComboBox>(TEXT("GuildComboBox")));
		FBuilder::AddRow(Content, B.ButtonRow({
			B.Button(TEXT("CancelButton"), LOCTEXT("Cancel", "Cancel")),
			B.Button(TEXT("SelectButton"), LOCTEXT("Select", "Select")) }), 8.0f);
		FBuilder::AddRow(Content, B.StatusLine(), 0.0f);
		Finish(Blueprint);
	}

	void CreateCreateGuild()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(CreateGuildPath, UVaesenCreateGuildWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}

		const FBuilder B{ Blueprint->WidgetTree };
		UVerticalBox* Content = B.DialogFrame(480.0f);
		FBuilder::AddRow(Content, B.Text(NAME_None, LOCTEXT("CreateGuildTitle", "Found a guild"), EVaesenTextRole::Heading), 6.0f);
		UVaesenTextBlock* Intro = B.Text(NAME_None, LOCTEXT("CreateGuildIntro", "You will be its guild master."), EVaesenTextRole::Label);
		FBuilder::AddRow(Content, Intro, 18.0f);
		B.AddField(Content, LOCTEXT("GuildNameLabel", "Guild name"),
			B.TextBox(TEXT("GuildNameTextBox"), LOCTEXT("GuildNameHint", "3-32 letters, e.g. The Lantern Watch")));
		FBuilder::AddRow(Content, B.ButtonRow({
			B.Button(TEXT("CancelButton"), LOCTEXT("Cancel", "Cancel")),
			B.Button(TEXT("CreateButton"), LOCTEXT("FoundGuild", "Found guild")) }), 8.0f);
		FBuilder::AddRow(Content, B.StatusLine(), 0.0f);
		Finish(Blueprint);
	}

	void CreateCreateCharacter()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(CreateCharacterPath, UVaesenCreateCharacterWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}

		const FBuilder B{ Blueprint->WidgetTree };
		UVerticalBox* Content = B.DialogFrame(520.0f);
		FBuilder::AddRow(Content, B.Text(NAME_None, LOCTEXT("CreateCharacterTitle", "New investigator"), EVaesenTextRole::Heading), 18.0f);
		B.AddField(Content, LOCTEXT("FirstNameLabel", "First name"), B.TextBox(TEXT("FirstNameTextBox"), LOCTEXT("FirstNameHint", "e.g. Astrid")));
		B.AddField(Content, LOCTEXT("LastNameLabel", "Last name"), B.TextBox(TEXT("LastNameTextBox"), LOCTEXT("LastNameHint", "e.g. Falk")));

		UVerticalBoxSlot* RandomizeSlot = FBuilder::AddRow(Content, B.Button(TEXT("RandomizeButton"), LOCTEXT("Randomize", "Randomize name")), 18.0f);
		RandomizeSlot->SetHorizontalAlignment(HAlign_Left);

		B.AddField(Content, LOCTEXT("CharacterNameLabel", "Name"), B.Text(TEXT("CharacterNameText"), FText::GetEmpty(), EVaesenTextRole::Heading));
		B.AddField(Content, LOCTEXT("ClassLabel", "Class"), B.Make<UVaesenComboBox>(TEXT("ClassComboBox")));
		FBuilder::AddRow(Content, B.ButtonRow({
			B.Button(TEXT("CancelButton"), LOCTEXT("Cancel", "Cancel")),
			B.Button(TEXT("CreateButton"), LOCTEXT("Create", "Create")) }), 8.0f);
		FBuilder::AddRow(Content, B.StatusLine(), 0.0f);
		Finish(Blueprint);
	}

	void CreateLoadingScreen()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(LoadingScreenPath, UVaesenLoadingScreenWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}

		const FBuilder B{ Blueprint->WidgetTree };
		UOverlay* Root = B.Make<UOverlay>(TEXT("Root"));
		Root->bIsVariable = false;
		Blueprint->WidgetTree->RootWidget = Root;

		// Black behind everything, so there's no flash of the world when no background is set.
		UBorder* Black = B.Make<UBorder>();
		Black->SetBrushColor(FLinearColor::Black);
		UOverlaySlot* BlackSlot = Root->AddChildToOverlay(Black);
		BlackSlot->SetHorizontalAlignment(HAlign_Fill);
		BlackSlot->SetVerticalAlignment(VAlign_Fill);

		UImage* Background = B.Make<UImage>(TEXT("BackgroundImage"));
		UOverlaySlot* BackgroundSlot = Root->AddChildToOverlay(Background);
		BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
		BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

		// Darkens the art so the text stays readable.
		UBorder* Shade = B.Make<UBorder>();
		Shade->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
		UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
		ShadeSlot->SetVerticalAlignment(VAlign_Fill);

		UVerticalBox* Info = B.Make<UVerticalBox>();
		UOverlaySlot* InfoSlot = Root->AddChildToOverlay(Info);
		InfoSlot->SetHorizontalAlignment(HAlign_Fill);
		InfoSlot->SetVerticalAlignment(VAlign_Bottom);
		InfoSlot->SetPadding(FMargin(96.0f, 0.0f, 96.0f, 64.0f));

		FBuilder::AddRow(Info, B.Text(TEXT("MapNameText"), LOCTEXT("MapNamePreview", "Gyllencreutz"), EVaesenTextRole::Title), 8.0f);
		UVaesenTextBlock* Tip = B.Text(TEXT("TipText"), LOCTEXT("TipPreview", "Vaesen are bound by old rules. Learn a creature's weakness before you face it."), EVaesenTextRole::Body);
		Tip->SetAutoWrapText(true);
		FBuilder::AddRow(Info, Tip, 28.0f);

		UHorizontalBox* StatusRow = B.Make<UHorizontalBox>();
		UCircularThrobber* Throbber = B.Make<UCircularThrobber>(TEXT("Throbber"));
		Throbber->bIsVariable = false;
		Throbber->SetRadius(14.0f);
		UHorizontalBoxSlot* ThrobberSlot = StatusRow->AddChildToHorizontalBox(Throbber);
		ThrobberSlot->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* StatusSlot = StatusRow->AddChildToHorizontalBox(B.Text(TEXT("StatusText"), LOCTEXT("StatusPreview", "Loading Gyllencreutz..."), EVaesenTextRole::Label));
		StatusSlot->SetVerticalAlignment(VAlign_Center);
		StatusSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		FBuilder::AddRow(Info, StatusRow, 12.0f);

		USizeBox* BarSize = B.Make<USizeBox>();
		BarSize->SetHeightOverride(6.0f);
		UProgressBar* Bar = B.Make<UProgressBar>(TEXT("ProgressBar"));
		Bar->SetFillColorAndOpacity(BrassBright);
		Bar->SetPercent(0.35f);
		BarSize->SetContent(Bar);
		FBuilder::AddRow(Info, BarSize, 0.0f);

		Finish(Blueprint);
	}

	/** Adds the controls WBP_CharacterSelect is missing to its root Canvas Panel; existing ones are left as they are. */
	void UpdateCharacterSelect()
	{
		const FString ObjectPath = FString(CharacterSelectPath) + TEXT(".WBP_CharacterSelect");
		UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPath);
		if (!Blueprint)
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("%s not found; skipping."), CharacterSelectPath);
			return;
		}

		UCanvasPanel* Canvas = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
		if (!Canvas)
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("WBP_CharacterSelect's root isn't a Canvas Panel; add its controls by hand."));
			return;
		}

		const FBuilder B{ Blueprint->WidgetTree };
		const auto Missing = [&Blueprint](const TCHAR* Name) { return Blueprint->WidgetTree->FindWidget(FName(Name)) == nullptr; };
		bool bChanged = false;

		// Guild header, top left: name, Change Guild / Found a Guild, New Character.
		if (Missing(TEXT("GuildNameText")) || Missing(TEXT("ChangeGuildButton")) || Missing(TEXT("CreateGuildButton")) || Missing(TEXT("CreateCharacterButton")))
		{
			UVerticalBox* Panel = B.Make<UVerticalBox>(TEXT("GuildPanel"));
			Panel->bIsVariable = false;
			if (Missing(TEXT("GuildNameText")))
			{
				FBuilder::AddRow(Panel, B.Text(TEXT("GuildNameText"), LOCTEXT("GuildNamePreview", "The Lantern Watch"), EVaesenTextRole::Heading), 12.0f);
			}

			UHorizontalBox* GuildButtons = B.Make<UHorizontalBox>();
			if (Missing(TEXT("ChangeGuildButton")))
			{
				GuildButtons->AddChildToHorizontalBox(B.Button(TEXT("ChangeGuildButton"), LOCTEXT("ChangeGuild", "Change guild")))
					->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			}
			if (Missing(TEXT("CreateGuildButton")))
			{
				GuildButtons->AddChildToHorizontalBox(B.Button(TEXT("CreateGuildButton"), LOCTEXT("FoundAGuild", "Found a guild")));
			}
			if (GuildButtons->GetChildrenCount() > 0)
			{
				FBuilder::AddRow(Panel, GuildButtons, 12.0f)->SetHorizontalAlignment(HAlign_Left);
			}

			if (Missing(TEXT("CreateCharacterButton")))
			{
				FBuilder::AddRow(Panel, B.Button(TEXT("CreateCharacterButton"), LOCTEXT("NewCharacter", "New character")), 0.0f)
					->SetHorizontalAlignment(HAlign_Left);
			}

			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Panel);
			Slot->SetAnchors(FAnchors(0.0f, 0.0f));
			Slot->SetPosition(FVector2D(60.0f, 60.0f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(10);
			bChanged = true;
		}

		// Character panel, bottom centre: the guild's characters, Play and the status line. Replaces the plain
		// PlayerComboBox / PlayButton / StatusText with Vaesen controls of the same names, wherever they were, and
		// drops the old GuildComboBox (Change Guild replaced it).
		if (Missing(TEXT("CharacterPanel")))
		{
			for (const TCHAR* Name : { TEXT("PlayerComboBox"), TEXT("PlayButton"), TEXT("StatusText"), TEXT("GuildComboBox") })
			{
				if (UWidget* Old = Blueprint->WidgetTree->FindWidget(FName(Name)))
				{
					Old->RemoveFromParent();
					Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
					UE_LOG(LogVaesenCreateWidgets, Display, TEXT("Replaced %s."), Name);
				}
			}

			USizeBox* Size = B.Make<USizeBox>();
			Size->SetWidthOverride(440.0f);
			UVerticalBox* Panel = B.Make<UVerticalBox>(TEXT("CharacterPanel"));
			Panel->bIsVariable = false;
			Size->SetContent(Panel);

			FBuilder::AddRow(Panel, B.Text(NAME_None, LOCTEXT("CharacterLabel", "Character"), EVaesenTextRole::Label), 4.0f);
			FBuilder::AddRow(Panel, B.Make<UVaesenComboBox>(TEXT("PlayerComboBox")), 12.0f);
			FBuilder::AddRow(Panel, B.Button(TEXT("PlayButton"), LOCTEXT("Play", "Play")), 12.0f);
			UVaesenTextBlock* Status = B.StatusLine();
			Status->SetJustification(ETextJustify::Center);
			FBuilder::AddRow(Panel, Status, 0.0f);

			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Size);
			Slot->SetAnchors(FAnchors(0.5f, 1.0f));
			Slot->SetAlignment(FVector2D(0.5f, 1.0f));
			Slot->SetPosition(FVector2D(0.0f, -80.0f));
			Slot->SetAutoSize(true);
			Slot->SetZOrder(10);
			bChanged = true;
		}

		if (Missing(TEXT("LoadingThrobber")))
		{
			UCircularThrobber* Throbber = B.Make<UCircularThrobber>(TEXT("LoadingThrobber"));
			Throbber->SetRadius(24.0f);
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Throbber);
			Slot->SetAnchors(FAnchors(0.5f, 0.5f));
			Slot->SetAlignment(FVector2D(0.5f, 0.5f));
			Slot->SetPosition(FVector2D::ZeroVector);
			Slot->SetAutoSize(true);
			Slot->SetZOrder(50);
			bChanged = true;
		}

		// The dialogs cover the whole screen (they dim it themselves) and start hidden.
		const TPair<const TCHAR*, const TCHAR*> Dialogs[] = {
			{ TEXT("PickGuildDialog"), PickGuildPath },
			{ TEXT("CreateGuildDialog"), CreateGuildPath },
			{ TEXT("CreateCharacterDialog"), CreateCharacterPath },
		};
		for (const TPair<const TCHAR*, const TCHAR*>& Dialog : Dialogs)
		{
			if (!Missing(Dialog.Key))
			{
				continue;
			}

			UClass* DialogClass = LoadWidgetClass(Dialog.Value);
			if (!DialogClass)
			{
				UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("%s isn't available; %s not added."), Dialog.Value, Dialog.Key);
				continue;
			}

			UUserWidget* Widget = B.Make<UUserWidget>(Dialog.Key, DialogClass);
			Widget->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
			Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			Slot->SetOffsets(FMargin(0.0f));
			Slot->SetZOrder(100);
			bChanged = true;
		}

		if (bChanged)
		{
			Finish(Blueprint);
		}
		else
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("WBP_CharacterSelect already has every control."));
		}
	}
}

UVaesenCreateWidgetsCommandlet::UVaesenCreateWidgetsCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVaesenCreateWidgetsCommandlet::Main(const FString& Params)
{
	using namespace VaesenCreateWidgets;

	// Dialogs first: WBP_CharacterSelect embeds them.
	CreatePickGuild();
	CreateCreateGuild();
	CreateCreateCharacter();
	CreateLoadingScreen();
	UpdateCharacterSelect();
	CreatePlayerBlueprints();
	SetupZoomInput();
	return 0;
}

#undef LOCTEXT_NAMESPACE
