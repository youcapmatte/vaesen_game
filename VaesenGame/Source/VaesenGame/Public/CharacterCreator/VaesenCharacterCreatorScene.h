#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterCreator/VaesenCharacterCreator.h"
#include "VaesenCharacterCreatorScene.generated.h"

class UCameraComponent;
class UDataTable;
class UVaesenAppearanceComponent;
class UVaesenSkinToneTable;

/**
 * The character creator's stage: the preview MetaHuman on a turntable and the camera. Place one in the creator level
 * (with its fixed lighting) and call Open: it becomes the view, starts a UVaesenCharacterCreator, and from then on shows
 * every change: the appearance component re-dresses (or swaps) the preview MetaHuman, and the camera blends between the
 * body and face framings as the step changes. The stepper widgets drive Creator and AddTurntableYaw; nothing else.
 */
UCLASS()
class VAESENGAME_API AVaesenCharacterCreatorScene : public AActor
{
	GENERATED_BODY()

public:
	AVaesenCharacterCreatorScene();

	/** Starts creating a character of the given sex; makes this the player's view. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void Open(APlayerController* Player, FName Sex);

	/** Shows a saved character's look (the body framing, no creator); an unset look shows nobody. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void Preview(const FVaesenAppearance& InAppearance);

	/** Makes this the player's view (the character select screen shows its previews here). */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void BecomeView(APlayerController* Player);

	/** Mouse wheel: moves the camera between the body (0) and face (1) framings; the step's framing sets it too. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void AddZoom(float Delta);

	/** Drag on the preview: turns the turntable. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void AddTurntableYaw(float Degrees);

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	UVaesenCharacterCreator* GetCreator() const { return Creator; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	TObjectPtr<UDataTable> Presets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	TObjectPtr<UVaesenSkinToneTable> SkinToneTable;

	/**
	 * Camera placements relative to the scene actor, for the two framings; zooming moves between them. They sit to the
	 * character's left (+Y) by the same share of the view, so it stays left of centre, beside the creator panel, at any zoom.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator|Camera")
	FTransform BodyFraming = FTransform(FRotator(-2.f, 0.f, 0.f), FVector(-400.f, 75.f, 100.f));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator|Camera")
	FTransform FaceFraming = FTransform(FRotator(-2.f, 0.f, 0.f), FVector(-100.f, 19.f, 158.f));

	/**
	 * The head height (the face rig's root joint, cm above this actor) FaceFraming was set for. The face framing moves
	 * up or down by how much the preview's head differs from it, so tall and short characters stay framed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator|Camera")
	float FaceFramingHeadHeight = 158.7f;

	/** How quickly the camera follows the zoom (higher is faster). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator|Camera", meta = (ClampMin = "0.1"))
	float ZoomSpeed = 6.f;

	/** The camera's vertical field of view: the framings show the same height whatever the window's shape. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator|Camera", meta = (ClampMin = "5", ClampMax = "120"))
	float VerticalFieldOfView = 34.f;

	/** For test levels: Open for the first local player at BeginPlay, with this sex (none: don't). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Creator")
	FName OpenOnBeginPlay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Creator")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Creator")
	TObjectPtr<UVaesenAppearanceComponent> Appearance;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<UVaesenCharacterCreator> Creator;

	/** The camera's zoom between the framings (0 body, 1 face), and where it is heading. */
	float Zoom = 0.f;
	float ZoomTarget = 0.f;
	/** How far the preview's head is above FaceFramingHeadHeight (cm). */
	float HeadHeightOffset = 0.f;
	EVaesenCreatorFraming Framing = EVaesenCreatorFraming::Body;
	float TurntableYaw = 0.f;

	UFUNCTION()
	void HandleChanged();

	void ApplyTurntable();
	void ApplyZoom();
	void MeasureHead(AActor* MetaHuman);
	void UpdateFieldOfView();
};
