#include "UI/Combat/VaesenCombatHUDWidget.h"

#include "Abilities/VaesenGeneralAbilities.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenGameplayTags.h"
#include "Fonts/FontMeasure.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
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
	NoticeText = FText::GetEmpty();
	ThrowArc.Reset();
	bShowLockMarker = false;

	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(GetOwningPlayerPawn());
	APlayerController* Controller = GetOwningPlayer();
	const UVaesenAbilitySystemComponent* ASC = BoundASC.Get();
	if (!Player || !Controller || !ASC)
	{
		return;
	}

	// The lock's marker, on the target's chest.
	if (const AActor* Locked = Player->GetLockTarget())
	{
		bShowLockMarker = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, AVaesenBaseCharacter::GetLockPoint(Locked), LockMarker, false);
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
		// (A bow nocked but held low isn't aimed: the hip spread.)
		const bool bAiming = ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming);
		const float Spread = Actor->GetCurrentSpread(bAiming);
		SpreadPixels = HalfWidth * FMath::Tan(FMath::DegreesToRadians(Spread)) / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));

		const int32 Carried = Ranged->CountAmmoIn(Player->GetInventory());
		// The trigger pulled for nothing a moment ago: say why, and what to do.
		double DryTime;
		EVaesenFireBlock Why;
		if (Actor->GetLastDryFire(DryTime, Why) && Now - DryTime < 2.5)
		{
			if (Why == EVaesenFireBlock::NeedsCycle)
			{
				NoticeText = LOCTEXT("WorkAction", "Work the action: press R");
			}
			else if (Why == EVaesenFireBlock::Empty)
			{
				NoticeText = Carried > 0 ? LOCTEXT("Empty", "Empty: press R to reload")
					: FText::Format(LOCTEXT("NoAmmo", "Out of ammunition: pick up {0}"),
						Ranged->AcceptedAmmo ? Ranged->AcceptedAmmo->GetDefaultObject<UVaesenAmmoItem>()->ItemName : LOCTEXT("Cartridges", "cartridges"));
			}
			else if (Why == EVaesenFireBlock::Broken)
			{
				NoticeText = FText::Format(LOCTEXT("BrokenNotice", "{0} is broken"), Ranged->ItemName);
			}
		}
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
		// A dark outline under the light lines, so it reads on sky and snow as well as in the dark.
		const FLinearColor Outline(0.0f, 0.0f, 0.0f, CrosshairColor.A * 0.6f);
		for (const TPair<FLinearColor, float>& Pass : { TPair<FLinearColor, float>(Outline, 3.5f), TPair<FLinearColor, float>(CrosshairColor, 1.5f) })
		{
			Line(Centre + FVector2D(Gap, 0), Centre + FVector2D(Gap + CrosshairLineLength, 0), Pass.Key, Pass.Value);
			Line(Centre - FVector2D(Gap, 0), Centre - FVector2D(Gap + CrosshairLineLength, 0), Pass.Key, Pass.Value);
			Line(Centre + FVector2D(0, Gap), Centre + FVector2D(0, Gap + CrosshairLineLength), Pass.Key, Pass.Value);
			Line(Centre - FVector2D(0, Gap), Centre - FVector2D(0, Gap + CrosshairLineLength), Pass.Key, Pass.Value);
			Line(Centre - FVector2D(1.0f, 0), Centre + FVector2D(1.0f, 0), Pass.Key, Pass.Value);
		}
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

	if (bShowLockMarker)
	{
		// A small diamond, outlined like the crosshair.
		const float Radius = 9.0f;
		const TArray<FVector2D> Diamond = { LockMarker + FVector2D(0, -Radius), LockMarker + FVector2D(Radius, 0), LockMarker + FVector2D(0, Radius),
			LockMarker + FVector2D(-Radius, 0), LockMarker + FVector2D(0, -Radius) };
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry, Diamond, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), true, 4.0f);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, Geometry, Diamond, ESlateDrawEffect::None, CrosshairColor, true, 2.0f);
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
	if (!NoticeText.IsEmpty())
	{
		const FSlateFontInfo NoticeFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(NoticeText, NoticeFont);
		const FVector2D Position(Centre.X - TextSize.X * 0.5f, Centre.Y + 60.0f);
		// A dark copy just under it, so it reads on any background.
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(Position + FVector2D(2.0, 2.0)))),
			NoticeText, NoticeFont, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(Position))),
			NoticeText, NoticeFont, ESlateDrawEffect::None, WarningColor);
	}
	if (!WarningText.IsEmpty())
	{
		const FVector2D Position(Size.X - 360.0f, Size.Y - 50.0f);
		FSlateDrawElement::MakeText(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(FVector2f(350.0f, 30.0f), FSlateLayoutTransform(FVector2f(Position))),
			WarningText, FCoreStyle::GetDefaultFontStyle("Regular", 14), ESlateDrawEffect::None, WarningColor);
	}
	return LayerId + 2;
}

#undef LOCTEXT_NAMESPACE
