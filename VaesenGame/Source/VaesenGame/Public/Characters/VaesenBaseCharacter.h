#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "VaesenBaseCharacter.generated.h"

class USkeletalMeshComponent;

/**
 * Shared base for every character in the world, player-controlled or AI.
 * Holds what all characters have in common (capsule, movement, body); no camera or input.
 *
 * Modular body, like clothing/armor systems: the character's Mesh is the Torso and drives the animation (it has the
 * animation Blueprint); Head, Arms, Legs and Feet are separate skeletal meshes on the same skeleton that copy its pose
 * (leader pose), so each part can be swapped (outfits, armor) or hidden on its own. Assign the parts' meshes in the
 * Blueprint (e.g. the SKM_Manny_* parts).
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
	USkeletalMeshComponent* GetArmsMesh() const { return ArmsMesh; }
	USkeletalMeshComponent* GetLegsMesh() const { return LegsMesh; }
	USkeletalMeshComponent* GetFeetMesh() const { return FeetMesh; }

	/** Every body part, the torso (Mesh) first. */
	TArray<USkeletalMeshComponent*> GetBodyParts() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> ArmsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> LegsMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<USkeletalMeshComponent> FeetMesh;

private:
	USkeletalMeshComponent* CreateBodyPart(FName Name);
};
