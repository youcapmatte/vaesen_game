#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaesenInteractableComponent.generated.h"

/**
 * Makes its actor something a player can interact with. When the local player looks at it within reach (see
 * AVaesenPlayerCharacter::InteractionRange) its meshes get a yellow outline: they're drawn into the custom stencil
 * buffer with HighlightStencilValue, which the PP_InteractionOutline post-process material outlines. The highlight is
 * local only; nothing replicates.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenInteractableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Custom stencil value of highlighted meshes; PP_InteractionOutline outlines this value. */
	static constexpr int32 HighlightStencilValue = 1;

	UVaesenInteractableComponent();

	/** Outlines (or stops outlining) every mesh on the owning actor. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetHighlighted(bool bInHighlighted);

	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsHighlighted() const { return bHighlighted; }

	/** What interacting does, for a prompt such as "Open door". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionText;

	/** Turn off to stop players focusing (and outlining) this actor, e.g. a door that's locked for good. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bCanInteract = true;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bHighlighted = false;
};
