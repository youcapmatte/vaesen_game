#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "VaesenBaseCharacter.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UGameplayEffect;
class USkeletalMeshComponent;
class UVaesenAttributeSet;
class UVaesenEquipmentComponent;
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
 *
 * Gameplay Ability System: every character has an ability system and vitals (UVaesenAttributeSet). A player's live on
 * their player state (they outlast the character); an NPC's on the character itself. Startup Abilities are given and
 * Startup Effects applied (on the server) when the character connects to its ability system.
 */
UCLASS(Abstract)
class VAESENGAME_API AVaesenBaseCharacter : public ACharacter, public IAbilitySystemInterface
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

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/** The character's vitals (Health, Mental), or null before it's connected to its ability system. */
	UFUNCTION(BlueprintPure, Category = "Abilities")
	UVaesenAttributeSet* GetAttributes() const;

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

	/** Abilities the character has from the start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Effects applied from the start (e.g. starting vitals, a lasting trait). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

	/**
	 * Connects the character to its ability system, owned by OwnerActor (the player state for players, the character
	 * for NPCs). Call on the server and the owning client; the server also gives the startup abilities and effects
	 * (once per ability system).
	 */
	void InitAbilitySystem(UAbilitySystemComponent* InAbilitySystem, AActor* OwnerActor);

private:
	USkeletalMeshComponent* CreateBodyPart(FName Name);

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
};
