#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spike/AnimNode_VaesenFaceJointOffsets.h"
#include "VaesenSpikeFaceDriver.generated.h"

class UAnimationAsset;
class USkeletalMeshComponent;

/** One face and the slider it shows. */
USTRUCT(BlueprintType)
struct FVaesenSpikeFaceSlider
{
	GENERATED_BODY()

	/** A MetaHuman actor; its "Face" component is driven. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	TObjectPtr<AActor> Target;

	/** The morph target to set (empty: none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	FName Morph;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	float Weight = 1.f;

	/** Joint offsets JSON ({"joint": [x, y, z]}, cm at weight 1) for FAnimNode_VaesenFaceJointOffsets (empty: none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	FString JointOffsetsFile;

	/** P0-6: 0 to 1. Raises the face's age-related wrinkle curves to a floor (permanent light wrinkles). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike", meta = (ClampMin = "0", ClampMax = "1"))
	float WrinkleAge = 0.f;
};

/**
 * Phase 0 spike (P0-3, P0-4): shows sculpt sliders on faces while playing. Every frame it sets each target face's
 * slider morph, and registers its joint offsets with FAnimNode_VaesenFaceJointOffsets (scaled by the weight), so a
 * face's animation Blueprint only needs that node after the face rig, nothing wired to it.
 */
UCLASS()
class VAESENGAME_API AVaesenSpikeFaceDriver : public AActor
{
	GENERATED_BODY()

public:
	AVaesenSpikeFaceDriver();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	TArray<FVaesenSpikeFaceSlider> Faces;

	/** Sweeps every weight between -1 and 1 (sine, this many seconds per cycle; 0 = keep the set weights). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	float SweepSeconds = 0.f;

	/**
	 * Played (looping) on every target face from the first frame, all in sync; e.g. the MetaHuman face range-of-motion
	 * animation. Started on the first tick rather than at BeginPlay, because the MetaHuman component sets the face's
	 * animation up at BeginPlay and would replace it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	TObjectPtr<UAnimationAsset> FaceAnimation;

	/** Like FaceAnimation, for every target's "Body" component (e.g. a retargeted mannequin walk, P0-9). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	TObjectPtr<UAnimationAsset> BodyAnimation;

	/**
	 * Keeps the target faces at full detail (LOD 0) and animating even when not rendered, for close-up slider checks.
	 * Off for performance measurements, which need the normal LOD and update behaviour.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spike")
	bool bForceFullDetail = true;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

private:
	TArray<TArray<FVaesenFaceJointOffset>> LoadedOffsets;
	bool bAnimationStarted = false;
	bool bJointsLogged = false;

	static USkeletalMeshComponent* FindFace(AActor* Target);
	static USkeletalMeshComponent* FindPart(AActor* Target, const TCHAR* Name);
};
