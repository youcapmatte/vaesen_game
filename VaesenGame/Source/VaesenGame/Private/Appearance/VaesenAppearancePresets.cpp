#include "Appearance/VaesenAppearancePresets.h"

#include "Appearance/VaesenFacePresetData.h"
#include "Dom/JsonObject.h"
#include "Math/RandomStream.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

TArray<TPair<FName, const FVaesenAppearancePresetRow*>> UVaesenAppearanceLibrary::PresetsFor(
	const UDataTable* Presets, EVaesenAppearancePresetKind Kind, FName Sex, bool bPlayerSelectableOnly)
{
	TArray<TPair<FName, const FVaesenAppearancePresetRow*>> Out;
	if (!Presets)
	{
		return Out;
	}
	for (const TPair<FName, uint8*>& Row : Presets->GetRowMap())
	{
		const FVaesenAppearancePresetRow* Preset = reinterpret_cast<const FVaesenAppearancePresetRow*>(Row.Value);
		if (Preset && Preset->Kind == Kind && (Preset->Sex.IsNone() || Preset->Sex == Sex) && (!bPlayerSelectableOnly || Preset->bPlayerSelectable))
		{
			Out.Emplace(Row.Key, Preset);
		}
	}
	// Row map order isn't guaranteed to be the same everywhere; sort so a seed picks the same preset on every machine.
	Out.Sort([](const TPair<FName, const FVaesenAppearancePresetRow*>& A, const TPair<FName, const FVaesenAppearancePresetRow*>& B)
	{
		return A.Key.LexicalLess(B.Key);
	});
	return Out;
}

namespace VaesenRandomAppearance
{
	const FVaesenAppearancePresetRow* Pick(FRandomStream& Random, const TArray<TPair<FName, const FVaesenAppearancePresetRow*>>& Options, FName& OutId)
	{
		float Total = 0.f;
		for (const TPair<FName, const FVaesenAppearancePresetRow*>& Option : Options)
		{
			Total += FMath::Max(Option.Value->RandomWeight, 0.f);
		}
		// Always draw, even with nothing to pick, so later draws don't depend on which kinds have presets.
		float Roll = Random.FRand() * Total;
		if (Total <= 0.f)
		{
			OutId = NAME_None;
			return nullptr;
		}
		for (const TPair<FName, const FVaesenAppearancePresetRow*>& Option : Options)
		{
			Roll -= FMath::Max(Option.Value->RandomWeight, 0.f);
			if (Roll <= 0.f && Option.Value->RandomWeight > 0.f)
			{
				OutId = Option.Key;
				return Option.Value;
			}
		}
		OutId = Options.Last().Key;
		return Options.Last().Value;
	}

	/** A value from -Spread to Spread, more often near 0 (the sum of two uniforms). */
	float SliderValue(FRandomStream& Random, float Spread)
	{
		return (Random.FRandRange(-1.f, 1.f) + Random.FRandRange(-1.f, 1.f)) * 0.5f * Spread;
	}
}

FVaesenAppearance UVaesenAppearanceLibrary::RandomAppearance(const UDataTable* Presets, FName Sex, int32 Seed, const FVaesenRandomAppearanceSettings& Settings)
{
	using namespace VaesenRandomAppearance;
	FRandomStream Random(Seed);
	const bool bOnly = Settings.bPlayerSelectableOnly;

	FVaesenAppearance Out;
	Out.AppearanceVersion = FVaesenAppearance::CurrentVersion;
	Out.Sex = Sex;

	FName Id;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::Body, Sex, bOnly), Id);
	Out.BodyPresetId = Id;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::Face, Sex, bOnly), Id);
	Out.FacePresetId = Id;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::EyeColor, Sex, bOnly), Id);
	Out.EyeColorId = Id;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::HairStyle, Sex, bOnly), Id);
	Out.HairStyleId = Id;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::Eyebrows, Sex, bOnly), Id);
	Out.EyebrowStyleId = Id;
	const bool bFacialHair = Random.FRand() < Settings.FacialHairChance;
	Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::FacialHair, Sex, bOnly), Id);
	Out.FacialHairId = bFacialHair ? Id : NAME_None;

	const float Jitter = Settings.ColorJitter;
	if (const FVaesenAppearancePresetRow* Tone = Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::SkinTone, Sex, bOnly), Id))
	{
		Out.SkinTone = FVector2f(
			FMath::Clamp(Tone->SkinTone.X + Random.FRandRange(-Jitter, Jitter), 0.f, 1.f),
			FMath::Clamp(Tone->SkinTone.Y + Random.FRandRange(-Jitter, Jitter), 0.f, 1.f));
	}
	if (const FVaesenAppearancePresetRow* Hair = Pick(Random, PresetsFor(Presets, EVaesenAppearancePresetKind::HairColor, Sex, bOnly), Id))
	{
		Out.HairMelanin = FMath::Clamp(Hair->HairMelanin + Random.FRandRange(-Jitter, Jitter), 0.f, 1.f);
		Out.HairRedness = FMath::Clamp(Hair->HairRedness + Random.FRandRange(-Jitter, Jitter), 0.f, 1.f);
		Out.HairGrey = Hair->HairGrey;
	}

	for (const FName Slider : Settings.Sliders)
	{
		Out.SetSlider(Slider, SliderValue(Random, Settings.SliderSpread));
	}
	Out.Age = FMath::Clamp(Random.FRandRange(Settings.MinAge, Settings.MaxAge), 0.f, 1.f);
	return Out;
}

FString UVaesenAppearanceLibrary::ToApiOptionsJson(const UDataTable* Presets)
{
	auto Ids = [Presets](EVaesenAppearancePresetKind Kind, FName Sex)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const TPair<FName, const FVaesenAppearancePresetRow*>& Preset : PresetsFor(Presets, Kind, Sex, /*bPlayerSelectableOnly*/ true))
		{
			Values.Add(MakeShared<FJsonValueString>(Preset.Key.ToString()));
		}
		return Values;
	};

	const TSharedRef<FJsonObject> BySex = MakeShared<FJsonObject>();
	for (const TCHAR* Sex : { TEXT("female"), TEXT("male") })
	{
		const TSharedRef<FJsonObject> Lists = MakeShared<FJsonObject>();
		Lists->SetArrayField(TEXT("Bodies"), Ids(EVaesenAppearancePresetKind::Body, Sex));
		Lists->SetArrayField(TEXT("Faces"), Ids(EVaesenAppearancePresetKind::Face, Sex));
		Lists->SetArrayField(TEXT("HairStyles"), Ids(EVaesenAppearancePresetKind::HairStyle, Sex));
		Lists->SetArrayField(TEXT("EyebrowStyles"), Ids(EVaesenAppearancePresetKind::Eyebrows, Sex));
		Lists->SetArrayField(TEXT("FacialHair"), Ids(EVaesenAppearancePresetKind::FacialHair, Sex));
		BySex->SetObjectField(Sex, Lists);
	}

	const TSharedRef<FJsonObject> Appearance = MakeShared<FJsonObject>();
	Appearance->SetObjectField(TEXT("PresetsBySex"), BySex);
	// Eye colours and sliders are for either sex: rows with no sex (or for a sex, which PresetsFor also returns).
	Appearance->SetArrayField(TEXT("EyeColors"), Ids(EVaesenAppearancePresetKind::EyeColor, NAME_None));
	Appearance->SetArrayField(TEXT("FaceSliders"), Ids(EVaesenAppearancePresetKind::Slider, NAME_None));

	const TSharedRef<FJsonObject> Characters = MakeShared<FJsonObject>();
	Characters->SetObjectField(TEXT("Appearance"), Appearance);
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetObjectField(TEXT("Characters"), Characters);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	return Json;
}