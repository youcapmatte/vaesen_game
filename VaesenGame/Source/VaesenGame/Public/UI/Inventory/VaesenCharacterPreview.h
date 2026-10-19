#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenCharacterPreview.generated.h"

class AVaesenBaseCharacter;
class UAnimationAsset;
class UPointLightComponent;
class USceneCaptureComponent2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * A live picture of a character for the inventory screen, like the paper doll in The Witcher 3: a copy of the
 * character's body (every part, worn-over layers and attached items) standing in an idle pose in a little studio of
 * its own, filmed onto a render target the UI shows.
 *
 * Spawned only on the viewing player's machine (it doesn't replicate) while the inventory is open, far above the
 * map. Its meshes and lights use lighting channel 2 only, so the level's sun, sky and fog don't touch it, and the
 * camera films nothing else. Make a Blueprint of it to tune the lights, framing or exposure.
 */
UCLASS(Blueprintable)
class VAESENGAME_API AVaesenCharacterPreview : public AActor
{
	GENERATED_BODY()

public:
	AVaesenCharacterPreview();

	/** Makes the preview look like Character now: body part meshes and attached equipment. */
	UFUNCTION(BlueprintCallable, Category = "Preview")
	void CopyAppearance(const AVaesenBaseCharacter* Character);

	/** Turns the character (dragging on the picture). */
	UFUNCTION(BlueprintCallable, Category = "Preview")
	void AddYaw(float Degrees);

	UFUNCTION(BlueprintPure, Category = "Preview")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** Where previews are put: well above any map. */
	static FVector GetStudioLocation() { return FVector(0.0, 0.0, 200000.0); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Played on the body, looping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UAnimationAsset> IdleAnimation;

	/** Picture size; the UI scales it to fit (keep the 2:3 shape). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	FIntPoint RenderTargetSize = FIntPoint(600, 900);

	/** Brighter (positive) or darker (negative) picture, in stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview")
	float ExposureBias = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USceneComponent> Turntable;

	/** The torso: plays the animation; every other part follows it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UPointLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UPointLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Preview")
	TObjectPtr<UPointLightComponent> RimLight;

private:
	UPointLightComponent* MakeLight(FName Name, const FVector& Location, float Intensity, const FLinearColor& Color, bool bShadows);

	/** The other body parts, in the order of AVaesenBaseCharacter::GetBodyParts after the torso. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMeshComponent>> Parts;

	/** Copies of the character's attached items (weapons, tools...). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Attachments;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;
};
