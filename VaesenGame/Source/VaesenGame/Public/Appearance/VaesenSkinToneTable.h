#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "VaesenSkinToneTable.generated.h"

/**
 * MetaHuman's skin tone model at runtime: for a tone (U, V), the per-channel bias and gain it gives skin (in linear
 * colour x 255: albedo = bias + gain * detail). Sampled on a grid in the editor (the model itself is editor-only) and
 * looked up bilinearly. Moving a face from its own tone to another: colour * (gain' / gain) + (bias' - bias * gain' /
 * gain) / 255 per channel, which the skin material's VaesenToneScale and VaesenToneOffset apply.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenSkinToneTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Points per side; Bias and Gain hold Resolution x Resolution entries, U fastest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tone", meta = (ClampMin = "2"))
	int32 Resolution = 9;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tone")
	TArray<FVector3f> Bias;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tone")
	TArray<FVector3f> Gain;

	/** False where the tone model has no valid tone (the grid's corners); lookups avoid those points. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skin Tone")
	TArray<bool> Valid;

	/** Bias and gain at (U, V), bilinear between valid grid points. False if the table is empty. */
	bool Lookup(FVector2f UV, FVector3f& OutBias, FVector3f& OutGain) const;

	/** The material parameters that move a face whose skin was made at (FromBias, FromGain) to tone UV. */
	bool ToneParameters(FVector3f FromBias, FVector3f FromGain, FVector2f UV, FLinearColor& OutScale, FLinearColor& OutOffset) const;

	/**
	 * The tone's floor (VaesenToneRatio): the base tone ratio, to bias / from bias. Pixels much darker than the skin
	 * (stubble shadow, brows) would go below zero in a channel with scale and offset, and change hue (blue stubble); the
	 * material keeps them at least at colour x ratio, which only darkens or lightens them.
	 */
	static FLinearColor ToneRatio(FVector3f FromBias, FVector3f ToBias);

	bool ToneRatioFor(FVector3f FromBias, FVector2f UV, FLinearColor& OutRatio) const;
};
