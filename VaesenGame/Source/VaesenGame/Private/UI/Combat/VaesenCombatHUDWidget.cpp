#include "UI/Combat/VaesenCombatHUDWidget.h"

#include "Abilities/VaesenGeneralAbilities.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenGameplayTags.h"
#include "Fonts/SlateFontInfo.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Weapons/VaesenRangedWeaponActor.h"

#define LOCTEXT_NAMESPACE "VaesenCombatHUD"

UVaesenCombatHUDWidget::UVaesenCombatHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVaesenCombatHUDWidget::BindToPawn()
{
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwningPlayerPawn());
	UVaesenAbilitySystemComponent* ASC = Player ? Player->GetVaesenAbilitySystem() : nullptr;
	if (ASC == BoundASC.Get())
	{
		return;
	}
	if (UVaesenAbilitySystemComponent* Old = BoundASC.Get())
	{
		Old->OnHitConfirmed.RemoveDynamic(this, &UVaesenCombatHUDWidget::HandleHitConfirmed);
	}
	BoundASC = ASC;
	if (ASC)
	{
		ASC->OnHitConfirmed.AddDynamic(this, &UVaesenCombatHUDWidget::HandleHitConfirmed);
	}
}

void UVaesenCombatHUDWidget::HandleHitConfirmed(EVaesenHitOutcome Outcome)
{
	HitMarkerTime = Now;
	HitMarkerOutcome = Outcome;
	OnHitMarker(Outcome);
}

void UVaesenCombatHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	BindToPawn();
	Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;

	bShowCrosshair = false;
	AmmoText = FText::GetEmpty();
	WarningText = FText::GetEmpty();
	ThrowArc.Reset();

	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwningPlayerPawn());
	APlayerController* Controller = GetOwningPlayer();
	const UVaesenAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Player || !Controller || !ASC)
	{
		return;
	}

	const UVaesenWeaponItem* Weapon = Player->GetEquipment()->GetDrawnWeapon();
	const UVaesenRangedWeaponItem* Ranged = Cast<UVaesenRangedWeaponItem>(Weapon);
	const bool bThrowing = ASC->HasMatchingGameplayTag(VaesenTags::State_Throwing);
	bShowCrosshair = Ranged || bThrowing;

	// The spread cone, as pixels at the screen's centre.
	const float HalfWidth = MyGeometry.GetLocalSize().X * 0.5f;
	const float FieldOfView = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;
	if (const AVaesenRangedWeaponActor* Actor = Ranged ? Cast<AVaesenRangedWeaponActor>(Ranged->GetWeaponActor()) : nullptr)
	{
		const bool bAiming = ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming) || ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow);
		const float Spread = Actor->GetCurrentSpread(bAiming);
		SpreadPixels = HalfWidth * FMath::Tan(FMath::DegreesToRadians(Spread)) / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));

		const int32 Carried = Ranged->CountAmmoIn(Player->GetInventory());
		AmmoText = Ranged->IsBow()
			? FText::Format(LOCTEXT("Arrows", "- / {0}"), FText::AsNumber(Carried))
			: FText::Format(LOCTEXT("Ammo", "{0} / {1}"), FText::AsNumber(Ranged->GetLoadedRounds()), FText::AsNumber(Carried));
	}
	else
	{
		SpreadPixels = 0.0f;
	}

	if (Weapon && Weapon->IsBroken())
	{
		WarningText = FText::Format(LOCTEXT("Broken", "{0} is broken"), Weapon->ItemName);
	}
	else if (Weapon && Weapon->IsNearBreaking())
	{
		WarningText = FText::Format(LOCTEXT("NearBreaking", "{0} is close to breaking"), Weapon->ItemName);
	}

	// The throw's arc, projected onto the screen.
	if (bThrowing)
	{
		const FVaesenThrowable Throwable = UVaesenGA_Throw::FindThrowable(Player);
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		FVector Start, Velocity;
		UVaesenGA_Throw::GetThrowLaunch(Player, Throwable.Item, ViewRotation.Vector(), Start, Velocity);

		FPredictProjectilePathParams Params(5.0f, Start, Velocity, 3.0f, ECC_Visibility, const_cast<AVaesenPlayerCharacter*>(Player));
		Params.SimFrequency = 15.0f;
		FPredictProjectilePathResult Result;
		UGameplayStatics::PredictProjectilePath(this, Params, Result);
		for (const FPredictProjectilePathPointData& Point : Result.PathData)
		{
			FVector2D Screen;
			if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Point.Location, Screen, false))
			{
				ThrowArc.Add(Screen);
			}
		}
	}
}

int32 UVaesenCombatHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FVector2D Centre = Size * 0.5f;
	const FPaintGeometry Geometry = AllottedGeometry.ToPaintGeometry();

	const auto Line = [&](const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry, TArray<FVector2D>{ A, B }, ESlateDrawEffect::None, Color, true, Thickness);
	};

	if (bShowCrosshair)
	{
		const float Gap = FMath::Max(SpreadPixels, CrosshairMinGap);
		Line(Centre + FVector2D(Gap, 0), Centre + FVector2D(Gap + CrosshairLineLength, 0), CrosshairColor, 1.5f);
		Line(Centre - FVector2D(Gap, 0), Centre - FVector2D(Gap + CrosshairLineLength, 0), CrosshairColor, 1.5f);
		Line(Centre + FVector2D(0, Gap), Centre + FVector2D(0, Gap + CrosshairLineLength), CrosshairColor, 1.5f);
		Line(Centre - FVector2D(0, Gap), Centre - FVector2D(0, Gap + CrosshairLineLength), CrosshairColor, 1.5f);
		Line(Centre - FVector2D(0.75f, 0), Centre + FVector2D(0.75f, 0), CrosshairColor, 1.5f);
	}

	// Hit marker: a cross of four short diagonals, fading out.
	const float Age = static_cast<float>(Now - HitMarkerTime);
	if (Age >= 0.0f && Age < HitMarkerSeconds)
	{
		const FLinearColor Base = HitMarkerOutcome == EVaesenHitOutcome::Killed ? KillColor
			: HitMarkerOutcome == EVaesenHitOutcome::Blocked ? BlockedColor
			: HitMarkerOutcome == EVaesenHitOutcome::Parried ? ParriedColor : HitColor;
		FLinearColor Color = Base;
		Color.A *= 1.0f - Age / HitMarkerSeconds;
		const float Inner = 6.0f;
		const float Outer = HitMarkerOutcome == EVaesenHitOutcome::Killed ? 16.0f : 12.0f;
		for (const FVector2D& Corner : { FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(-1, -1) })
		{
			Line(Centre + Corner * Inner, Centre + Corner * Outer, Color, 2.0f);
		}
	}

	if (ThrowArc.Num() > 1)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry, ThrowArc, ESlateDrawEffect::None, CrosshairColor * FLinearColor(1, 1, 1, 0.6f), true, 2.0f);
	}

	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	if (!AmmoText.IsEmpty())
	{
		const FVector2D Position(Size.X - 160.0f, Size.Y - 80.0f);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(150.0f, 30.0f), FSlateLayoutTransform(FVector2f(Position))),
			AmmoText, Font, ESlateDrawEffect::None, CrosshairColor);
	}
	if (!WarningText.IsEmpty())
	{
		const FVector2D Position(Size.X - 360.0f, Size.Y - 50.0f);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(350.0f, 30.0f), FSlateLayoutTransform(FVector2f(Position))),
			WarningText, FCoreStyle::GetDefaultFontStyle("Regular", 14), ESlateDrawEffect::None, WarningColor);
	}
	return LayerId + 1;
}

#undef LOCTEXT_NAMESPACE
