#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Appearance/VaesenAppearance.h"
#include "VaesenAppearanceComponent.generated.h"

class UDataTable;
class UMeshComponent;
class USkeletalMeshComponent;
class UVaesenFacePresetData;

/** A MetaHuman the component spawned is ready to be set up (also again once it has settled, as its own setup resets parts). */
DECLARE_MULTICAST_DELEGATE_OneParam(FVaesenMetaHumanReadySignature, AActor* /*MetaHuman*/);

/**
 * Gives a character the look in its FVaesenAppearance. The server sets the appearance (from the API record when the
 * character spawns, or generated for an NPC); it replicates, and every client applies it locally, so every player sees
 * the same face. The character creator's preview uses the same component and the same ApplyAppearance.
 *
 * A dedicated server never applies anything: it holds and replicates the data but loads no meshes, grooms or materials.
 *
 * Applied: the base face (with PresetTable), sculpt sliders (morph target pairs on the face at every LOD, plus joint
 * offsets through the face's joint-offset node), age (the skin only: the skin material's age layer and a wrinkle
 * floor), skin tone, eye colour, hairstyle, eyebrows and facial hair (grooms bound to the face, see
 * UVaesenFacePresetData::GroomBindings) and hair colour.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenAppearanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaesenAppearanceComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Appearance")
	const FVaesenAppearance& GetAppearance() const { return Appearance; }

	/**
	 * Changes the look. On the server it replicates to everyone; on a client with no authority (the creator's preview
	 * actor is local) it applies locally only.
	 */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void SetAppearance(const FVaesenAppearance& InAppearance);

	/** Applies the current appearance to the owner's meshes. Does nothing on a dedicated server. */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void ApplyAppearance();

	/**
	 * The actor whose meshes get the look; none: the owner. Lets the look live on one actor and be shown on another
	 * (the character creator's preview MetaHuman).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<AActor> TargetActor;

	/** The base face's runtime data (slider joint offsets). Set from the face's preset row when PresetTable is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UVaesenFacePresetData> FacePreset;

	/**
	 * DT_AppearancePresets. With it, the component shows the appearance's base face itself: it spawns the face row's
	 * MetaHuman Blueprint (attached to the owner, at MetaHumanOffset) and swaps it when the face changes; the spawned
	 * MetaHuman becomes the TargetActor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<UDataTable> PresetTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FTransform MetaHumanOffset;

	/** What the spawned MetaHuman is attached to (at MetaHumanOffset); none: the owner's root. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<USceneComponent> AttachParent;

	FVaesenMetaHumanReadySignature OnMetaHumanReady;

	/** MetaHuman's skin tone model, for moving the face's and body's skin to the appearance's tone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	TObjectPtr<class UVaesenSkinToneTable> SkinToneTable;

	/** Removes the MetaHuman this component spawned (and forgets the look). */
	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void ClearMetaHuman();

	/** The MetaHuman this component spawned, if any. */
	UFUNCTION(BlueprintPure, Category = "Appearance")
	AActor* GetSpawnedMetaHuman() const { return SpawnedMetaHuman; }

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The owner's face mesh component's name (a MetaHuman's is "Face"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
	FName FaceComponentName = TEXT("Face");

	/** Wrinkle-map curves raised with age (0.6 x age at most), for the permanent light wrinkles of an older face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Age")
	TArray<FName> AgeWrinkleCurves;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Age", meta = (ClampMin = "0", ClampMax = "1"))
	float AgeWrinkleStrength = 0.6f;

	/** How far the skin goes toward the young or old skin at age 0 or 1 (the rest is the face's own detail). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Age", meta = (ClampMin = "0", ClampMax = "1"))
	float AgeSkinStrength = 0.85f;

	/** The colour hair dye moves toward (the hair material's hairDye; white is undyed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Hair")
	FLinearColor HairDyeColor = FLinearColor(0.75f, 0.12f, 0.06f);

private:
	UPROPERTY(ReplicatedUsing = OnRep_Appearance)
	FVaesenAppearance Appearance;

	/** Sliders set on the face last time, so ones removed since are cleared. */
	TArray<FName> AppliedSliders;

	/** The grooms applied to the current MetaHuman (swapping a groom rebuilds it, so only changes are applied). */
	FName AppliedHairStyle;
	FName AppliedEyebrows;
	FName AppliedFacialHair;

	UPROPERTY(Transient)
	TObjectPtr<AActor> SpawnedMetaHuman;
	FName SpawnedFaceId;
	FTimerHandle SettleTimer;

	/** Spawns (or swaps) the MetaHuman for the appearance's face. True if a new one was spawned. */
	bool UpdateMetaHuman();

	UFUNCTION()
	void OnRep_Appearance();

	AActor* GetTarget() const { return TargetActor ? TargetActor.Get() : GetOwner(); }
	USkeletalMeshComponent* FindFace() const;
	void ApplyFace(USkeletalMeshComponent* Face);
	void ApplyHairColor();
	void ApplySkinTone();
	void ApplySkinAge(USkeletalMeshComponent* Face);
	void ApplyEyeColor(USkeletalMeshComponent* Face);
	void ApplyGrooms();
	/** After a new MetaHuman has settled: everything again, grooms included. */
	void ApplyAfterSettle();
	const struct FVaesenAppearancePresetRow* FindPreset(FName Id) const;
};
