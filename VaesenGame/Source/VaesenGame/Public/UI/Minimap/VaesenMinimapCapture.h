#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VaesenMinimapCapture.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * The minimap's camera: looks straight down over a point (the player) with a flat, map-like (orthographic) view and
 * films the level into a render target a few times a second. Spawned on the viewing player's machine only, by the
 * minimap widget.
 *
 * What it leaves out: fog, sky and atmosphere, foliage and grass (trees placed with the foliage tool or PCG), skeletal
 * meshes (characters, creatures), particles, and any actor tagged MapHidden or component with Hidden In Scene Capture
 * ticked. Buildings, rocks, terrain and roads show.
 */
UCLASS(Blueprintable)
class VAESENGAME_API AVaesenMinimapCapture : public AActor
{
	GENERATED_BODY()

public:
	AVaesenMinimapCapture();

	/** Centres the map on Location, turned to Yaw (the map's up). */
	void Follow(const FVector& Location, float Yaw);

	UFUNCTION(BlueprintPure, Category = "Minimap")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** Actors with this tag never show on the map (e.g. trees placed as single actors, roofs). */
	static const FName HiddenTag;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** How much of the world the map shows, edge to edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "500", Units = "cm"))
	float MapWidth = 6000.0f;

	/** How often the map is re-filmed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "1", ClampMax = "60"))
	float UpdatesPerSecond = 10.0f;

	/** How high above the followed point the camera sits (above roofs and hills). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (Units = "cm"))
	float CameraHeight = 20000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap", meta = (ClampMin = "64", ClampMax = "2048"))
	int32 Resolution = 512;

	/** Brighter (positive) or darker (negative), in stops. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minimap")
	float ExposureBias = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minimap")
	TObjectPtr<USceneCaptureComponent2D> Capture;

private:
	void CaptureNow();

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	FTimerHandle CaptureTimer;
};
