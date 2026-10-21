#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Appearance/VaesenAppearance.h"
#include "VaesenAppearancePresets.generated.h"

class UVaesenFacePresetData;

/** What a preset in DT_AppearancePresets is. */
UENUM(BlueprintType)
enum class EVaesenAppearancePresetKind : uint8
{
	Body,
	Face,
	HairStyle,
	Eyebrows,
	FacialHair,
	EyeColor,
	/** A skin tone swatch (SkinTone). */
	SkinTone,
	/** A natural hair colour swatch (HairMelanin, HairRedness). */
	HairColor,
	/** A sculpt slider (row name = slider name, e.g. "jawWidth"); for either sex. */
	Slider,
};

/**
 * One row of DT_AppearancePresets: a preset the appearance data can name. The row name is the stored ID
 * ("face_f_07"); it's never renamed or reused once shipped, and its assets are frozen. The API's allowed lists are
 * exported from this table.
 */
USTRUCT(BlueprintType)
struct VAESENGAME_API FVaesenAppearancePresetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	EVaesenAppearancePresetKind Kind = EVaesenAppearancePresetKind::Face;

	/** "female", "male", or none for either (eye colours, skin tones, hair colours). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	FName Sex;

	/** Shown in the character creator. Off for NPC-only presets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset")
	bool bPlayerSelectable = true;

	/** How often random appearances (NPCs, Randomise) pick it, relative to the others of its kind; 0 never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset", meta = (ClampMin = "0"))
	float RandomWeight = 1.f;

	/** Face: the built MetaHuman (its Blueprint) and its runtime data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Face")
	TSoftClassPtr<AActor> MetaHumanClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Face")
	TSoftObjectPtr<UVaesenFacePresetData> FacePreset;

	/** Body, hairstyle, eyebrows, facial hair: the mesh or groom. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Asset")
	TSoftObjectPtr<UObject> Asset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Values")
	FVector2f SkinTone = FVector2f(0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Values")
	float HairMelanin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Values")
	float HairRedness = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Values")
	float HairGrey = 0.f;

	/** Eye colour: the iris colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preset|Values")
	FLinearColor IrisColor = FLinearColor::Gray;
};

/** How random a random appearance is. */
USTRUCT(BlueprintType)
struct VAESENGAME_API FVaesenRandomAppearanceSettings
{
	GENERATED_BODY()

	/** The sculpt sliders to vary. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random")
	TArray<FName> Sliders;

	/** Slider values stay within ±this (natural faces vary a little; the full range is for the creator). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0", ClampMax = "1"))
	float SliderSpread = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0", ClampMax = "1"))
	float MinAge = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxAge = 1.f;

	/** How far skin tone and hair colour may wander from their swatch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0", ClampMax = "0.2"))
	float ColorJitter = 0.04f;

	/** Chance a male appearance has facial hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random", meta = (ClampMin = "0", ClampMax = "1"))
	float FacialHairChance = 0.6f;

	/** Only presets a player could pick (the creator's Randomise); off for NPCs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Random")
	bool bPlayerSelectableOnly = false;
};

UCLASS()
class VAESENGAME_API UVaesenAppearanceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * A random, valid appearance of the given sex from the presets: the same seed always gives the same look, so an NPC
	 * seeded by its ID looks the same on every machine and every visit. Kinds with no presets for the sex stay unset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	static FVaesenAppearance RandomAppearance(const UDataTable* Presets, FName Sex, int32 Seed, const FVaesenRandomAppearanceSettings& Settings);

	/**
	 * The table as the API's "Characters:Appearance" configuration (JSON): the player-selectable IDs per sex, the eye
	 * colours and the slider names. Written to the API's appearance-presets.json, so the API accepts exactly what the
	 * game offers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	static FString ToApiOptionsJson(const UDataTable* Presets);

	/** The presets of one kind usable for the sex (rows for either sex included). */
	static TArray<TPair<FName, const FVaesenAppearancePresetRow*>> PresetsFor(const UDataTable* Presets, EVaesenAppearancePresetKind Kind, FName Sex, bool bPlayerSelectableOnly);
};
