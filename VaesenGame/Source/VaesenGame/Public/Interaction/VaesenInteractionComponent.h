#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "VaesenInteractionComponent.generated.h"

class AVaesenPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FVaesenInteractionEvent, AVaesenPlayerCharacter*, Character);

/**
 * Makes its actor interactable (a pickup, a door, a lever...). Add it to the actor and bind the events on the actor
 * (OnInteract is the one that does the thing); the actor fills in the card (name, description, quantity, action).
 *
 * It's a screen-space widget component: it shows an interaction card (UVaesenInteractionCardWidget; Widget Class is
 * WBP_InteractionCard by default, else the one in Project Settings > Vaesen) at its location while the local player
 * looks at it, and the actor's meshes get
 * the yellow outline (custom stencil HighlightStencilValue, drawn by PP_InteractionOutline on the player's camera).
 *
 * Players find it with UVaesenInteractorComponent. The flow, and where each part runs:
 *   BeginFocus / EndFocus      looked at / looked away; the looking player's machine only (card and outline).
 *   BeginInteract / EndInteract   interact key pressed / released or looked away; on that player's machine and
 *                              the server.
 *   Interact                   after InteractionTime of holding (immediately if 0); server only, so do game changes
 *                              (give the item, open the door) there.
 * Interactors holds the players interacting right now (on the server: all of them; on a client: only its own).
 * With bAllowMultipleInteractors off, nobody else can start while someone is interacting.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenInteractionComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	/** Custom stencil value of the focused actor's meshes; PP_InteractionOutline outlines this value. */
	static constexpr int32 HighlightStencilValue = 1;

	UVaesenInteractionComponent();

	// --- Card content (set in the actor's defaults, or at runtime with the setters, which update the card) ---

	/** E.g. "Lantern". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card")
	FText InteractableName;

	/** What interacting does, e.g. "Pick up", "Open". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card")
	FText ActionText;

	/** Optional extra line(s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card", meta = (MultiLine = "true"))
	FText Description;

	/** E.g. how many items a pickup holds. 0 or less = not shown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card")
	int32 Quantity = 0;

	// --- Rules ---

	/** How long the interact key must be held. 0 = interacts at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "s"))
	float InteractionTime = 0.0f;

	/** How close (from the player's eyes to the actor's nearest edge) a player must be to interact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float InteractionDistance = 200.0f;

	/** Off: only one player at a time can interact (e.g. searching a chest). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bAllowMultipleInteractors = true;

	/** Off: nobody can focus or interact (e.g. a door locked for good). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;

	/**
	 * Show the card just above the actor's top, wherever it is and however it lies (e.g. a dropped item), instead of
	 * at this component's own place.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card")
	bool bPlaceCardAboveOwner = false;

	/** How far above the actor's top, with bPlaceCardAboveOwner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Card", meta = (Units = "cm", EditCondition = "bPlaceCardAboveOwner"))
	float CardHeightAboveOwner = 15.0f;

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractableName(const FText& NewName);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetActionText(const FText& NewActionText);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetDescription(const FText& NewDescription);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetQuantity(int32 NewQuantity);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetInteractionEnabled(bool bEnabled);

	/** Whether this player may interact now: enabled, close enough, and not taken by someone else. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool CanInteract(const AVaesenPlayerCharacter* Character) const;

	/** Distance from the player's eyes to the nearest edge of this actor's (colliding) components. */
	float GetDistanceTo(const AVaesenPlayerCharacter* Character) const;

	UFUNCTION(BlueprintPure, Category = "Interaction")
	TArray<AVaesenPlayerCharacter*> GetInteractors() const { return ObjectPtrDecay(Interactors); }

	/** The local player's hold progress, 0..1 (0 when not holding); for the card's progress bar. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionProgress() const;

	/** Pushes the content into the card. Called by the setters and when the card is shown. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshCard();

	// --- Events (bind on the actor) ---

	/** The local player started looking at it. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FVaesenInteractionEvent OnBeginFocus;

	/** The local player stopped looking at it. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FVaesenInteractionEvent OnEndFocus;

	/** A player pressed interact on it (that player's machine and the server). */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FVaesenInteractionEvent OnBeginInteract;

	/** A player let go, looked away or moved out of reach (that player's machine and the server). */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FVaesenInteractionEvent OnEndInteract;

	/** The interaction happened (held for InteractionTime). Server only. */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FVaesenInteractionEvent OnInteract;

	// --- Called by UVaesenInteractorComponent ---

	void BeginFocus(AVaesenPlayerCharacter* Character);
	void EndFocus(AVaesenPlayerCharacter* Character);
	void BeginInteract(AVaesenPlayerCharacter* Character);
	void EndInteract(AVaesenPlayerCharacter* Character);
	void Interact(AVaesenPlayerCharacter* Character);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Outlines (or stops outlining) the actor's meshes. */
	void SetHighlighted(bool bHighlighted);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AVaesenPlayerCharacter>> Interactors;

	/** The local player looking at it, if any. */
	TWeakObjectPtr<AVaesenPlayerCharacter> FocusingCharacter;
};
