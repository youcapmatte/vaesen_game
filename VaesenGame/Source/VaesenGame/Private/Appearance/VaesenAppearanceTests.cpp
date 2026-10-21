#include "Appearance/VaesenAppearance.h"
#include "Appearance/VaesenFacePresetData.h"
#include "Appearance/VaesenAppearancePresets.h"
#include "CharacterCreator/VaesenCharacterCreator.h"
#include "Engine/DataTable.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VaesenAppearanceTests
{
	TSharedPtr<FJsonObject> Parse(const TCHAR* Json)
	{
		TSharedPtr<FJsonObject> Object;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
		return Object;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAppearanceJsonRoundTripTest, "Vaesen.Appearance.JsonRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenAppearanceJsonRoundTripTest::RunTest(const FString& Parameters)
{
	FVaesenAppearance Appearance;
	Appearance.AppearanceVersion = FVaesenAppearance::CurrentVersion;
	Appearance.Sex = TEXT("female");
	Appearance.BodyPresetId = TEXT("body_f_slim");
	Appearance.SkinTone = FVector2f(0.42f, 0.3f);
	Appearance.FacePresetId = TEXT("face_f_07");
	Appearance.SetSlider(TEXT("jawWidth"), 0.2f);
	Appearance.SetSlider(TEXT("cheekboneHeight"), -0.3f);
	Appearance.Age = 0.35f;
	Appearance.EyeColorId = TEXT("eye_grey_blue");
	Appearance.HairStyleId = TEXT("hair_f_01");
	Appearance.HairMelanin = 0.7f;
	Appearance.HairRedness = 0.3f;
	Appearance.EyebrowStyleId = TEXT("brow_f_03");

	const FVaesenAppearance Loaded = FVaesenAppearance::FromJson(Appearance.ToJson());
	TestTrue(TEXT("Round trip keeps every field"), Loaded == Appearance);
	TestTrue(TEXT("No facial hair stays none"), Loaded.FacialHairId.IsNone());
	TestEqual(TEXT("Slider value"), Loaded.GetSlider(TEXT("cheekboneHeight")), -0.3f);
	TestEqual(TEXT("Missing slider is 0"), Loaded.GetSlider(TEXT("noseWidth")), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAppearanceJsonToleranceTest, "Vaesen.Appearance.JsonTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenAppearanceJsonToleranceTest::RunTest(const FString& Parameters)
{
	// An API record with unknown fields, missing fields and out-of-range numbers.
	const FVaesenAppearance Loaded = FVaesenAppearance::FromJson(VaesenAppearanceTests::Parse(TEXT(
		"{\"appearanceVersion\":1,\"sex\":\"male\",\"facePresetId\":\"face_m_03\",\"futureField\":{\"x\":1},"
		"\"faceSliders\":{\"jawWidth\":3.0,\"noseWidth\":\"oops\"},\"age\":-1,\"facialHairId\":null}")));

	TestTrue(TEXT("Set"), Loaded.IsSet());
	TestTrue(TEXT("Sex"), Loaded.Sex == FName(TEXT("male")));
	TestTrue(TEXT("Face"), Loaded.FacePresetId == FName(TEXT("face_m_03")));
	TestEqual(TEXT("Slider clamped"), Loaded.GetSlider(TEXT("jawWidth")), 1.f);
	TestEqual(TEXT("Non-number slider ignored"), Loaded.FaceSliders.Num(), 1);
	TestEqual(TEXT("Age clamped"), Loaded.Age, 0.f);
	TestTrue(TEXT("Missing skin tone keeps its default"), Loaded.SkinTone == FVector2f(0.5f, 0.5f));
	TestTrue(TEXT("Null facial hair is none"), Loaded.FacialHairId.IsNone());

	const FVaesenAppearance Older = FVaesenAppearance::FromJson(VaesenAppearanceTests::Parse(TEXT("{\"sex\":\"female\"}")));
	TestFalse(TEXT("A record without a version is no appearance"), Older.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFacePresetOffsetsTest, "Vaesen.Appearance.SliderJointOffsets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenFacePresetOffsetsTest::RunTest(const FString& Parameters)
{
	UVaesenFacePresetData* Preset = NewObject<UVaesenFacePresetData>();
	Preset->PositiveOffsets.Add(TEXT("jawWidth"), { { { TEXT("FACIAL_L_Jawline2"), FVector(0, 0.4, 0) } } });
	Preset->NegativeOffsets.Add(TEXT("jawWidth"), { { { TEXT("FACIAL_L_Jawline2"), FVector(0, -0.4, 0) } } });
	Preset->PositiveOffsets.Add(TEXT("noseWidth"), { { { TEXT("FACIAL_L_Jawline2"), FVector(0.1, 0, 0) }, { TEXT("FACIAL_C_Nose"), FVector(0, 0, 0.2) } } });

	const TArray<FVaesenFaceJointOffset> Combined = Preset->CombinedOffsets({ { TEXT("jawWidth"), -0.5f }, { TEXT("noseWidth"), 1.f } });
	const FVaesenFaceJointOffset* Jaw = Combined.FindByPredicate([](const FVaesenFaceJointOffset& O) { return O.Joint == TEXT("FACIAL_L_Jawline2"); });
	const FVaesenFaceJointOffset* Nose = Combined.FindByPredicate([](const FVaesenFaceJointOffset& O) { return O.Joint == TEXT("FACIAL_C_Nose"); });
	TestEqual(TEXT("Two joints"), Combined.Num(), 2);
	TestTrue(TEXT("Jaw: negative side at half strength plus the nose slider's share"), Jaw && Jaw->Offset.Equals(FVector(0.1, -0.2, 0)));
	TestTrue(TEXT("Nose"), Nose && Nose->Offset.Equals(FVector(0, 0, 0.2)));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

namespace VaesenAppearanceTests
{
	UDataTable* MakePresets()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FVaesenAppearancePresetRow::StaticStruct();
		auto Add = [Table](const TCHAR* Id, EVaesenAppearancePresetKind Kind, const TCHAR* Sex, float Weight = 1.f)
		{
			FVaesenAppearancePresetRow Row;
			Row.Kind = Kind;
			Row.Sex = Sex ? FName(Sex) : NAME_None;
			Row.RandomWeight = Weight;
			Row.SkinTone = FVector2f(0.4f, 0.3f);
			Row.HairMelanin = 0.6f;
			Table->AddRow(FName(Id), Row);
		};
		using enum EVaesenAppearancePresetKind;
		for (const TCHAR* Id : { TEXT("face_f_01"), TEXT("face_f_02"), TEXT("face_f_03") }) { Add(Id, Face, TEXT("female")); }
		for (const TCHAR* Id : { TEXT("face_m_01"), TEXT("face_m_02") }) { Add(Id, Face, TEXT("male")); }
		Add(TEXT("face_f_never"), Face, TEXT("female"), 0.f);
		Add(TEXT("body_f_slim"), Body, TEXT("female"));
		Add(TEXT("body_m_heavy"), Body, TEXT("male"));
		Add(TEXT("hair_f_01"), HairStyle, TEXT("female"));
		Add(TEXT("hair_m_01"), HairStyle, TEXT("male"));
		Add(TEXT("brow_f_01"), Eyebrows, TEXT("female"));
		Add(TEXT("brow_m_01"), Eyebrows, TEXT("male"));
		Add(TEXT("beard_m_01"), FacialHair, TEXT("male"));
		Add(TEXT("eye_blue"), EyeColor, nullptr);
		Add(TEXT("eye_brown"), EyeColor, nullptr);
		Add(TEXT("skin_fair"), SkinTone, nullptr);
		Add(TEXT("hair_brown"), HairColor, nullptr);
		Add(TEXT("jawWidth"), Slider, nullptr);
		Add(TEXT("noseWidth"), Slider, nullptr);
		Add(TEXT("age"), Slider, nullptr);
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenRandomAppearanceTest, "Vaesen.Appearance.RandomAppearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenRandomAppearanceTest::RunTest(const FString& Parameters)
{
	UDataTable* Presets = VaesenAppearanceTests::MakePresets();
	FVaesenRandomAppearanceSettings Settings;
	Settings.Sliders = { TEXT("jawWidth"), TEXT("noseWidth") };
	Settings.SliderSpread = 0.4f;
	Settings.MinAge = 0.2f;
	Settings.MaxAge = 0.6f;

	const FVaesenAppearance A = UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("female"), 1234, Settings);
	const FVaesenAppearance B = UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("female"), 1234, Settings);
	TestTrue(TEXT("Same seed, same look"), A == B);

	TSet<FName> FacesSeen;
	bool bAllValid = true;
	for (int32 Seed = 0; Seed < 200; ++Seed)
	{
		const FVaesenAppearance F = UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("female"), Seed, Settings);
		FacesSeen.Add(F.FacePresetId);
		bAllValid &= F.IsSet() && F.Sex == TEXT("female") && F.FacePresetId.ToString().StartsWith(TEXT("face_f_")) && F.FacePresetId != TEXT("face_f_never")
			&& F.BodyPresetId == TEXT("body_f_slim") && F.HairStyleId == TEXT("hair_f_01") && F.FacialHairId.IsNone()
			&& F.Age >= 0.2f && F.Age <= 0.6f && FMath::Abs(F.GetSlider(TEXT("jawWidth"))) <= 0.4f
			&& FMath::Abs(F.SkinTone.X - 0.4f) <= Settings.ColorJitter + 1e-4f;

		const FVaesenAppearance M = UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("male"), Seed, Settings);
		bAllValid &= M.FacePresetId.ToString().StartsWith(TEXT("face_m_")) && M.BodyPresetId == TEXT("body_m_heavy")
			&& (M.FacialHairId.IsNone() || M.FacialHairId == TEXT("beard_m_01"));
	}
	TestTrue(TEXT("Every random appearance is valid for its sex"), bAllValid);
	TestEqual(TEXT("All three weighted female faces come up, the zero-weight one never"), FacesSeen.Num(), 3);
	TestFalse(TEXT("Different seeds differ"), UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("female"), 1, Settings)
		== UVaesenAppearanceLibrary::RandomAppearance(Presets, TEXT("female"), 2, Settings));
	return true;
}

#endif
#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenCharacterCreatorStepsTest, "Vaesen.CharacterCreator.Steps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenCharacterCreatorStepsTest::RunTest(const FString& Parameters)
{
	UVaesenCharacterCreator* Creator = NewObject<UVaesenCharacterCreator>();
	Creator->Begin(VaesenAppearanceTests::MakePresets(), TEXT("female"));

	TestTrue(TEXT("Starts on Basics"), Creator->GetStep() == EVaesenCreatorStep::Basics);
	TestFalse(TEXT("No back from the first step"), Creator->CanGoBack());
	TestFalse(TEXT("Basics needs a class"), Creator->Next());
	Creator->SetClass(TEXT("Occultist"));
	TestTrue(TEXT("Basics -> Body and skin"), Creator->Next() && Creator->GetStep() == EVaesenCreatorStep::BodyAndSkin);
	TestTrue(TEXT("Body framing"), Creator->GetFraming() == EVaesenCreatorFraming::Body);
	TestTrue(TEXT("-> Face"), Creator->Next() && Creator->GetStep() == EVaesenCreatorStep::Face);
	TestTrue(TEXT("Face framing on the Face step"), Creator->GetFraming() == EVaesenCreatorFraming::Face);
	TestTrue(TEXT("-> Eyes and hair"), Creator->Next() && Creator->GetFraming() == EVaesenCreatorFraming::Face);
	TestTrue(TEXT("-> Name and review"), Creator->Next() && Creator->GetStep() == EVaesenCreatorStep::NameAndReview);
	TestTrue(TEXT("Body framing on review"), Creator->GetFraming() == EVaesenCreatorFraming::Body);
	TestFalse(TEXT("No step after the last"), Creator->Next());
	TestTrue(TEXT("Back works"), Creator->Back() && Creator->GetStep() == EVaesenCreatorStep::EyesAndHair);
	Creator->Next();

	TestFalse(TEXT("Can't create without a name"), Creator->CanCreate());
	Creator->SetName(TEXT(" Astrid "), TEXT("Falk"));
	TestEqual(TEXT("Name"), Creator->GetCharacterName(), FString(TEXT("Astrid Falk")));
	TestTrue(TEXT("Complete: can create"), Creator->CanCreate());
	Creator->SetName(TEXT("Bad<Name>"), TEXT("Falk"));
	TestFalse(TEXT("An invalid name blocks Create"), Creator->CanCreate());
	TestTrue(TEXT("Names with an apostrophe or hyphen are fine"), UVaesenCharacterCreator::IsValidPersonName(TEXT("Anna-Lisa")) && UVaesenCharacterCreator::IsValidPersonName(TEXT("O'Neill")));
	TestFalse(TEXT("Digits aren't"), UVaesenCharacterCreator::IsValidPersonName(TEXT("Erik2")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenCharacterCreatorAppearanceTest, "Vaesen.CharacterCreator.Appearance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FVaesenCharacterCreatorAppearanceTest::RunTest(const FString& Parameters)
{
	UVaesenCharacterCreator* Creator = NewObject<UVaesenCharacterCreator>();
	Creator->Begin(VaesenAppearanceTests::MakePresets(), TEXT("female"));
	const FVaesenAppearance& A = Creator->GetAppearance();

	TestTrue(TEXT("Starts with the sex's first presets"), A.FacePresetId == TEXT("face_f_01") && A.BodyPresetId == TEXT("body_f_slim"));
	TestEqual(TEXT("Three selectable female faces (the zero-weight one is still selectable)"), Creator->GetOptions(EVaesenAppearancePresetKind::Face).Num(), 4);
	TestFalse(TEXT("A male face can't be picked for a female character"), Creator->SetPreset(EVaesenAppearancePresetKind::Face, TEXT("face_m_01")));
	TestTrue(TEXT("A female face can"), Creator->SetPreset(EVaesenAppearancePresetKind::Face, TEXT("face_f_02")) && A.FacePresetId == TEXT("face_f_02"));
	TestTrue(TEXT("Slider"), Creator->SetSlider(TEXT("jawWidth"), 2.f) && A.GetSlider(TEXT("jawWidth")) == 1.f);
	TestFalse(TEXT("Unknown slider"), Creator->SetSlider(TEXT("earSize"), 0.5f));

	TestTrue(TEXT("Changing sex after editing the face needs a confirmation"), Creator->SetSex(TEXT("male"), false) == EVaesenSexChange::NeedsConfirmation);
	TestTrue(TEXT("Still female"), A.Sex == TEXT("female"));
	TestTrue(TEXT("Confirmed"), Creator->SetSex(TEXT("male"), true) == EVaesenSexChange::Changed);
	TestTrue(TEXT("Face and body reset for the new sex, sliders cleared"), A.FacePresetId == TEXT("face_m_01") && A.BodyPresetId == TEXT("body_m_heavy") && A.FaceSliders.IsEmpty());
	TestTrue(TEXT("Unchanged sex"), Creator->SetSex(TEXT("male"), false) == EVaesenSexChange::Unchanged);
	TestTrue(TEXT("Facial hair can be picked and cleared"), Creator->SetPreset(EVaesenAppearancePresetKind::FacialHair, TEXT("beard_m_01"))
		&& Creator->SetPreset(EVaesenAppearancePresetKind::FacialHair, NAME_None) && A.FacialHairId.IsNone());

	TestTrue(TEXT("A skin swatch sets the tone"), Creator->ApplySwatch(TEXT("skin_fair")) && A.SkinTone == FVector2f(0.4f, 0.3f));
	TestFalse(TEXT("A face isn't a swatch"), Creator->ApplySwatch(TEXT("face_m_01")));

	Creator->Randomise(42);
	const FVaesenAppearance First = Creator->GetAppearance();
	Creator->Randomise(42);
	TestTrue(TEXT("Randomise: same seed, same result"), Creator->GetAppearance() == First);
	TestTrue(TEXT("Randomise keeps the sex and body"), First.Sex == TEXT("male") && First.BodyPresetId == TEXT("body_m_heavy"));
	TestTrue(TEXT("Randomise stays valid"), First.FacePresetId.ToString().StartsWith(TEXT("face_m_")) && First.Age <= 0.5f
		&& FMath::Abs(First.GetSlider(TEXT("jawWidth"))) <= 0.3f && First.GetSlider(TEXT("age")) == 0.f);
	return true;
}

#endif