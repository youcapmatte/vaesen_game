#include "Appearance/VaesenAppearance.h"

#include "Dom/JsonObject.h"

namespace VaesenAppearanceJson
{
	FName ReadName(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field)
	{
		FString Value;
		return Json->TryGetStringField(Field, Value) && !Value.IsEmpty() ? FName(*Value) : NAME_None;
	}

	float ReadFloat(const TSharedPtr<FJsonObject>& Json, const TCHAR* Field, float Default)
	{
		double Value = Default;
		return Json.IsValid() && Json->TryGetNumberField(Field, Value) ? (float)Value : Default;
	}

	void WriteName(const TSharedRef<FJsonObject>& Json, const TCHAR* Field, FName Value)
	{
		if (Value.IsNone())
		{
			Json->SetField(Field, MakeShared<FJsonValueNull>());
		}
		else
		{
			Json->SetStringField(Field, Value.ToString());
		}
	}
}

float FVaesenAppearance::GetSlider(FName Slider) const
{
	const FVaesenFaceSliderValue* Found = FaceSliders.FindByPredicate([Slider](const FVaesenFaceSliderValue& S) { return S.Name == Slider; });
	return Found ? Found->Value : 0.f;
}

void FVaesenAppearance::SetSlider(FName Slider, float Value)
{
	Value = FMath::Clamp(Value, -1.f, 1.f);
	if (FVaesenFaceSliderValue* Found = FaceSliders.FindByPredicate([Slider](const FVaesenFaceSliderValue& S) { return S.Name == Slider; }))
	{
		Found->Value = Value;
	}
	else
	{
		FaceSliders.Add({ Slider, Value });
	}
}

TSharedRef<FJsonObject> FVaesenAppearance::ToJson() const
{
	using namespace VaesenAppearanceJson;
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("appearanceVersion"), CurrentVersion);
	WriteName(Json, TEXT("sex"), Sex);
	WriteName(Json, TEXT("bodyPresetId"), BodyPresetId);

	const TSharedRef<FJsonObject> Tone = MakeShared<FJsonObject>();
	Tone->SetNumberField(TEXT("u"), SkinTone.X);
	Tone->SetNumberField(TEXT("v"), SkinTone.Y);
	Json->SetObjectField(TEXT("skinTone"), Tone);

	WriteName(Json, TEXT("facePresetId"), FacePresetId);
	const TSharedRef<FJsonObject> Sliders = MakeShared<FJsonObject>();
	for (const FVaesenFaceSliderValue& Slider : FaceSliders)
	{
		Sliders->SetNumberField(Slider.Name.ToString(), Slider.Value);
	}
	Json->SetObjectField(TEXT("faceSliders"), Sliders);
	Json->SetNumberField(TEXT("age"), Age);
	WriteName(Json, TEXT("eyeColorId"), EyeColorId);

	WriteName(Json, TEXT("hairStyleId"), HairStyleId);
	const TSharedRef<FJsonObject> Hair = MakeShared<FJsonObject>();
	Hair->SetNumberField(TEXT("melanin"), HairMelanin);
	Hair->SetNumberField(TEXT("redness"), HairRedness);
	Hair->SetNumberField(TEXT("grey"), HairGrey);
	Hair->SetNumberField(TEXT("dye"), HairDye);
	Json->SetObjectField(TEXT("hairColor"), Hair);
	WriteName(Json, TEXT("eyebrowStyleId"), EyebrowStyleId);
	WriteName(Json, TEXT("facialHairId"), FacialHairId);
	return Json;
}

FVaesenAppearance FVaesenAppearance::FromJson(const TSharedPtr<FJsonObject>& Json)
{
	using namespace VaesenAppearanceJson;
	FVaesenAppearance Out;
	if (!Json.IsValid())
	{
		return Out;
	}

	double Version = 0;
	Json->TryGetNumberField(TEXT("appearanceVersion"), Version);
	// Version 1 is the first; later versions migrate older records here (rename sliders, map retired presets...).
	Out.AppearanceVersion = Version >= 1 ? CurrentVersion : 0;

	Out.Sex = ReadName(Json, TEXT("sex"));
	Out.BodyPresetId = ReadName(Json, TEXT("bodyPresetId"));
	const TSharedPtr<FJsonObject>* Tone = nullptr;
	if (Json->TryGetObjectField(TEXT("skinTone"), Tone))
	{
		Out.SkinTone = FVector2f(ReadFloat(*Tone, TEXT("u"), 0.5f), ReadFloat(*Tone, TEXT("v"), 0.5f));
	}

	Out.FacePresetId = ReadName(Json, TEXT("facePresetId"));
	const TSharedPtr<FJsonObject>* Sliders = nullptr;
	if (Json->TryGetObjectField(TEXT("faceSliders"), Sliders))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Sliders)->Values)
		{
			double Value = 0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Value))
			{
				Out.SetSlider(FName(*Pair.Key), (float)Value);
			}
		}
	}
	Out.Age = FMath::Clamp(ReadFloat(Json, TEXT("age"), 0.f), 0.f, 1.f);
	Out.EyeColorId = ReadName(Json, TEXT("eyeColorId"));

	Out.HairStyleId = ReadName(Json, TEXT("hairStyleId"));
	const TSharedPtr<FJsonObject>* Hair = nullptr;
	if (Json->TryGetObjectField(TEXT("hairColor"), Hair))
	{
		Out.HairMelanin = FMath::Clamp(ReadFloat(*Hair, TEXT("melanin"), 0.5f), 0.f, 1.f);
		Out.HairRedness = FMath::Clamp(ReadFloat(*Hair, TEXT("redness"), 0.f), 0.f, 1.f);
		Out.HairGrey = FMath::Clamp(ReadFloat(*Hair, TEXT("grey"), 0.f), 0.f, 1.f);
		Out.HairDye = FMath::Clamp(ReadFloat(*Hair, TEXT("dye"), 0.f), 0.f, 1.f);
	}
	Out.EyebrowStyleId = ReadName(Json, TEXT("eyebrowStyleId"));
	Out.FacialHairId = ReadName(Json, TEXT("facialHairId"));
	return Out;
}

bool FVaesenAppearance::operator==(const FVaesenAppearance& Other) const
{
	if (FaceSliders.Num() != Other.FaceSliders.Num())
	{
		return false;
	}
	for (const FVaesenFaceSliderValue& Slider : FaceSliders)
	{
		if (Other.GetSlider(Slider.Name) != Slider.Value)
		{
			return false;
		}
	}
	return AppearanceVersion == Other.AppearanceVersion && Sex == Other.Sex && BodyPresetId == Other.BodyPresetId && SkinTone == Other.SkinTone
		&& FacePresetId == Other.FacePresetId && Age == Other.Age && EyeColorId == Other.EyeColorId && HairStyleId == Other.HairStyleId
		&& HairMelanin == Other.HairMelanin && HairRedness == Other.HairRedness && HairGrey == Other.HairGrey && HairDye == Other.HairDye
		&& EyebrowStyleId == Other.EyebrowStyleId && FacialHairId == Other.FacialHairId;
}
