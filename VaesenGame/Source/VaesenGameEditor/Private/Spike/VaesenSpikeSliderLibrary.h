#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "VaesenSpikeSliderLibrary.generated.h"

class UAnimBlueprint;
class UMaterial;
class USkeletalMesh;
class UTexture2D;

/**
 * Phase 0 spike (P0-3): turns a sculpted, auto-rigged variant of a base face into a slider.
 *
 * Given the base face's head DNA and a variant's head DNA (same topology, exported as .dna files), it adds a morph
 * target to the base face's built head mesh and writes the slider's joint offsets to a JSON file:
 *  - joints: each face joint's move, taken as the skin-weighted average move of the skin it drives, in UE bone-parent
 *    space (what FAnimNode_VaesenFaceJointOffsets adds at runtime);
 *  - shape: the variant's vertex positions minus the base's, minus the part the moved joints already cause through skin
 *    weights, so skin isn't moved twice.
 * The DNA head mesh matching the built head (the build may start at a lower DNA LOD) and the DNA's axes are found
 * automatically, by best fit.
 */
UCLASS()
class UVaesenSpikeSliderLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns a report (one line per finding). Starts with "OK" on success, "FAILED" otherwise. */
	/**
	 * Puts a "Vaesen Face Joint Offsets" node in the animation Blueprint's AnimGraph, right before Output Pose (after the
	 * face rig), then compiles and saves it. Does nothing if one is already there. Returns a report starting with OK or
	 * FAILED.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString InsertFaceJointOffsetsNode(UAnimBlueprint* AnimBlueprint);

	/**
	 * Runtime skin tone (P0-5): after every skin colour texture sampled through the given parameter names, inserts
	 * colour * VaesenToneScale + VaesenToneOffset (vector parameters; identity by default, so nothing changes until a
	 * character sets them), then recompiles and saves the material. Done once already: nothing changes. Returns a
	 * report starting with OK or FAILED.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString AddSkinToneParameters(UMaterial* Material, const TArray<FName>& ColorTextureParameters);

	/**
	 * Undoes AddSkinToneParameters: everything that reads the toned colour reads the original source again, and the
	 * inserted nodes are removed, in the material and every material function it uses. Returns a report.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString RemoveSkinToneParameters(UMaterial* Material);

	/**
	 * Adds the age skin layer to M_skin_unified_baked, at the material output (after the tone): the final normal
	 * blends toward a young face's baked normal (VaesenAgeYoung) and then an old face's (VaesenAgeOld), as
	 * VaesenAgeYoungNormal and VaesenAgeOldNormal texture parameters (MetaHuman heads share one UV layout). Both
	 * weights are 0 by default; the appearance component sets them on the face only. Skipped if already there.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString AddSkinAgeLayer(UMaterial* Material, UTexture* YoungNormal, UTexture* OldNormal);

	/**
	 * The age layer's colour: before the skin tone, the base colour blends toward a young and an old skin's baked
	 * colour (VaesenAgeYoungColor / VaesenAgeOldColor, same weights as the normal), so the tone then applies to it like
	 * to the face's own. Needs AddSkinToneParameters first. Skipped if already there.
	 */
	/**
	 * The skin tone's floor: the toned colour is at least colour x VaesenToneRatio (vector parameter, 1 by default), so
	 * pixels far darker than the skin keep their hue. Needs AddSkinToneParameters first. Skipped if already there.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString AddSkinToneFloor(UMaterial* Material);

	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString AddSkinAgeColor(UMaterial* Material, UTexture* YoungColor, UTexture* OldColor);

	/** P0-8: splits a MetaHuman body into the armour system's parts (torso with neck, hands, legs, feet). See the log. */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static bool SplitBodyForArmour(USkeletalMesh* Body, const FString& OutFolder, const FString& Prefix);

	/**
	 * Phase 0 spike (P0-5): copies a skin colour texture to DestPath (a new asset) with its skin tone moved, the way
	 * MetaHuman tints body skin: in linear colour x 255, albedo = bias + gain * detail. So detail = (albedo - SrcBias) /
	 * SrcGain, and the copy gets DstBias + DstGain * detail. Bias and gain per channel (R, G, B), as stored in the
	 * MetaHuman character's skin settings (body_bias, body_gain). 8-bit BGRA sources only. Returns the new texture.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static UTexture2D* RetoneSkinTexture(UTexture2D* Source, const FString& DestPath, FVector SrcBias, FVector SrcGain, FVector DstBias, FVector DstGain);

	UFUNCTION(BlueprintCallable, Category = "Vaesen|Spike")
	static FString AddSliderMorphFromDNA(USkeletalMesh* HeadMesh, const FString& BaseDNAPath, const FString& VariantDNAPath, FName MorphName,
		const FString& JointOffsetsJsonPath, bool bCompensateJoints = true);
};
