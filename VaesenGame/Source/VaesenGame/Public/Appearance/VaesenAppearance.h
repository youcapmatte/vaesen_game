#pragma once

#include "CoreMinimal.h"
#include "VaesenAppearance.generated.h"

class FJsonObject;

/** One sculpt slider's value (a list rather than a map, so the appearance can replicate). */
USTRUCT(BlueprintType)
struct VAESENGAME_API FVaesenFaceSliderValue
{
	GENERATED_BODY()

	/** The slider's name, e.g. "jawWidth" (see DT_AppearancePresets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName Name;

	/** -1 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (ClampMin = "-1", ClampMax = "1"))
	float Value = 0.f;
};

/**
 * How a character looks, as data: what the character creator makes, the API stores with the character
 * (CharacterAppearance on the C# Character, "appearance" in JSON) and every client rebuilds the character from.
 * Preset IDs are looked up in DT_AppearancePresets; numbers are normalised. Never holds meshes or engine values.
 */
USTRUCT(BlueprintType)
struct VAESENGAME_API FVaesenAppearance
{
	GENERATED_BODY()

	/** The record's version (the API's CharacterAppearance.CurrentVersion); 0 means "no appearance" (an older character). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	int32 AppearanceVersion = 0;

	/** "female" or "male". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName Sex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Body")
	FName BodyPresetId;

	/** MetaHuman skin tone, a point (U, V) in its tone model, each 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Body")
	FVector2f SkinTone = FVector2f(0.5f, 0.5f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Face")
	FName FacePresetId;

	/** Sliders left out are 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Face")
	TArray<FVaesenFaceSliderValue> FaceSliders;

	/** 0 = young adult, 1 = elderly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Face", meta = (ClampMin = "0", ClampMax = "1"))
	float Age = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Face")
	FName EyeColorId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair")
	FName HairStyleId;

	/** Natural colour: melanin and redness, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair", meta = (ClampMin = "0", ClampMax = "1"))
	float HairMelanin = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair", meta = (ClampMin = "0", ClampMax = "1"))
	float HairRedness = 0.f;

	/** Grey: how much of the hair is white (salt and pepper), 0 to 1; 1 is white. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair", meta = (ClampMin = "0", ClampMax = "1"))
	float HairGrey = 0.f;

	/** Artificial dye amount, 0 to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair", meta = (ClampMin = "0", ClampMax = "1"))
	float HairDye = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair")
	FName EyebrowStyleId;

	/** None: no facial hair. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair")
	FName FacialHairId;

	/** The record version this game writes. */
	static constexpr int32 CurrentVersion = 1;

	bool IsSet() const { return AppearanceVersion > 0; }

	/** A slider's value, 0 if not set. */
	float GetSlider(FName Slider) const;
	void SetSlider(FName Slider, float Value);

	/** As the API's JSON ("appearance" object). */
	TSharedRef<FJsonObject> ToJson() const;

	/**
	 * From the API's JSON. Missing fields keep their defaults and unknown ones are ignored, so older and newer records
	 * both load; a record from an older version is migrated to CurrentVersion here.
	 */
	static FVaesenAppearance FromJson(const TSharedPtr<FJsonObject>& Json);

	bool operator==(const FVaesenAppearance& Other) const;
	bool operator!=(const FVaesenAppearance& Other) const { return !(*this == Other); }
};
