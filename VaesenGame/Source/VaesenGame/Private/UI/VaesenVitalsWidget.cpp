#include "UI/VaesenVitalsWidget.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/VaesenAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Core/VaesenSettings.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "VaesenVitals"

void UVaesenVitalsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The character's ability system (players: on their player state) may arrive after the HUD.
	const IAbilitySystemInterface* Owner = Cast<IAbilitySystemInterface>(GetOwningPlayerPawn());
	UAbilitySystemComponent* Current = Owner ? Owner->GetAbilitySystemComponent() : nullptr;
	if (Current != AbilitySystem.Get())
	{
		Bind(Current);
	}

	TickBar(Health, HealthBar, HealthMaterial, HealthColor, InDeltaTime);
	TickBar(Stamina, StaminaBar, StaminaMaterial, StaminaColor, InDeltaTime);

	StatusTimer -= InDeltaTime;
	if (StatusTimer <= 0.0f)
	{
		StatusTimer = 0.25f;
		UpdateStatuses();
	}
}

void UVaesenVitalsWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UVaesenVitalsWidget::Bind(UAbilitySystemComponent* InAbilitySystem)
{
	Unbind();
	AbilitySystem = InAbilitySystem;
	if (!InAbilitySystem)
	{
		return;
	}

	HealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetHealthAttribute()).AddUObject(this, &UVaesenVitalsWidget::OnHealthChanged);
	MaxHealthChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetMaxHealthAttribute()).AddUObject(this, &UVaesenVitalsWidget::OnHealthChanged);
	StaminaChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetStaminaAttribute()).AddUObject(this, &UVaesenVitalsWidget::OnStaminaChanged);
	MaxStaminaChangedHandle = InAbilitySystem->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetMaxStaminaAttribute()).AddUObject(this, &UVaesenVitalsWidget::OnStaminaChanged);

	// Start from where they are, without animating from full.
	ReadAttributes();
	Health.Shown = Health.Trail = Health.Target;
	Stamina.Shown = Stamina.Trail = Stamina.Target;
}

void UVaesenVitalsWidget::Unbind()
{
	if (UAbilitySystemComponent* System = AbilitySystem.Get())
	{
		System->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
		System->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthChangedHandle);
		System->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		System->GetGameplayAttributeValueChangeDelegate(UVaesenAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
	}
	AbilitySystem.Reset();
}

void UVaesenVitalsWidget::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	ReadAttributes();
}

void UVaesenVitalsWidget::OnStaminaChanged(const FOnAttributeChangeData& Data)
{
	ReadAttributes();
}

void UVaesenVitalsWidget::ReadAttributes()
{
	const UAbilitySystemComponent* System = AbilitySystem.Get();
	if (!System || !System->GetSet<UVaesenAttributeSet>())
	{
		return;
	}
	SetTarget(Health, System->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()), System->GetNumericAttribute(UVaesenAttributeSet::GetMaxHealthAttribute()));
	SetTarget(Stamina, System->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()), System->GetNumericAttribute(UVaesenAttributeSet::GetMaxStaminaAttribute()));

	const auto Show = [](UTextBlock* Text, const FBar& Bar)
	{
		if (Text)
		{
			Text->SetText(FText::Format(LOCTEXT("Value", "{0} / {1}"), FText::AsNumber(FMath::RoundToInt(Bar.Current)), FText::AsNumber(FMath::RoundToInt(Bar.Max))));
		}
	};
	Show(HealthText, Health);
	Show(StaminaText, Stamina);
}

void UVaesenVitalsWidget::SetTarget(FBar& Bar, float Current, float Max)
{
	Bar.Current = Current;
	Bar.Max = Max;
	const float NewTarget = Max > 0.0f ? FMath::Clamp(Current / Max, 0.0f, 1.0f) : 0.0f;
	// A loss leaves a chunk behind for a moment.
	if (NewTarget < Bar.Target)
	{
		Bar.TrailHold = TrailDelay;
	}
	Bar.Target = NewTarget;
}

void UVaesenVitalsWidget::TickBar(FBar& Bar, UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& Material, const FLinearColor& Color, float DeltaTime)
{
	if (!Image)
	{
		return;
	}
	if (!Material)
	{
		Material = Image->GetDynamicMaterial();
		if (!Material)
		{
			return;
		}
		Material->SetVectorParameterValue(TEXT("FillColor"), Color);
		Material->SetVectorParameterValue(TEXT("TrailColor"), TrailColor);
	}

	Bar.Shown = FMath::FInterpTo(Bar.Shown, Bar.Target, DeltaTime, BarSpeed);
	if (Bar.Target >= Bar.Trail)
	{
		Bar.Trail = Bar.Shown;
	}
	else if ((Bar.TrailHold -= DeltaTime) <= 0.0f)
	{
		// After the wait, the chunk shrinks at a steady pace.
		Bar.Trail = FMath::Max(Bar.Target, Bar.Trail - TrailShrinkSpeed * DeltaTime);
	}
	Material->SetScalarParameterValue(TEXT("Percent"), Bar.Shown);
	Material->SetScalarParameterValue(TEXT("Trail"), FMath::Max(Bar.Trail, Bar.Shown));
}

void UVaesenVitalsWidget::UpdateStatuses()
{
	const UAbilitySystemComponent* System = AbilitySystem.Get();
	if (!StatusRow || !System)
	{
		return;
	}

	const FGameplayTag Parent = StatusParentTag.IsValid() ? StatusParentTag : FGameplayTag::RequestGameplayTag(TEXT("Status"), false);
	FGameplayTagContainer Owned;
	System->GetOwnedGameplayTags(Owned);
	FGameplayTagContainer Statuses;
	for (const FGameplayTag& Tag : Owned)
	{
		if (Parent.IsValid() && Tag.MatchesTag(Parent) && Tag != Parent)
		{
			Statuses.AddTag(Tag);
		}
	}

	// The badges: rebuilt only when the set of statuses changes.
	if (!Statuses.HasAllExact(ShownStatuses) || !ShownStatuses.HasAllExact(Statuses))
	{
		ShownStatuses = Statuses;
		StatusRow->ClearChildren();
		StatusTimers.Reset();
		for (const FGameplayTag& Tag : Statuses)
		{
			UBorder* Badge = WidgetTree->ConstructWidget<UBorder>();
			Badge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.03f, 0.03f, 0.85f), 4.0f, FLinearColor(0.55f, 0.15f, 0.1f), 1.0f));
			Badge->SetPadding(FMargin(2.0f));
			Badge->SetToolTipText(FText::FromName(Tag.GetTagName()));

			USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
			Size->SetWidthOverride(26.0f);
			Size->SetHeightOverride(26.0f);
			Badge->SetContent(Size);
			UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>();
			Size->SetContent(Layers);

			const TSoftObjectPtr<UTexture2D>* Icon = UVaesenSettings::Get()->StatusIcons.Find(Tag);
			if (UTexture2D* Texture = Icon ? Icon->LoadSynchronous() : nullptr)
			{
				UImage* Picture = WidgetTree->ConstructWidget<UImage>();
				Picture->SetBrushFromTexture(Texture);
				Layers->AddChildToOverlay(Picture);
			}
			else
			{
				// No icon: the status's first letters, e.g. "Po" for Status.Poisoned.
				FString Leaf = Tag.GetTagName().ToString();
				Leaf.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				UTextBlock* Letters = WidgetTree->ConstructWidget<UTextBlock>();
				Letters->SetText(FText::FromString(Leaf.Left(2)));
				FSlateFontInfo Font = Letters->GetFont();
				Font.Size = 10;
				Font.TypefaceFontName = TEXT("Bold");
				Letters->SetFont(Font);
				Letters->SetColorAndOpacity(FLinearColor(0.95f, 0.85f, 0.7f));
				UOverlaySlot* LettersSlot = Layers->AddChildToOverlay(Letters);
				LettersSlot->SetHorizontalAlignment(HAlign_Center);
				LettersSlot->SetVerticalAlignment(VAlign_Center);
			}

			// Time left, bottom right (empty for statuses without a duration).
			UTextBlock* Timer = WidgetTree->ConstructWidget<UTextBlock>();
			FSlateFontInfo TimerFont = Timer->GetFont();
			TimerFont.Size = 7;
			Timer->SetFont(TimerFont);
			Timer->SetShadowOffset(FVector2D(1.0f, 1.0f));
			Timer->SetShadowColorAndOpacity(FLinearColor::Black);
			UOverlaySlot* TimerSlot = Layers->AddChildToOverlay(Timer);
			TimerSlot->SetHorizontalAlignment(HAlign_Right);
			TimerSlot->SetVerticalAlignment(VAlign_Bottom);
			StatusTimers.Add(Tag, Timer);

			UPanelSlot* PanelSlot = StatusRow->AddChild(Badge);
			if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
			{
				BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 0.0f));
			}
		}
	}

	// Seconds left from the longest effect granting each status.
	for (const TPair<FGameplayTag, TWeakObjectPtr<UTextBlock>>& Entry : StatusTimers)
	{
		UTextBlock* Timer = Entry.Value.Get();
		if (!Timer)
		{
			continue;
		}
		const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Entry.Key));
		float Longest = -1.0f;
		for (const float Remaining : System->GetActiveEffectsTimeRemaining(Query))
		{
			Longest = FMath::Max(Longest, Remaining);
		}
		Timer->SetText(Longest > 0.0f ? FText::AsNumber(FMath::CeilToInt(Longest)) : FText::GetEmpty());
	}
}

#undef LOCTEXT_NAMESPACE
