#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VaesenBaseCharacter.generated.h"

class USkeletalMeshComponent;
class UVaesenEquipmentComponent;
enum class EVaesenEquipmentSlot : uint8;

/**
 * Shared base for every character in the world, player-controlled or AI.
 * Holds what all characters have in common (capsule, movement, body); no camera or input.
 *
 * Modular body, like clothing/armor systems: the character's Mesh is the Torso (with the arms) and drives the
 * animation (it has the animation Blueprint); Head, Hands, Legs and Feet are separate skeletal meshes on the same
 * skeleton that copy its pose (leader pose), so each part can be swapped (armor) or hidden on its own. Back has no
 * mesh of its own: it shows a cloak when one is worn. Assign the parts' meshes in the Blueprint (the SKM_Manny_*
 * parts). What's worn is the Equipment component's business.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenBaseCharacter : public ACharacter
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

	/** The mesh component an armor slot dresses: Chest is the torso; FullBody and non-armor slots have none. */
	USkeletalMeshComponent* GetBodyPartFor(EVaesenEquipmentSlot Slot) const;

	/** Every body part, the torso (Mesh) first, the back last. */
	TArray<USkeletalMeshComponent*> GetBodyParts() const;

	UFUNCTION(BlueprintPure, Category = "Equipment")
	UVaesenEquipmentComponent* GetEquipment() const { return Equipment; }

protected:
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

	/** What the character wears and holds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UVaesenEquipmentComponent> Equipment;

private:
	USkeletalMeshComponent* CreateBodyPart(FName Name);
};
