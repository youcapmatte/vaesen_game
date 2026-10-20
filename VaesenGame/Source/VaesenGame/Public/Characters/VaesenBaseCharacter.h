#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "GameplayCueInterface.h"
#include "GameplayTagContainer.h"
#include "VaesenBaseCharacter.generated.h"

class USkeletalMeshComponent;
class UVaesenEquipmentComponent;
class UVaesenAbilitySystemComponent;
class UVaesenAttributeSet;
class UVaesenGameplayAbility;
class USoundBase;
class UAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FVaesenMeleeHitWindow, bool /* bOpen */);
enum class EVaesenEquipmentSlot : uint8;

/**
 * Shared base for every character in the world, player-controlled or AI.
 * Holds what all characters have in common (capsule, movement, body); no camera or input.
 *
 * Modular body, like clothing/armor systems: the character's Mesh is the Torso (with the arms) and drives the
 * animation (it has the animation Blueprint); Head, Hands, Legs and Feet are separate skeletal meshes on the same
 * skeleton that copy its pose (leader pose), so each part can be swapped (armor) or hidden on its own. Each part also
 * has an empty "worn over" layer on top, for armor that doesn't replace the part (a hat over the head); Back is such
 * a layer for cloaks. Assign the parts' meshes in the Blueprint (the SKM_Manny_* parts). What's worn is the Equipment
 * component's business.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenBaseCharacter : public ACharacter, public IAbilitySystemInterface, public IGameplayCueInterface
{
	GENERATED_BODY()

public:
	AVaesenBaseCharacter();

	virtual void PostInitializeComponents() override;

	USkeletalMeshComponent* GetTorsoMesh() const { return GetMesh(); }
	USkeletalMeshComponent* GetHeadMesh() const { return HeadMesh; }
	USkeletalMeshComponent* GetHandsMesh() const { return HandsMesh; }
	USkeletalMeshComponent* GetLegsMesh() const { return LegsMesh; }
	USkeletalMeshComponent* GetFeetMesh() const { return FeetMesh; }
	USkeletalMeshComponent* GetBackMesh() const { return BackMesh; }

	/** The mesh component an armor slot dresses: Chest is the torso, Back the cloak layer; FullBody and non-armor slots have none. */
	USkeletalMeshComponent* GetBodyPartFor(EVaesenEquipmentSlot Slot) const;

	/** The worn-over layer on top of a body part (Head, Chest, Hands, Legs, Feet), or null. */
	USkeletalMeshComponent* GetWornOverLayerFor(EVaesenEquipmentSlot Slot) const;

	/** Every body part, the torso (Mesh) first, then the back, then the worn-over layers. */
	TArray<USkeletalMeshComponent*> GetBodyParts() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UVaesenEquipmentComponent* GetEquipment() const { return Equipment; }

	// --- Combat (Gameplay Ability System) ---

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "Combat")
	UVaesenAbilitySystemComponent* GetVaesenAbilitySystem() const { return AbilitySystem; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	const UVaesenAttributeSet* GetAttributes() const { return Attributes; }

	/** Hit cues (GameplayCue.Hit.*) play the impact table's sound and effect here, and the pain vocal. */
	virtual void HandleGameplayCue(AActor* Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	/** ANS_MeleeHitWindow opened or closed in a montage on this character (every machine; the melee ability listens on the server). */
	FVaesenMeleeHitWindow OnMeleeHitWindow;

	/** Plays the effort (heavy attack, throw) or pain vocal from this character's voice. */
	void PlayVocal(bool bPain);

	/** Faces where it aims (strafes) rather than where it walks: aiming, blocking, drawing a bow, throwing. */
	virtual bool WantsToFaceAim() const;

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;

	/** Rotation follows the view or the movement (see WantsToFaceAim); must match on the owner and the server. */
	virtual void UpdateRotationMode();

	/** Abilities granted when play starts (server). Defaults: fire, reload, cycle, aim, bow, melee, block, throw, holster. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	TArray<TSubclassOf<UVaesenGameplayAbility>> DefaultAbilities;

	/** Speed while blocking, aiming, drawing a bow, throwing or staggered, as a share of normal. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float BlockingSpeedScale = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float AimingSpeedScale = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0", ClampMax = "1"))
	float StaggeredSpeedScale = 0.2f;

	/** Grunt on heavy attacks and throws. Empty = silent. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Voice")
	TObjectPtr<USoundBase> EffortSound;

	/** On being hit. Empty = silent. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Voice")
	TObjectPtr<USoundBase> PainSound;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UVaesenAbilitySystemComponent> AbilitySystem;

	UPROPERTY()
	TObjectPtr<UVaesenAttributeSet> Attributes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> HandsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> LegsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> FeetMesh;

	/** Empty unless a cloak is worn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> BackMesh;

	// Worn-over layers: empty unless armor worn over that part (a hat, a scarf...).

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body|Worn Over")
	TObjectPtr<USkeletalMeshComponent> HeadOverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body|Worn Over")
	TObjectPtr<USkeletalMeshComponent> ChestOverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body|Worn Over")
	TObjectPtr<USkeletalMeshComponent> HandsOverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body|Worn Over")
	TObjectPtr<USkeletalMeshComponent> LegsOverMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body|Worn Over")
	TObjectPtr<USkeletalMeshComponent> FeetOverMesh;

	/** What the character wears and holds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquipmentComponent> Equipment;

private:
	USkeletalMeshComponent* CreateBodyPart(FName Name);

	/** A combat state tag came or went: speed, rotation, stagger. */
	void OnCombatStateChanged(const FGameplayTag Tag, int32 NewCount);

	/** Walk speed from the class defaults, before combat scales it. */
	float BaseWalkSpeed = 0.0f;

	bool bAbilitiesGranted = false;
};
