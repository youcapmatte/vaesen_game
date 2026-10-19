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
#include "Components/UniformGridPanel.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenPickup.h"
#include "Items/VaesenIngredientItem.h"
#include "Items/VaesenPouchItem.h"
#include "Items/VaesenArmorItem.h"
#include "GameplayTagContainer.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Inventory/VaesenLootContainer.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Game/VaesenGameModeBase.h"
#include "VaesenMannequinSplitter.h"
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
#include "UI/Interaction/VaesenInteractionCardWidget.h"
#include "UI/Inventory/VaesenDropZoneWidget.h"
#include "UI/Inventory/VaesenQuickBarWidget.h"
#include "UI/Minimap/VaesenMinimapWidget.h"
#include "UI/VaesenHUDWidget.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture2D.h"
#include "UI/Inventory/VaesenQuickSlotWidget.h"
#include "Items/VaesenConsumableItem.h"
#include "UI/Inventory/VaesenEquipmentPanelWidget.h"
#include "UI/Inventory/VaesenEquipmentSlotWidget.h"
#include "UI/Inventory/VaesenInventoryPanelWidget.h"
#include "UI/Inventory/VaesenInventoryScreenWidget.h"
#include "UI/Inventory/VaesenInventorySlotWidget.h"
#include "UI/Inventory/VaesenItemTooltipWidget.h"
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

	/**
	 * Modular body: splits Manny into parts (once) and puts them on BP_VaesenPlayerCharacter: the Torso as the
	 * character's Mesh, the rest on the Head/Hands/Legs/Feet parts. Only fills in what's still the whole Manny or empty.
	 */
	void SetupModularCharacter()
	{
		if (!VaesenMannequinSplitter::SplitMannequin())
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("Splitting the mannequin had problems; see above."));
		}

		UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter")));
		AVaesenBaseCharacter* Defaults = Character ? Cast<AVaesenBaseCharacter>(Character->GeneratedClass->GetDefaultObject()) : nullptr;
		if (!Defaults)
		{
			return;
		}

		const auto LoadPart = [](const TCHAR* Name)
		{
			const FString Path = FString(VaesenMannequinSplitter::PartsFolder) / Name + TEXT(".") + Name;
			return LoadObject<USkeletalMesh>(nullptr, *Path);
		};

		bool bChanged = false;
		const auto Assign = [&bChanged](USkeletalMeshComponent* Component, USkeletalMesh* Part, bool bReplaceWholeManny)
		{
			if (!Component || !Part)
			{
				return;
			}
			const USkeletalMesh* Current = Component->GetSkeletalMeshAsset();
			const bool bIsWholeManny = Current && Current->GetName() == TEXT("SKM_Manny");
			if (!Current || (bReplaceWholeManny && bIsWholeManny))
			{
				Component->SetSkeletalMeshAsset(Part);
				bChanged = true;
			}
		};

		Assign(Defaults->GetTorsoMesh(), LoadPart(TEXT("SKM_Manny_Torso")), true);
		Assign(Defaults->GetHeadMesh(), LoadPart(TEXT("SKM_Manny_Head")), false);
		Assign(Defaults->GetHandsMesh(), LoadPart(TEXT("SKM_Manny_Hands")), false);
		Assign(Defaults->GetLegsMesh(), LoadPart(TEXT("SKM_Manny_Legs")), false);
		Assign(Defaults->GetFeetMesh(), LoadPart(TEXT("SKM_Manny_Feet")), false);

		if (bChanged)
		{
			Finish(Character);
		}
		else
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("BP_VaesenPlayerCharacter already has its body parts."));
		}
	}

	const TCHAR* OutlineMaterialPath = TEXT("/Game/Vaesen/Materials/PP_InteractionOutline");
	const TCHAR* InteractableTestPath = TEXT("/Game/Vaesen/Blueprints/Interaction/BP_InteractableTest");

	/**
	 * Post-process outline for UVaesenInteractableComponent: pixels just outside meshes drawn with its stencil value
	 * become OutlineColor (a faint tint covers the mesh itself). After tonemapping, so the colour is exactly what's set.
	 */
	UMaterial* CreateOutlineMaterial()
	{
		if (AssetExists(OutlineMaterialPath))
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("%s already exists; leaving it alone."), OutlineMaterialPath);
			return LoadObject<UMaterial>(nullptr, *(FString(OutlineMaterialPath) + TEXT(".PP_InteractionOutline")));
		}

		UPackage* Package = CreatePackage(OutlineMaterialPath);
		UMaterial* Material = NewObject<UMaterial>(Package, TEXT("PP_InteractionOutline"), RF_Public | RF_Standalone);
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = BL_SceneColorAfterTonemapping;

		const auto Add = [Material](TSubclassOf<UMaterialExpression> Class, int32 X, int32 Y)
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
		};

		// The scene textures are also fed in as inputs so the shader binds them (Custom code alone doesn't).
		UMaterialExpressionSceneTexture* SceneColor = CastChecked<UMaterialExpressionSceneTexture>(Add(UMaterialExpressionSceneTexture::StaticClass(), -600, -200));
		SceneColor->SceneTextureId = PPI_PostProcessInput0;
		UMaterialExpressionSceneTexture* Stencil = CastChecked<UMaterialExpressionSceneTexture>(Add(UMaterialExpressionSceneTexture::StaticClass(), -600, 0));
		Stencil->SceneTextureId = PPI_CustomStencil;

		UMaterialExpressionVectorParameter* Color = CastChecked<UMaterialExpressionVectorParameter>(Add(UMaterialExpressionVectorParameter::StaticClass(), -600, 200));
		Color->ParameterName = TEXT("OutlineColor");
		Color->DefaultValue = FLinearColor(1.0f, 0.78f, 0.2f);

		const auto Scalar = [&Add](const TCHAR* Name, float Value, int32 Y)
		{
			UMaterialExpressionScalarParameter* Parameter = CastChecked<UMaterialExpressionScalarParameter>(Add(UMaterialExpressionScalarParameter::StaticClass(), -600, Y));
			Parameter->ParameterName = Name;
			Parameter->DefaultValue = Value;
			return Parameter;
		};
		UMaterialExpressionScalarParameter* Thickness = Scalar(TEXT("Thickness"), 2.0f, 400);
		UMaterialExpressionScalarParameter* Fill = Scalar(TEXT("FillOpacity"), 0.06f, 500);
		UMaterialExpressionScalarParameter* StencilValue = Scalar(TEXT("StencilValue"), UVaesenInteractionComponent::HighlightStencilValue, 600);

		UMaterialExpressionCustom* Outline = CastChecked<UMaterialExpressionCustom>(Add(UMaterialExpressionCustom::StaticClass(), -250, 0));
		Outline->Description = TEXT("Outline");
		Outline->OutputType = CMOT_Float3;
		Outline->Inputs.Reset();
		for (const TCHAR* Name : { TEXT("SceneColor"), TEXT("Stencil"), TEXT("Color"), TEXT("Thickness"), TEXT("Fill"), TEXT("StencilValue") })
		{
			FCustomInput& Input = Outline->Inputs.AddDefaulted_GetRef();
			Input.InputName = Name;
		}
		Outline->Code = TEXT(R"(float Inside = abs(Stencil.r - StencilValue) < 0.5 ? 1.0 : 0.0;
float Edge = 0.0;
float Radius = max(Thickness, 1.0);
for (int i = 0; i < 8; i++)
{
	float Angle = i * 0.7853982;
	float Neighbour = SceneTextureFetch(PPI_CustomStencil, float2(cos(Angle), sin(Angle)) * Radius).r;
	Edge = max(Edge, abs(Neighbour - StencilValue) < 0.5 ? 1.0 : 0.0);
}
float Amount = max(Edge * (1.0 - Inside), Inside * Fill);
return lerp(SceneColor.rgb, Color.rgb, Amount);)");

		UMaterialEditingLibrary::ConnectMaterialExpressions(SceneColor, TEXT("Color"), Outline, TEXT("SceneColor"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Stencil, TEXT("Color"), Outline, TEXT("Stencil"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Color, TEXT(""), Outline, TEXT("Color"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Thickness, TEXT(""), Outline, TEXT("Thickness"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Fill, TEXT(""), Outline, TEXT("Fill"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(StencilValue, TEXT(""), Outline, TEXT("StencilValue"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Outline, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::RecompileMaterial(Material);
		FAssetRegistryModule::AssetCreated(Material);
		Save(Material);
		return Material;
	}

	const TCHAR* InteractionCardPath = TEXT("/Game/Vaesen/UI/Interaction/WBP_InteractionCard");

	/**
	 * The card UVaesenInteractionComponent shows: a dark rounded panel with the name (and quantity), a short
	 * description, the key in a key-cap box beside the action, and a slim progress bar along the bottom edge that
	 * fills while the key is held (only for interactions that take time).
	 */
	void CreateInteractionCard()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(InteractionCardPath, UVaesenInteractionCardWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}

		const FBuilder Build{ Blueprint->WidgetTree };
		UObject* FontObject = LoadObject<UObject>(nullptr, *UWidget::GetDefaultFontName());
		const FLinearColor Parchment(0.93f, 0.88f, 0.76f);
		const FLinearColor Muted(0.72f, 0.68f, 0.60f);

		const auto Line = [&Build, FontObject](FName Name, const TCHAR* Sample, int32 Size, FName Typeface, const FLinearColor& Color)
		{
			UTextBlock* Text = Build.Make<UTextBlock>(Name);
			Text->SetText(FText::FromString(Sample));
			Text->SetFont(FSlateFontInfo(FontObject, Size, Typeface));
			Text->SetColorAndOpacity(Color);
			return Text;
		};

		// The card sits at the bottom of its 200x150 area (the component's pivot is its bottom centre).
		UBorder* Panel = Build.Make<UBorder>(TEXT("Panel"));
		Panel->bIsVariable = false;
		Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.03f, 0.025f, 0.9f), 6.0f, FLinearColor(Brass.R, Brass.G, Brass.B, 0.8f), 1.0f));
		Panel->SetPadding(FMargin(0.0f));
		Panel->SetVerticalAlignment(VAlign_Bottom);
		USizeBox* Area = Build.Make<USizeBox>();
		Area->SetWidthOverride(200.0f);
		Area->SetHeightOverride(150.0f);
		UVerticalBox* AreaBox = Build.Make<UVerticalBox>();
		Area->SetContent(AreaBox);
		UVerticalBoxSlot* PanelSlot = AreaBox->AddChildToVerticalBox(Panel);
		PanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PanelSlot->SetVerticalAlignment(VAlign_Bottom);
		Blueprint->WidgetTree->RootWidget = Area;

		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Panel->SetContent(Content);

		// Name, with the quantity on the right.
		UHorizontalBox* Header = Build.Make<UHorizontalBox>();
		UTextBlock* Name = Line(TEXT("NameText"), TEXT("Name"), 13, TEXT("Bold"), Parchment);
		Name->SetAutoWrapText(true);
		UHorizontalBoxSlot* NameSlot = Header->AddChildToHorizontalBox(Name);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		UHorizontalBoxSlot* QuantitySlot = Header->AddChildToHorizontalBox(Line(TEXT("QuantityText"), TEXT("x1"), 11, TEXT("Bold"), BrassBright));
		QuantitySlot->SetVerticalAlignment(VAlign_Center);
		QuantitySlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
		Content->AddChildToVerticalBox(Header)->SetPadding(FMargin(10.0f, 8.0f, 10.0f, 2.0f));

		UTextBlock* Description = Line(TEXT("DescriptionText"), TEXT("Description"), 9, TEXT("Regular"), Muted);
		Description->SetAutoWrapText(true);
		Content->AddChildToVerticalBox(Description)->SetPadding(FMargin(10.0f, 0.0f, 10.0f, 6.0f));

		// The key in a key cap, then the action.
		UHorizontalBox* ActionRow = Build.Make<UHorizontalBox>(TEXT("ActionRow"));
		UBorder* KeyCap = Build.Make<UBorder>();
		KeyCap->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.10f, 0.08f, 0.06f, 1.0f), 3.0f, BrassBright, 1.0f));
		KeyCap->SetPadding(FMargin(6.0f, 1.0f));
		KeyCap->SetHorizontalAlignment(HAlign_Center);
		KeyCap->SetVerticalAlignment(VAlign_Center);
		KeyCap->SetContent(Line(TEXT("KeyText"), TEXT("E"), 10, TEXT("Bold"), BrassBright));
		ActionRow->AddChildToHorizontalBox(KeyCap)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* ActionSlot = ActionRow->AddChildToHorizontalBox(Line(TEXT("ActionText"), TEXT("Interact"), 11, TEXT("Bold"), Parchment));
		ActionSlot->SetVerticalAlignment(VAlign_Center);
		ActionSlot->SetPadding(FMargin(7.0f, 0.0f, 0.0f, 0.0f));
		Content->AddChildToVerticalBox(ActionRow)->SetPadding(FMargin(10.0f, 0.0f, 10.0f, 8.0f));

		// The hold progress: a slim bar along the bottom edge.
		UProgressBar* Progress = Build.Make<UProgressBar>(TEXT("InteractionProgress"));
		Progress->SetFillColorAndOpacity(BrassBright);
		FProgressBarStyle ProgressStyle = Progress->GetWidgetStyle();
		ProgressStyle.BackgroundImage = FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 2.0f);
		ProgressStyle.FillImage = FSlateRoundedBoxBrush(FLinearColor::White, 2.0f);
		Progress->SetWidgetStyle(ProgressStyle);
		USizeBox* ProgressSize = Build.Make<USizeBox>();
		ProgressSize->SetHeightOverride(4.0f);
		ProgressSize->SetContent(Progress);
		Content->AddChildToVerticalBox(ProgressSize)->SetPadding(FMargin(6.0f, 0.0f, 6.0f, 5.0f));

		Finish(Blueprint);
	}

	/** The interact key: IA_Interact (button), E in IMC_Default, set on BP_VaesenPlayerCharacter. Each part only when missing. */
	void SetupInteractInput()
	{
		UInputAction* Interact = LoadObject<UInputAction>(nullptr, TEXT("/Game/Vaesen/Input/IA_Interact.IA_Interact"));
		if (!Interact)
		{
			UPackage* Package = CreatePackage(TEXT("/Game/Vaesen/Input/IA_Interact"));
			Interact = NewObject<UInputAction>(Package, TEXT("IA_Interact"), RF_Public | RF_Standalone);
			Interact->ValueType = EInputActionValueType::Boolean;
			FAssetRegistryModule::AssetCreated(Interact);
			Save(Interact);
		}

		if (UInputMappingContext* Mappings = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default")))
		{
			const bool bMapped = Mappings->GetMappings().ContainsByPredicate([Interact](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Interact;
			});
			if (!bMapped)
			{
				Mappings->MapKey(Interact, EKeys::E);
				Save(Mappings);
			}
		}

		if (UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter"))))
		{
			UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), TEXT("InteractAction"));
			if (Property && !Property->GetObjectPropertyValue_InContainer(Defaults))
			{
				SetDefault(Defaults, TEXT("InteractAction"), Interact);
				Finish(Character);
			}
		}
	}

	/**
	 * Interaction: the PP_InteractionOutline material and the interact key on BP_VaesenPlayerCharacter, the card
	 * (WBP_InteractionCard), and BP_InteractableTest, a cube with a UVaesenInteractionComponent (hold E for a
	 * second; the server logs the interaction) to drop into a level and try it on.
	 */
	void SetupInteraction()
	{
		UMaterial* Material = CreateOutlineMaterial();

		if (UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter"))))
		{
			UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), TEXT("InteractionOutlineMaterial"));
			if (Material && Property && !Property->GetObjectPropertyValue_InContainer(Defaults))
			{
				SetDefault(Defaults, TEXT("InteractionOutlineMaterial"), Material);
				Finish(Character);
			}
		}

		SetupInteractInput();
		CreateInteractionCard();

		if (UBlueprint* Blueprint = CreateActorBlueprint(InteractableTestPath, AStaticMeshActor::StaticClass()))
		{
			AStaticMeshActor* Defaults = CastChecked<AStaticMeshActor>(Blueprint->GeneratedClass->GetDefaultObject());
			if (UStaticMesh* Cube = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cube.Cube"))))
			{
				Defaults->GetStaticMeshComponent()->SetStaticMesh(Cube);
			}
			Defaults->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);

			USCS_Node* Node = Blueprint->SimpleConstructionScript->CreateNode(UVaesenInteractionComponent::StaticClass(), TEXT("Interaction"));
			UVaesenInteractionComponent* Interaction = CastChecked<UVaesenInteractionComponent>(Node->ComponentTemplate);
			Interaction->InteractableName = LOCTEXT("TestName", "Supply crate");
			Interaction->Description = LOCTEXT("TestDescription", "A test interactable. Hold E to open it.");
			Interaction->ActionText = LOCTEXT("TestAction", "Open");
			Interaction->Quantity = 3;
			Interaction->InteractionTime = 1.0f;
			// The card's bottom just above the cube (the cube is 100 cm, centred on the actor).
			Interaction->SetRelativeLocation(FVector(0.0, 0.0, 70.0));
			Blueprint->SimpleConstructionScript->AddNode(Node);
			Finish(Blueprint);
		}
	}

	const TCHAR* TestItemPath = TEXT("/Game/Vaesen/Blueprints/Items/BP_Item_LampOil");
	const TCHAR* PickupPath = TEXT("/Game/Vaesen/Blueprints/Interaction/BP_Pickup");

	/** A test item (BP_Item_LampOil), and BP_Pickup: place it and choose its Item Template (lamp oil, bread...). */
	void SetupPickups()
	{
		UStaticMesh* Cylinder = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));

		if (UBlueprint* Blueprint = CreateActorBlueprint(TestItemPath, UVaesenItem::StaticClass()))
		{
			UVaesenItem* Defaults = CastChecked<UVaesenItem>(Blueprint->GeneratedClass->GetDefaultObject());
			Defaults->ItemName = LOCTEXT("LampOilName", "Lamp oil");
			Defaults->Description = LOCTEXT("LampOilDescription", "A small flask of lamp oil.");
			Defaults->PickupMesh = Cylinder;
			Defaults->bStackable = true;
			Defaults->MaxStackSize = 10;
			Finish(Blueprint);
		}

		// The pickup: one for every item; each placed one picks its item (and so its mesh) in the level.
		if (UBlueprint* Blueprint = CreateActorBlueprint(PickupPath, AVaesenPickup::StaticClass()))
		{
			Finish(Blueprint);
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

	const TCHAR* ItemTooltipPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_ItemTooltip");
	const TCHAR* InventorySlotPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_InventorySlot");
	const TCHAR* InventoryPanelPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_InventoryPanel");
	const TCHAR* InventoryScreenPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_InventoryScreen");
	const TCHAR* LootChestPath = TEXT("/Game/Vaesen/Blueprints/Interaction/BP_LootChest");
	const TCHAR* EquipmentSlotPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_EquipmentSlot");
	const TCHAR* EquipmentPanelPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_EquipmentPanel");
	const TCHAR* DropZonePath = TEXT("/Game/Vaesen/UI/Inventory/WBP_DropZone");
	const TCHAR* QuickSlotPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_QuickSlot");
	const TCHAR* QuickBarPath = TEXT("/Game/Vaesen/UI/Inventory/WBP_QuickBar");

	/** Plain text in the inventory UI's own sizes (the Vaesen text roles are sized for menus). */
	UTextBlock* InventoryText(const FBuilder& Build, FName Name, const TCHAR* Sample, int32 Size, FName Typeface, const FLinearColor& Color)
	{
		static UObject* FontObject = LoadObject<UObject>(nullptr, *UWidget::GetDefaultFontName());
		UTextBlock* Text = Build.Make<UTextBlock>(Name);
		Text->SetText(FText::FromString(Sample));
		Text->SetFont(FSlateFontInfo(FontObject, Size, Typeface));
		Text->SetColorAndOpacity(Color);
		return Text;
	}

	const FLinearColor InventoryParchment(0.93f, 0.88f, 0.76f);
	const FLinearColor InventoryMuted(0.70f, 0.66f, 0.58f);

	/** A dark panel with a brass edge. */
	UBorder* InventoryPanelBorder(const FBuilder& Build, FName Name, float Padding)
	{
		UBorder* Panel = Build.Make<UBorder>(Name);
		Panel->bIsVariable = false;
		Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.04f, 0.03f, 0.94f), 6.0f, Brass, 1.0f));
		Panel->SetPadding(FMargin(Padding));
		return Panel;
	}

	void CreateItemTooltip()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(ItemTooltipPath, UVaesenItemTooltipWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		UBorder* Panel = InventoryPanelBorder(Build, TEXT("Panel"), 10.0f);
		Blueprint->WidgetTree->RootWidget = Panel;
		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetMaxDesiredWidth(260.0f);
		Panel->SetContent(Size);
		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Size->SetContent(Content);

		FBuilder::AddRow(Content, InventoryText(Build, TEXT("NameText"), TEXT("Item"), 14, TEXT("Bold"), InventoryParchment), 2.0f);
		FBuilder::AddRow(Content, InventoryText(Build, TEXT("CategoryText"), TEXT("Category"), 10, TEXT("Italic"), BrassBright), 6.0f);
		UTextBlock* Description = InventoryText(Build, TEXT("DescriptionText"), TEXT("Description"), 11, TEXT("Regular"), FLinearColor(0.85f, 0.82f, 0.76f));
		Description->SetAutoWrapText(true);
		FBuilder::AddRow(Content, Description, 8.0f);
		FBuilder::AddRow(Content, InventoryText(Build, TEXT("StatsText"), TEXT("Weight"), 10, TEXT("Regular"), InventoryMuted), 0.0f);
		Finish(Blueprint);
	}

	void CreateInventorySlot()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(InventorySlotPath, UVaesenInventorySlotWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetWidthOverride(64.0f);
		Size->SetHeightOverride(64.0f);
		Blueprint->WidgetTree->RootWidget = Size;

		// The border's colour is tinted by the slot (normal / hovered), so its own fill is white.
		UBorder* SlotBorder = Build.Make<UBorder>(TEXT("SlotBorder"));
		SlotBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f, FLinearColor(0.35f, 0.29f, 0.20f), 1.0f));
		SlotBorder->SetBrushColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.85f));
		SlotBorder->SetPadding(FMargin(0.0f));
		Size->SetContent(SlotBorder);

		UOverlay* Layers = Build.Make<UOverlay>();
		SlotBorder->SetContent(Layers);

		UImage* Thumbnail = Build.Make<UImage>(TEXT("ThumbnailImage"));
		UOverlaySlot* ThumbnailSlot = Layers->AddChildToOverlay(Thumbnail);
		ThumbnailSlot->SetHorizontalAlignment(HAlign_Fill);
		ThumbnailSlot->SetVerticalAlignment(VAlign_Fill);
		ThumbnailSlot->SetPadding(FMargin(6.0f));

		UTextBlock* Name = InventoryText(Build, TEXT("NameText"), TEXT("Item"), 9, TEXT("Regular"), InventoryParchment);
		Name->SetAutoWrapText(true);
		Name->SetJustification(ETextJustify::Center);
		UOverlaySlot* NameSlot = Layers->AddChildToOverlay(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(3.0f));

		UTextBlock* Quantity = InventoryText(Build, TEXT("QuantityText"), TEXT("2"), 11, TEXT("Bold"), BrassBright);
		Quantity->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Quantity->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		UOverlaySlot* QuantitySlot = Layers->AddChildToOverlay(Quantity);
		QuantitySlot->SetHorizontalAlignment(HAlign_Right);
		QuantitySlot->SetVerticalAlignment(VAlign_Bottom);
		QuantitySlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 2.0f));

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("TooltipClass"), LoadWidgetClass(ItemTooltipPath));
		Finish(Blueprint);
	}

	void CreateInventoryPanel()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(InventoryPanelPath, UVaesenInventoryPanelWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		UBorder* Panel = InventoryPanelBorder(Build, TEXT("Panel"), 14.0f);
		Blueprint->WidgetTree->RootWidget = Panel;
		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Panel->SetContent(Content);

		UHorizontalBox* Header = Build.Make<UHorizontalBox>();
		UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(InventoryText(Build, TEXT("TitleText"), TEXT("Inventory"), 18, TEXT("Bold"), InventoryParchment));
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* CapacitySlot = Header->AddChildToHorizontalBox(InventoryText(Build, TEXT("CapacityText"), TEXT("0 / 24"), 12, TEXT("Regular"), InventoryMuted));
		CapacitySlot->SetVerticalAlignment(VAlign_Center);
		FBuilder::AddRow(Content, Header, 10.0f);

		UUniformGridPanel* Grid = Build.Make<UUniformGridPanel>(TEXT("SlotGrid"));
		Grid->SetSlotPadding(FMargin(2.0f));
		FBuilder::AddRow(Content, Grid, 10.0f);

		UTextBlock* Weight = InventoryText(Build, TEXT("WeightText"), TEXT("0 kg"), 11, TEXT("Regular"), InventoryMuted);
		FBuilder::AddRow(Content, Weight, 0.0f)->SetHorizontalAlignment(HAlign_Right);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("SlotWidgetClass"), LoadWidgetClass(InventorySlotPath));
		Finish(Blueprint);
	}

	void CreateEquipmentSlot()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(EquipmentSlotPath, UVaesenEquipmentSlotWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetWidthOverride(72.0f);
		Size->SetHeightOverride(72.0f);
		Blueprint->WidgetTree->RootWidget = Size;

		UBorder* SlotBorder = Build.Make<UBorder>(TEXT("SlotBorder"));
		SlotBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f, Brass, 1.0f));
		SlotBorder->SetBrushColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.85f));
		SlotBorder->SetPadding(FMargin(0.0f));
		Size->SetContent(SlotBorder);

		UOverlay* Layers = Build.Make<UOverlay>();
		SlotBorder->SetContent(Layers);

		UImage* Thumbnail = Build.Make<UImage>(TEXT("ThumbnailImage"));
		UOverlaySlot* ThumbnailSlot = Layers->AddChildToOverlay(Thumbnail);
		ThumbnailSlot->SetHorizontalAlignment(HAlign_Fill);
		ThumbnailSlot->SetVerticalAlignment(VAlign_Fill);
		ThumbnailSlot->SetPadding(FMargin(8.0f, 14.0f, 8.0f, 4.0f));

		UTextBlock* Label = InventoryText(Build, TEXT("SlotLabelText"), TEXT("Head"), 8, TEXT("Regular"), InventoryMuted);
		UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(Label);
		LabelSlot->SetHorizontalAlignment(HAlign_Left);
		LabelSlot->SetVerticalAlignment(VAlign_Top);
		LabelSlot->SetPadding(FMargin(4.0f, 2.0f, 0.0f, 0.0f));

		UTextBlock* Name = InventoryText(Build, TEXT("NameText"), TEXT("Item"), 9, TEXT("Regular"), InventoryParchment);
		Name->SetAutoWrapText(true);
		Name->SetJustification(ETextJustify::Center);
		UOverlaySlot* NameSlot = Layers->AddChildToOverlay(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(3.0f, 10.0f, 3.0f, 0.0f));

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("TooltipClass"), LoadWidgetClass(ItemTooltipPath));
		Finish(Blueprint);
	}

	void CreateEquipmentPanel()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(EquipmentPanelPath, UVaesenEquipmentPanelWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		// The paper doll: armor slots | the live character in a dark frame | the other slots.
		UBorder* Panel = InventoryPanelBorder(Build, TEXT("Panel"), 16.0f);
		Blueprint->WidgetTree->RootWidget = Panel;
		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Panel->SetContent(Content);

		FBuilder::AddRow(Content, InventoryText(Build, TEXT("TitleText"), TEXT("Equipment"), 18, TEXT("Bold"), InventoryParchment), 6.0f);
		UBorder* Rule = Build.Make<UBorder>();
		Rule->SetBrushColor(FLinearColor(Brass.R, Brass.G, Brass.B, 0.45f));
		Rule->SetPadding(FMargin(0.0f));
		USizeBox* RuleSize = Build.Make<USizeBox>();
		RuleSize->SetHeightOverride(1.0f);
		RuleSize->SetContent(Rule);
		FBuilder::AddRow(Content, RuleSize, 12.0f);

		UHorizontalBox* Doll = Build.Make<UHorizontalBox>();

		UVerticalBox* Left = Build.Make<UVerticalBox>(TEXT("LeftSlots"));
		UHorizontalBoxSlot* LeftSlot = Doll->AddChildToHorizontalBox(Left);
		LeftSlot->SetVerticalAlignment(VAlign_Center);
		LeftSlot->SetPadding(FMargin(0.0f, 0.0f, 14.0f, 0.0f));

		USizeBox* Frame = Build.Make<USizeBox>();
		Frame->SetWidthOverride(300.0f);
		Frame->SetHeightOverride(450.0f);
		UBorder* FrameBorder = Build.Make<UBorder>();
		FrameBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Black, 6.0f, FLinearColor(0.35f, 0.29f, 0.20f), 1.0f));
		FrameBorder->SetPadding(FMargin(1.0f));
		UImage* Picture = Build.Make<UImage>(TEXT("PreviewImage"));
		FrameBorder->SetContent(Picture);
		Frame->SetContent(FrameBorder);
		UVerticalBox* Middle = Build.Make<UVerticalBox>();
		Middle->AddChildToVerticalBox(Frame);
		UTextBlock* TurnHint = InventoryText(Build, NAME_None, TEXT("Drag to turn"), 9, TEXT("Italic"), InventoryMuted);
		UVerticalBoxSlot* TurnHintSlot = Middle->AddChildToVerticalBox(TurnHint);
		TurnHintSlot->SetHorizontalAlignment(HAlign_Center);
		TurnHintSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
		Doll->AddChildToHorizontalBox(Middle)->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Right = Build.Make<UVerticalBox>(TEXT("RightSlots"));
		UHorizontalBoxSlot* RightSlot = Doll->AddChildToHorizontalBox(Right);
		RightSlot->SetVerticalAlignment(VAlign_Center);
		RightSlot->SetPadding(FMargin(14.0f, 0.0f, 0.0f, 0.0f));

		FBuilder::AddRow(Content, Doll, 0.0f);

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("SlotWidgetClass"), LoadWidgetClass(EquipmentSlotPath));
		Finish(Blueprint);
	}

	/** The "Drop" box: a slot-sized box with a label, like The Witcher 3's. */
	void CreateDropZone()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(DropZonePath, UVaesenDropZoneWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetWidthOverride(72.0f);
		Size->SetHeightOverride(72.0f);
		Blueprint->WidgetTree->RootWidget = Size;

		UBorder* Zone = Build.Make<UBorder>(TEXT("ZoneBorder"));
		Zone->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f, FLinearColor(0.45f, 0.22f, 0.12f), 1.0f));
		Zone->SetBrushColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.85f));
		Zone->SetHorizontalAlignment(HAlign_Center);
		Zone->SetVerticalAlignment(VAlign_Center);
		Size->SetContent(Zone);

		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Zone->SetContent(Content);
		UTextBlock* Arrow = InventoryText(Build, NAME_None, TEXT("v"), 18, TEXT("Bold"), BrassBright);
		Content->AddChildToVerticalBox(Arrow)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Label = InventoryText(Build, NAME_None, TEXT("Drop"), 11, TEXT("Bold"), InventoryParchment);
		Content->AddChildToVerticalBox(Label)->SetHorizontalAlignment(HAlign_Center);

		Finish(Blueprint);
	}

	/** One quick slot: key number top left, picture or name, count bottom right. */
	void CreateQuickSlot()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(QuickSlotPath, UVaesenQuickSlotWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetWidthOverride(64.0f);
		Size->SetHeightOverride(64.0f);
		Blueprint->WidgetTree->RootWidget = Size;

		UBorder* SlotBorder = Build.Make<UBorder>(TEXT("SlotBorder"));
		SlotBorder->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 4.0f, Brass, 1.0f));
		SlotBorder->SetBrushColor(FLinearColor(0.03f, 0.025f, 0.02f, 0.85f));
		SlotBorder->SetPadding(FMargin(0.0f));
		Size->SetContent(SlotBorder);

		UOverlay* Layers = Build.Make<UOverlay>();
		SlotBorder->SetContent(Layers);

		UOverlaySlot* ThumbnailSlot = Layers->AddChildToOverlay(Build.Make<UImage>(TEXT("ThumbnailImage")));
		ThumbnailSlot->SetHorizontalAlignment(HAlign_Fill);
		ThumbnailSlot->SetVerticalAlignment(VAlign_Fill);
		ThumbnailSlot->SetPadding(FMargin(8.0f));

		UTextBlock* Name = InventoryText(Build, TEXT("NameText"), TEXT("Item"), 9, TEXT("Regular"), InventoryParchment);
		Name->SetAutoWrapText(true);
		Name->SetJustification(ETextJustify::Center);
		UOverlaySlot* NameSlot = Layers->AddChildToOverlay(Name);
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetPadding(FMargin(3.0f));

		UOverlaySlot* KeySlot = Layers->AddChildToOverlay(InventoryText(Build, TEXT("KeyText"), TEXT("1"), 10, TEXT("Bold"), BrassBright));
		KeySlot->SetHorizontalAlignment(HAlign_Left);
		KeySlot->SetVerticalAlignment(VAlign_Top);
		KeySlot->SetPadding(FMargin(4.0f, 2.0f, 0.0f, 0.0f));

		UTextBlock* Count = InventoryText(Build, TEXT("CountText"), TEXT("3"), 11, TEXT("Bold"), InventoryParchment);
		Count->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Count->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		UOverlaySlot* CountSlot = Layers->AddChildToOverlay(Count);
		CountSlot->SetHorizontalAlignment(HAlign_Right);
		CountSlot->SetVerticalAlignment(VAlign_Bottom);
		CountSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 2.0f));

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("TooltipClass"), LoadWidgetClass(ItemTooltipPath));
		Finish(Blueprint);
	}

	/** The quick slots in a framed row: on the HUD (bottom centre) and in the inventory screen. */
	void CreateQuickBar()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(QuickBarPath, UVaesenQuickBarWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		UBorder* Frame = Build.Make<UBorder>(TEXT("Frame"));
		Frame->bIsVariable = false;
		Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.02f, 0.015f, 0.01f, 0.55f), 6.0f));
		Frame->SetPadding(FMargin(8.0f));
		Frame->SetContent(Build.Make<UHorizontalBox>(TEXT("SlotRow")));
		Blueprint->WidgetTree->RootWidget = Frame;

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("SlotWidgetClass"), LoadWidgetClass(QuickSlotPath));
		Finish(Blueprint);
	}

	/** IA_QuickSlot1..8 on keys 1-8 in IMC_Default, set as the character's QuickSlotActions. Each part only when missing. */
	void SetupQuickSlotInput()
	{
		const FKey Keys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight };
		UInputMappingContext* Mappings = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default"));
		TArray<UInputAction*> Actions;
		bool bMappingsChanged = false;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Keys); ++Index)
		{
			const FString Name = FString::Printf(TEXT("IA_QuickSlot%d"), Index + 1);
			const FString PackagePath = TEXT("/Game/Vaesen/Input/") + Name;
			UInputAction* Action = LoadObject<UInputAction>(nullptr, *(PackagePath + TEXT(".") + Name));
			if (!Action)
			{
				UPackage* Package = CreatePackage(*PackagePath);
				Action = NewObject<UInputAction>(Package, *Name, RF_Public | RF_Standalone);
				Action->ValueType = EInputActionValueType::Boolean;
				FAssetRegistryModule::AssetCreated(Action);
				Save(Action);
			}
			Actions.Add(Action);

			const bool bMapped = Mappings && Mappings->GetMappings().ContainsByPredicate([Action](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == Action;
			});
			if (Mappings && !bMapped)
			{
				Mappings->MapKey(Action, Keys[Index]);
				bMappingsChanged = true;
			}
		}
		if (bMappingsChanged)
		{
			Save(Mappings);
		}

		UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter")));
		UObject* Defaults = Character ? Character->GeneratedClass->GetDefaultObject() : nullptr;
		FArrayProperty* Property = Defaults ? FindFProperty<FArrayProperty>(Defaults->GetClass(), TEXT("QuickSlotActions")) : nullptr;
		if (Property)
		{
			FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Defaults));
			if (Array.Num() == 0)
			{
				const FObjectPropertyBase* Inner = CastFieldChecked<FObjectPropertyBase>(Property->Inner);
				for (UInputAction* Action : Actions)
				{
					const int32 Added = Array.AddValue();
					Inner->SetObjectPropertyValue(Array.GetRawPtr(Added), Action);
				}
				Finish(Character);
			}
		}
	}

	void CreateInventoryScreen()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(InventoryScreenPath, UVaesenInventoryScreenWidget::StaticClass());
		UClass* PanelClass = LoadWidgetClass(InventoryPanelPath);
		UClass* EquipmentPanelClass = LoadWidgetClass(EquipmentPanelPath);
		UClass* DropZoneClass = LoadWidgetClass(DropZonePath);
		UClass* QuickSlotClass = LoadWidgetClass(QuickSlotPath);
		if (!Blueprint || !PanelClass || !EquipmentPanelClass || !DropZoneClass || !QuickSlotClass)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		// Full screen and hit-testable, so dropping outside the panels lands here (on the ground).
		UBorder* Backdrop = Build.Make<UBorder>(TEXT("Backdrop"));
		Backdrop->bIsVariable = false;
		Backdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.62f));
		Backdrop->SetPadding(FMargin(0.0f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		Backdrop->SetVisibility(ESlateVisibility::Visible);
		Blueprint->WidgetTree->RootWidget = Backdrop;

		UVerticalBox* Content = Build.Make<UVerticalBox>();
		Backdrop->SetContent(Content);

		UTextBlock* Heading = InventoryText(Build, NAME_None, TEXT("INVENTORY"), 22, TEXT("Bold"), BrassBright);
		FBuilder::AddRow(Content, Heading, 16.0f)->SetHorizontalAlignment(HAlign_Center);

		UHorizontalBox* Panels = Build.Make<UHorizontalBox>();
		UHorizontalBoxSlot* EquipmentSlot = Panels->AddChildToHorizontalBox(Build.Make<UVaesenEquipmentPanelWidget>(TEXT("EquipmentPanel"), EquipmentPanelClass));
		EquipmentSlot->SetPadding(FMargin(0.0f, 0.0f, 24.0f, 0.0f));
		EquipmentSlot->SetVerticalAlignment(VAlign_Top);
		// The player's inventory with the drop box under it.
		UVerticalBox* PlayerColumn = Build.Make<UVerticalBox>();
		PlayerColumn->AddChildToVerticalBox(Build.Make<UVaesenInventoryPanelWidget>(TEXT("PlayerPanel"), PanelClass));
		UVerticalBoxSlot* DropSlot = PlayerColumn->AddChildToVerticalBox(Build.Make<UVaesenDropZoneWidget>(NAME_None, DropZoneClass));
		DropSlot->SetHorizontalAlignment(HAlign_Right);
		DropSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		Panels->AddChildToHorizontalBox(PlayerColumn)->SetVerticalAlignment(VAlign_Top);
		UHorizontalBoxSlot* ContainerSlot = Panels->AddChildToHorizontalBox(Build.Make<UVaesenInventoryPanelWidget>(TEXT("ContainerPanel"), PanelClass));
		ContainerSlot->SetPadding(FMargin(24.0f, 0.0f, 0.0f, 0.0f));
		ContainerSlot->SetVerticalAlignment(VAlign_Top);
		FBuilder::AddRow(Content, Panels, 14.0f);

		// The quick slots, to drag usable things onto.
		FBuilder::AddRow(Content, InventoryText(Build, NAME_None, TEXT("Quick slots"), 11, TEXT("Bold"), InventoryMuted), 4.0f)->SetHorizontalAlignment(HAlign_Center);
		FBuilder::AddRow(Content, Build.Make<UVaesenQuickBarWidget>(TEXT("QuickBar"), LoadWidgetClass(QuickBarPath)), 14.0f)->SetHorizontalAlignment(HAlign_Center);

		UTextBlock* Hint = InventoryText(Build, TEXT("HintText"),
			TEXT("Drag: move, equip, quick slot   Ctrl+drag: split   Shift+click: transfer   Right-click: use, equip, take off, clear   Drag to Drop or outside: drop   1-8: quick slots   I: close"),
			11, TEXT("Regular"), InventoryMuted);
		FBuilder::AddRow(Content, Hint, 0.0f)->SetHorizontalAlignment(HAlign_Center);

		Finish(Blueprint);
	}

	/**
	 * An input action (button) mapped to Key in IMC_Default and set as PropertyName on the character. Each part only
	 * when missing; OldKeys it used to be mapped to are unmapped.
	 */
	void SetupButtonInput(const TCHAR* Name, FKey Key, const TCHAR* PropertyName, TArray<FKey> OldKeys = {})
	{
		const FString PackagePath = FString(TEXT("/Game/Vaesen/Input/")) + Name;
		UInputAction* Action = LoadObject<UInputAction>(nullptr, *(PackagePath + TEXT(".") + Name));
		if (!Action)
		{
			UPackage* Package = CreatePackage(*PackagePath);
			Action = NewObject<UInputAction>(Package, Name, RF_Public | RF_Standalone);
			Action->ValueType = EInputActionValueType::Boolean;
			FAssetRegistryModule::AssetCreated(Action);
			Save(Action);
		}

		if (UInputMappingContext* Mappings = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default")))
		{
			const auto IsMapped = [Mappings, Action](const FKey& MappedKey)
			{
				return Mappings->GetMappings().ContainsByPredicate([Action, &MappedKey](const FEnhancedActionKeyMapping& Mapping)
				{
					return Mapping.Action == Action && Mapping.Key == MappedKey;
				});
			};
			bool bChanged = false;
			for (const FKey& OldKey : OldKeys)
			{
				if (IsMapped(OldKey))
				{
					Mappings->UnmapKey(Action, OldKey);
					bChanged = true;
				}
			}
			if (!IsMapped(Key))
			{
				Mappings->MapKey(Action, Key);
				bChanged = true;
			}
			if (bChanged)
			{
				Save(Mappings);
			}
		}

		if (UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *(FString(PlayerCharacterPath) + TEXT(".BP_VaesenPlayerCharacter"))))
		{
			UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
			const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), PropertyName);
			if (Property && !Property->GetObjectPropertyValue_InContainer(Defaults))
			{
				SetDefault(Defaults, PropertyName, Action);
				Finish(Character);
			}
		}
	}

	/**
	 * Inventory: the screen and its parts (WBP_ItemTooltip, WBP_InventorySlot, WBP_InventoryPanel,
	 * WBP_InventoryScreen), I to open it, and BP_LootChest (a crate with some lamp oil) to loot.
	 */
	void SetupInventory()
	{
		CreateItemTooltip();
		CreateInventorySlot();
		CreateInventoryPanel();
		CreateEquipmentSlot();
		CreateEquipmentPanel();
		CreateDropZone();
		CreateQuickSlot();
		CreateQuickBar();
		CreateInventoryScreen();
		SetupQuickSlotInput();
		SetupButtonInput(TEXT("IA_Inventory"), EKeys::I, TEXT("InventoryAction"), { EKeys::Tab });

		if (UBlueprint* Blueprint = CreateActorBlueprint(LootChestPath, AVaesenLootContainer::StaticClass()))
		{
			AVaesenLootContainer* Defaults = CastChecked<AVaesenLootContainer>(Blueprint->GeneratedClass->GetDefaultObject());
			if (UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Defaults->GetRootComponent()))
			{
				Mesh->SetStaticMesh(Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cube.Cube"))));
			}
			// A low crate.
			Defaults->GetRootComponent()->SetRelativeScale3D(FVector(1.0, 0.7, 0.6));
			if (UClass* LampOil = LoadBlueprintClass(TestItemPath))
			{
				FVaesenItemStack Stack;
				Stack.ItemClass = LampOil;
				Stack.Quantity = 7;
				Defaults->GetInventory()->StartingItems.Add(Stack);
			}
			Finish(Blueprint);
		}
	}

	const TCHAR* HerbPouchPath = TEXT("/Game/Vaesen/Blueprints/Items/BP_Item_HerbPouch");
	const TCHAR* YarrowPath = TEXT("/Game/Vaesen/Blueprints/Items/Herbs/BP_Item_Yarrow");

	/**
	 * Test items: BP_Item_HerbPouch (4 slots, takes Item.Herb), BP_Item_Yarrow (a herb), clothing and rye bread, each
	 * with its dropped size (place them with BP_Pickup). Also keeps BP_Item_LampOil stackable now that plain items
	 * aren't by default.
	 */
	void SetupPouches()
	{
		const FGameplayTag HerbTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Herb"), false);
		if (!HerbTag.IsValid())
		{
			UE_LOG(LogVaesenCreateWidgets, Warning, TEXT("Gameplay tag Item.Herb is missing (Config/DefaultGameplayTags.ini)."));
		}

		if (UBlueprint* Blueprint = CreateActorBlueprint(HerbPouchPath, UVaesenPouchItem::StaticClass()))
		{
			UVaesenPouchItem* Defaults = CastChecked<UVaesenPouchItem>(Blueprint->GeneratedClass->GetDefaultObject());
			Defaults->ItemName = LOCTEXT("HerbPouchName", "Herb pouch");
			Defaults->Description = LOCTEXT("HerbPouchDescription", "A small leather pouch. Herbs you pick go in here while there's room.");
			Defaults->PickupMesh = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
			Defaults->Weight = 0.2f;
			Defaults->PouchSlots = 4;
			if (HerbTag.IsValid())
			{
				Defaults->AcceptedItemTags.AddTag(HerbTag);
			}
			Finish(Blueprint);
		}

		if (UBlueprint* Blueprint = CreateActorBlueprint(YarrowPath, UVaesenIngredientItem::StaticClass()))
		{
			UVaesenIngredientItem* Defaults = CastChecked<UVaesenIngredientItem>(Blueprint->GeneratedClass->GetDefaultObject());
			Defaults->ItemName = LOCTEXT("YarrowName", "Yarrow");
			Defaults->Description = LOCTEXT("YarrowDescription", "A white-flowered herb, good for wounds.");
			Defaults->PickupMesh = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cone.Cone")));
			Defaults->Weight = 0.05f;
			if (HerbTag.IsValid())
			{
				Defaults->ItemTags.AddTag(HerbTag);
			}
			Finish(Blueprint);
		}

		// Clothing to try the equipment with (no meshes of their own yet, so the body doesn't change).
		struct FTestClothing { const TCHAR* Path; FText Name; FText Description; EVaesenEquipmentSlot Slot; };
		const FTestClothing Clothing[] = {
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_LeatherBoots"),
				LOCTEXT("BootsName", "Leather boots"), LOCTEXT("BootsDescription", "Sturdy boots for wet roads."), EVaesenEquipmentSlot::Feet },
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_WorkGloves"),
				LOCTEXT("GlovesName", "Work gloves"), LOCTEXT("GlovesDescription", "Worn leather gloves."), EVaesenEquipmentSlot::Hands },
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_WoolCloak"),
				LOCTEXT("CloakName", "Wool cloak"), LOCTEXT("CloakDescription", "A heavy grey cloak against the rain."), EVaesenEquipmentSlot::Back },
		};
		UStaticMesh* Cube = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cube.Cube")));
		for (const FTestClothing& Piece : Clothing)
		{
			if (UBlueprint* Blueprint = CreateActorBlueprint(Piece.Path, UVaesenArmorItem::StaticClass()))
			{
				UVaesenArmorItem* Defaults = CastChecked<UVaesenArmorItem>(Blueprint->GeneratedClass->GetDefaultObject());
				Defaults->ItemName = Piece.Name;
				Defaults->Description = Piece.Description;
				Defaults->Slot = Piece.Slot;
				Defaults->PickupMesh = Cube;
				Defaults->Weight = 1.0f;
				Finish(Blueprint);
			}
		}

		// Bread: a consumable for the quick slots.
		const TCHAR* BreadPath = TEXT("/Game/Vaesen/Blueprints/Items/Food/BP_Item_RyeBread");
		if (UBlueprint* Blueprint = CreateActorBlueprint(BreadPath, UVaesenConsumableItem::StaticClass()))
		{
			UVaesenConsumableItem* Defaults = CastChecked<UVaesenConsumableItem>(Blueprint->GeneratedClass->GetDefaultObject());
			Defaults->ItemName = LOCTEXT("BreadName", "Rye bread");
			Defaults->Description = LOCTEXT("BreadDescription", "A dense loaf of dark rye bread.");
			Defaults->ConsumableType = EVaesenConsumableType::Food;
			Defaults->UseActionText = LOCTEXT("BreadUse", "Eat");
			Defaults->PickupMesh = Cast<UStaticMesh>(LoadAsset(TEXT("/Engine/BasicShapes/Cube.Cube")));
			Defaults->PickupScale = FVector(0.25, 0.15, 0.1);
			Defaults->Weight = 0.3f;
			FVaesenItemEffect& Nourish = Defaults->Effects.AddDefaulted_GetRef();
			Nourish.Effect = FGameplayTag::RequestGameplayTag(TEXT("Effect.Nourish"), false);
			Nourish.Magnitude = 1.0f;
			Finish(Blueprint);
		}

		// Test items at a sensible size (basic shapes are a metre across) when placed or dropped.
		const TPair<const TCHAR*, FVector> DropScales[] = {
			{ TestItemPath, FVector(0.2, 0.2, 0.25) },
			{ HerbPouchPath, FVector(0.2) },
			{ YarrowPath, FVector(0.15, 0.15, 0.25) },
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_LeatherBoots"), FVector(0.3, 0.3, 0.15) },
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_WorkGloves"), FVector(0.3, 0.3, 0.15) },
			{ TEXT("/Game/Vaesen/Blueprints/Items/Clothing/BP_Item_WoolCloak"), FVector(0.3, 0.3, 0.15) },
		};
		for (const TPair<const TCHAR*, FVector>& Entry : DropScales)
		{
			const FString ObjectPath = FString(Entry.Key) + TEXT(".") + FPackageName::GetShortName(Entry.Key);
			UBlueprint* ItemBlueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
			UVaesenItem* Defaults = ItemBlueprint ? Cast<UVaesenItem>(ItemBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
			if (Defaults && Defaults->PickupScale.Equals(FVector::OneVector))
			{
				Defaults->PickupScale = Entry.Value;
				Finish(ItemBlueprint);
			}
		}

		// Plain items used to stack by default; lamp oil should keep doing so.
		const FString LampOilObject = FString(TestItemPath) + TEXT(".") + FPackageName::GetShortName(TestItemPath);
		if (UBlueprint* LampOil = LoadObject<UBlueprint>(nullptr, *LampOilObject))
		{
			UVaesenItem* Defaults = CastChecked<UVaesenItem>(LampOil->GeneratedClass->GetDefaultObject());
			if (!Defaults->bStackable)
			{
				Defaults->bStackable = true;
				Defaults->MaxStackSize = 10;
				Finish(LampOil);
			}
		}
	}

	const TCHAR* MinimapMaterialPath = TEXT("/Game/Vaesen/Materials/M_Minimap");
	const TCHAR* MinimapWidgetPath = TEXT("/Game/Vaesen/UI/Minimap/WBP_Minimap");

	/**
	 * The minimap's look (a UI material): the live map cut round, a soft shadow inside the rim, a brass ring with a
	 * lighter band and dark edges, and the player's arrow in the middle pointing up.
	 */
	UMaterial* CreateMinimapMaterial()
	{
		if (AssetExists(MinimapMaterialPath))
		{
			UE_LOG(LogVaesenCreateWidgets, Display, TEXT("%s already exists; leaving it alone."), MinimapMaterialPath);
			return LoadObject<UMaterial>(nullptr, *(FString(MinimapMaterialPath) + TEXT(".M_Minimap")));
		}

		UPackage* Package = CreatePackage(MinimapMaterialPath);
		UMaterial* Material = NewObject<UMaterial>(Package, TEXT("M_Minimap"), RF_Public | RF_Standalone);
		Material->MaterialDomain = MD_UI;
		Material->BlendMode = BLEND_Translucent;

		const auto Add = [Material](TSubclassOf<UMaterialExpression> Class, int32 X, int32 Y)
		{
			return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
		};

		UMaterialExpressionTextureSampleParameter2D* Map = CastChecked<UMaterialExpressionTextureSampleParameter2D>(Add(UMaterialExpressionTextureSampleParameter2D::StaticClass(), -700, -200));
		Map->ParameterName = TEXT("MapTexture");
		Map->Texture = Cast<UTexture2D>(LoadAsset(TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
		UMaterialExpression* Coordinates = Add(UMaterialExpressionTextureCoordinate::StaticClass(), -700, 0);

		UMaterialExpressionVectorParameter* RingColor = CastChecked<UMaterialExpressionVectorParameter>(Add(UMaterialExpressionVectorParameter::StaticClass(), -700, 150));
		RingColor->ParameterName = TEXT("RingColor");
		RingColor->DefaultValue = FLinearColor(0.62f, 0.48f, 0.28f);
		UMaterialExpressionScalarParameter* RingWidth = CastChecked<UMaterialExpressionScalarParameter>(Add(UMaterialExpressionScalarParameter::StaticClass(), -700, 350));
		RingWidth->ParameterName = TEXT("RingWidth");
		RingWidth->DefaultValue = 0.07f;
		UMaterialExpressionVectorParameter* ArrowColor = CastChecked<UMaterialExpressionVectorParameter>(Add(UMaterialExpressionVectorParameter::StaticClass(), -700, 450));
		ArrowColor->ParameterName = TEXT("ArrowColor");
		ArrowColor->DefaultValue = FLinearColor(0.98f, 0.86f, 0.5f);

		UMaterialExpressionCustom* Round = CastChecked<UMaterialExpressionCustom>(Add(UMaterialExpressionCustom::StaticClass(), -300, 0));
		Round->Description = TEXT("Round minimap");
		Round->OutputType = CMOT_Float4;
		Round->Inputs.Reset();
		for (const TCHAR* Name : { TEXT("MapColor"), TEXT("UV"), TEXT("RingColor"), TEXT("RingWidth"), TEXT("ArrowColor") })
		{
			Round->Inputs.AddDefaulted_GetRef().InputName = Name;
		}
		Round->Code = TEXT(R"(float2 P = UV * 2.0 - 1.0;
float R = length(P);
float Soft = 0.012;
float Alpha = 1.0 - smoothstep(1.0 - Soft, 1.0, R);

// The map, darkened a little towards the rim.
float RingIn = 1.0 - RingWidth;
float Ring = smoothstep(RingIn - Soft, RingIn, R);
float Shadow = smoothstep(RingIn - 0.16, RingIn, R) * (1.0 - Ring);
float3 Rgb = MapColor.rgb * (1.0 - 0.55 * Shadow);

// The ring: brass, lighter along its middle, with dark edges.
float Band = saturate(1.0 - abs(R - (RingIn + 1.0) * 0.5) / (RingWidth * 0.5));
float Edges = smoothstep(0.0, Soft * 1.5, abs(R - RingIn)) * smoothstep(0.0, Soft * 1.5, abs(R - (1.0 - Soft)));
float3 RingRgb = RingColor.rgb * (0.55 + 0.65 * Band) * lerp(0.3, 1.0, Edges);
Rgb = lerp(Rgb, RingRgb, Ring);

// The player: an arrow pointing up, with a dark outline.
float2 Up = float2(P.x, -P.y);
float2 Q = Up / 0.075;
float Outer = (Q.y <= 1.25 && Q.y >= -0.8 + 0.5 * abs(Q.x) && abs(Q.x) <= (1.25 - Q.y) * 0.47) ? 1.0 : 0.0;
float Inner = (Q.y <= 1.0 && Q.y >= -0.6 + 0.5 * abs(Q.x) && abs(Q.x) <= (1.0 - Q.y) * 0.44) ? 1.0 : 0.0;
Rgb = lerp(Rgb, float3(0.05, 0.04, 0.03), Outer);
Rgb = lerp(Rgb, ArrowColor.rgb, Inner);

return float4(Rgb, Alpha);)");

		UMaterialEditingLibrary::ConnectMaterialExpressions(Map, TEXT("RGB"), Round, TEXT("MapColor"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Coordinates, TEXT(""), Round, TEXT("UV"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingColor, TEXT(""), Round, TEXT("RingColor"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingWidth, TEXT(""), Round, TEXT("RingWidth"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ArrowColor, TEXT(""), Round, TEXT("ArrowColor"));

		UMaterialExpressionComponentMask* Colour = CastChecked<UMaterialExpressionComponentMask>(Add(UMaterialExpressionComponentMask::StaticClass(), -50, -60));
		Colour->R = Colour->G = Colour->B = 1;
		Colour->A = 0;
		UMaterialExpressionComponentMask* Opacity = CastChecked<UMaterialExpressionComponentMask>(Add(UMaterialExpressionComponentMask::StaticClass(), -50, 60));
		Opacity->R = Opacity->G = Opacity->B = 0;
		Opacity->A = 1;
		UMaterialEditingLibrary::ConnectMaterialExpressions(Round, TEXT(""), Colour, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Round, TEXT(""), Opacity, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialProperty(Colour, TEXT(""), MP_EmissiveColor);
		UMaterialEditingLibrary::ConnectMaterialProperty(Opacity, TEXT(""), MP_Opacity);

		UMaterialEditingLibrary::RecompileMaterial(Material);
		FAssetRegistryModule::AssetCreated(Material);
		Save(Material);
		return Material;
	}

	/** WBP_HUD: the in-game HUD, full screen: the minimap in the top right, the quick bar at the bottom centre. */
	void CreateHUD()
	{
		UWidgetBlueprint* Blueprint = CreateBlueprint(TEXT("/Game/Vaesen/UI/WBP_HUD"), UVaesenHUDWidget::StaticClass());
		UClass* MinimapClass = LoadWidgetClass(MinimapWidgetPath);
		UClass* QuickBarClass = LoadWidgetClass(QuickBarPath);
		if (!Blueprint || !MinimapClass || !QuickBarClass)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		UCanvasPanel* Canvas = Build.Make<UCanvasPanel>(TEXT("Canvas"));
		Blueprint->WidgetTree->RootWidget = Canvas;

		UCanvasPanelSlot* MapSlot = Canvas->AddChildToCanvas(Build.Make<UVaesenMinimapWidget>(TEXT("Minimap"), MinimapClass));
		MapSlot->SetAnchors(FAnchors(1.0f, 0.0f));
		MapSlot->SetAlignment(FVector2D(1.0f, 0.0f));
		MapSlot->SetPosition(FVector2D(-28.0f, 28.0f));
		MapSlot->SetAutoSize(true);

		UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Build.Make<UVaesenQuickBarWidget>(TEXT("QuickBar"), QuickBarClass));
		BarSlot->SetAnchors(FAnchors(0.5f, 1.0f));
		BarSlot->SetAlignment(FVector2D(0.5f, 1.0f));
		BarSlot->SetPosition(FVector2D(0.0f, -24.0f));
		BarSlot->SetAutoSize(true);

		Finish(Blueprint);
	}

	/** WBP_Minimap: the round map, 220 px (the HUD places it). */
	void CreateMinimap()
	{
		UMaterial* Material = CreateMinimapMaterial();

		UWidgetBlueprint* Blueprint = CreateBlueprint(MinimapWidgetPath, UVaesenMinimapWidget::StaticClass());
		if (!Blueprint)
		{
			return;
		}
		const FBuilder Build{ Blueprint->WidgetTree };

		USizeBox* Size = Build.Make<USizeBox>();
		Size->SetWidthOverride(220.0f);
		Size->SetHeightOverride(220.0f);
		Size->SetContent(Build.Make<UImage>(TEXT("MapImage")));
		Blueprint->WidgetTree->RootWidget = Size;

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		SetDefault(Blueprint->GeneratedClass->GetDefaultObject(), TEXT("MapMaterial"), Material);
		Finish(Blueprint);
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
	SetupModularCharacter();
	SetupInteraction();
	SetupPickups();
	SetupInventory();
	SetupPouches();
	CreateMinimap();
	CreateHUD();
	return 0;
}

#undef LOCTEXT_NAMESPACE
