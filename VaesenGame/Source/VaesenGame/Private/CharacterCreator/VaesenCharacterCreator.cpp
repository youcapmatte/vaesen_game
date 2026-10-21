#include "CharacterCreator/VaesenCharacterCreator.h"

#include "Engine/DataTable.h"
#include "Internationalization/Regex.h"
#include "Online/VaesenApiSubsystem.h"

void UVaesenCharacterCreator::Begin(UDataTable* InPresets, FName InSex)
{
	Presets = InPresets;
	Step = EVaesenCreatorStep::Basics;
	Appearance = FVaesenAppearance();
	Appearance.AppearanceVersion = FVaesenAppearance::CurrentVersion;
	Appearance.Sex = InSex;
	Class.Reset();
	FirstName.Reset();
	LastName.Reset();
	ResetForSex();
	bBodyOrFaceEdited = false;
	Changed();
}

EVaesenCreatorFraming UVaesenCharacterCreator::GetFraming() const
{
	return Step == EVaesenCreatorStep::Face || Step == EVaesenCreatorStep::EyesAndHair ? EVaesenCreatorFraming::Face : EVaesenCreatorFraming::Body;
}

bool UVaesenCharacterCreator::CanGoNext() const
{
	switch (Step)
	{
	case EVaesenCreatorStep::Basics:
		return !Appearance.Sex.IsNone() && !Class.IsEmpty();
	case EVaesenCreatorStep::BodyAndSkin:
		return !Appearance.BodyPresetId.IsNone();
	case EVaesenCreatorStep::Face:
		return !Appearance.FacePresetId.IsNone();
	case EVaesenCreatorStep::EyesAndHair:
		return !Appearance.HairStyleId.IsNone() && !Appearance.EyeColorId.IsNone();
	default:
		return false;
	}
}

bool UVaesenCharacterCreator::Next()
{
	if (!CanGoNext())
	{
		return false;
	}
	Step = (EVaesenCreatorStep)((uint8)Step + 1);
	Changed();
	return true;
}

bool UVaesenCharacterCreator::Back()
{
	if (!CanGoBack())
	{
		return false;
	}
	Step = (EVaesenCreatorStep)((uint8)Step - 1);
	Changed();
	return true;
}

EVaesenSexChange UVaesenCharacterCreator::SetSex(FName InSex, bool bConfirmed)
{
	if (InSex == Appearance.Sex)
	{
		return EVaesenSexChange::Unchanged;
	}
	if (UVaesenAppearanceLibrary::PresetsFor(Presets, EVaesenAppearancePresetKind::Face, InSex, true).IsEmpty())
	{
		return EVaesenSexChange::Invalid;
	}
	if (bBodyOrFaceEdited && !bConfirmed)
	{
		return EVaesenSexChange::NeedsConfirmation;
	}
	Appearance.Sex = InSex;
	ResetForSex();
	bBodyOrFaceEdited = false;
	Changed();
	return EVaesenSexChange::Changed;
}

void UVaesenCharacterCreator::SetClass(const FString& InClass)
{
	Class = InClass;
	Changed();
}

TArray<FName> UVaesenCharacterCreator::GetOptions(EVaesenAppearancePresetKind Kind) const
{
	TArray<FName> Out;
	for (const TPair<FName, const FVaesenAppearancePresetRow*>& Preset : UVaesenAppearanceLibrary::PresetsFor(Presets, Kind, Appearance.Sex, true))
	{
		Out.Add(Preset.Key);
	}
	return Out;
}

bool UVaesenCharacterCreator::IsAllowed(EVaesenAppearancePresetKind Kind, FName Id) const
{
	return GetOptions(Kind).Contains(Id);
}

bool UVaesenCharacterCreator::SetPreset(EVaesenAppearancePresetKind Kind, FName Id)
{
	const bool bClearing = Kind == EVaesenAppearancePresetKind::FacialHair && Id.IsNone();
	if (!bClearing && !IsAllowed(Kind, Id))
	{
		return false;
	}
	switch (Kind)
	{
	case EVaesenAppearancePresetKind::Body: Appearance.BodyPresetId = Id; break;
	case EVaesenAppearancePresetKind::Face: Appearance.FacePresetId = Id; break;
	case EVaesenAppearancePresetKind::HairStyle: Appearance.HairStyleId = Id; break;
	case EVaesenAppearancePresetKind::Eyebrows: Appearance.EyebrowStyleId = Id; break;
	case EVaesenAppearancePresetKind::FacialHair: Appearance.FacialHairId = Id; break;
	case EVaesenAppearancePresetKind::EyeColor: Appearance.EyeColorId = Id; break;
	default: return false;
	}
	bBodyOrFaceEdited |= Kind == EVaesenAppearancePresetKind::Body || Kind == EVaesenAppearancePresetKind::Face;
	Changed();
	return true;
}

bool UVaesenCharacterCreator::ApplySwatch(FName SwatchId)
{
	const FVaesenAppearancePresetRow* Row = Presets ? Presets->FindRow<FVaesenAppearancePresetRow>(SwatchId, TEXT("ApplySwatch"), false) : nullptr;
	if (!Row || !Row->bPlayerSelectable)
	{
		return false;
	}
	if (Row->Kind == EVaesenAppearancePresetKind::SkinTone)
	{
		Appearance.SkinTone = Row->SkinTone;
		bBodyOrFaceEdited = true;
	}
	else if (Row->Kind == EVaesenAppearancePresetKind::HairColor)
	{
		Appearance.HairMelanin = Row->HairMelanin;
		Appearance.HairRedness = Row->HairRedness;
		Appearance.HairGrey = Row->HairGrey;
	}
	else
	{
		return false;
	}
	Changed();
	return true;
}

void UVaesenCharacterCreator::SetSkinTone(FVector2f InTone)
{
	Appearance.SkinTone = FVector2f(FMath::Clamp(InTone.X, 0.f, 1.f), FMath::Clamp(InTone.Y, 0.f, 1.f));
	bBodyOrFaceEdited = true;
	Changed();
}

bool UVaesenCharacterCreator::SetSlider(FName Slider, float Value)
{
	if (!IsAllowed(EVaesenAppearancePresetKind::Slider, Slider))
	{
		return false;
	}
	Appearance.SetSlider(Slider, Value);
	bBodyOrFaceEdited = true;
	Changed();
	return true;
}

void UVaesenCharacterCreator::SetAge(float InAge)
{
	Appearance.Age = FMath::Clamp(InAge, 0.f, 1.f);
	bBodyOrFaceEdited = true;
	Changed();
}

void UVaesenCharacterCreator::SetHairGrey(float Grey)
{
	Appearance.HairGrey = FMath::Clamp(Grey, 0.f, 1.f);
	Changed();
}

void UVaesenCharacterCreator::SetHairColor(float Melanin, float Redness, float Dye)
{
	Appearance.HairMelanin = FMath::Clamp(Melanin, 0.f, 1.f);
	Appearance.HairRedness = FMath::Clamp(Redness, 0.f, 1.f);
	Appearance.HairDye = FMath::Clamp(Dye, 0.f, 1.f);
	Changed();
}

void UVaesenCharacterCreator::Randomise(int32 Seed)
{
	FVaesenRandomAppearanceSettings Settings;
	Settings.bPlayerSelectableOnly = true;
	// Small offsets: a natural face from the base face, which the player then adjusts.
	Settings.SliderSpread = 0.3f;
	Settings.MinAge = 0.f;
	Settings.MaxAge = 0.5f;
	for (const FName Slider : GetOptions(EVaesenAppearancePresetKind::Slider))
	{
		if (Slider != TEXT("age"))
		{
			Settings.Sliders.Add(Slider);
		}
	}
	FVaesenAppearance Random = UVaesenAppearanceLibrary::RandomAppearance(Presets, Appearance.Sex, Seed, Settings);
	// The Face step's Randomise keeps the body chosen on the step before.
	Random.BodyPresetId = Appearance.BodyPresetId;
	Appearance = Random;
	bBodyOrFaceEdited = true;
	Changed();
}

void UVaesenCharacterCreator::SetName(const FString& InFirstName, const FString& InLastName)
{
	FirstName = InFirstName.TrimStartAndEnd();
	LastName = InLastName.TrimStartAndEnd();
	Changed();
}

FString UVaesenCharacterCreator::GetCharacterName() const
{
	return FString::Printf(TEXT("%s %s"), *FirstName, *LastName).TrimStartAndEnd();
}

bool UVaesenCharacterCreator::IsValidPersonName(const FString& Name)
{
	if (Name.Len() < 1 || Name.Len() > 32)
	{
		return false;
	}
	static const FRegexPattern Pattern(TEXT("^\\p{L}+(?:[ '\\-]\\p{L}+)*$"));
	FRegexMatcher Matcher(Pattern, Name);
	return Matcher.FindNext();
}

bool UVaesenCharacterCreator::CanCreate() const
{
	return Step == EVaesenCreatorStep::NameAndReview && IsValidPersonName(FirstName) && IsValidPersonName(LastName)
		&& !Class.IsEmpty() && !Appearance.FacePresetId.IsNone() && !Appearance.BodyPresetId.IsNone();
}

void UVaesenCharacterCreator::Create(UVaesenApiSubsystem* Api, const FString& GuildId,
	TFunction<void(bool bSuccess, const FVaesenCharacter& Character, const FString& Error)> OnComplete)
{
	if (!Api || !CanCreate())
	{
		OnComplete(false, FVaesenCharacter(), TEXT("The character isn't complete yet."));
		return;
	}
	Api->CreateCharacter(GuildId, FirstName, LastName, Class, Appearance, MoveTemp(OnComplete));
}

void UVaesenCharacterCreator::ResetForSex()
{
	auto First = [this](EVaesenAppearancePresetKind Kind)
	{
		const TArray<FName> Options = GetOptions(Kind);
		return Options.IsEmpty() ? NAME_None : Options[0];
	};
	Appearance.BodyPresetId = First(EVaesenAppearancePresetKind::Body);
	Appearance.FacePresetId = First(EVaesenAppearancePresetKind::Face);
	// A hairstyle with a groom (bald is a choice, not the default).
	Appearance.HairStyleId = First(EVaesenAppearancePresetKind::HairStyle);
	for (const TPair<FName, const FVaesenAppearancePresetRow*>& Style : UVaesenAppearanceLibrary::PresetsFor(Presets, EVaesenAppearancePresetKind::HairStyle, Appearance.Sex, true))
	{
		if (!Style.Value->Asset.IsNull())
		{
			Appearance.HairStyleId = Style.Key;
			break;
		}
	}
	Appearance.EyebrowStyleId = First(EVaesenAppearancePresetKind::Eyebrows);
	Appearance.FacialHairId = NAME_None;
	if (Appearance.EyeColorId.IsNone())
	{
		Appearance.EyeColorId = First(EVaesenAppearancePresetKind::EyeColor);
	}
	Appearance.FaceSliders.Reset();
	Appearance.Age = 0.f;
}
