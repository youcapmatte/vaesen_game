#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VaesenInteractorComponent.generated.h"

class AVaesenPlayerCharacter;
class UVaesenInteractionComponent;

/** What the player is looking at and doing with it. */
USTRUCT(BlueprintType)
struct FVaesenInteractionData
{
	GENERATED_BODY()

	/** The interactable being looked at (focused), if any. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UVaesenInteractionComponent> ViewedInteractionComponent;

	/** World time of the last trace. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float LastInteractionCheckTime = 0.0f;

	/** The interact key is down. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bInteractHeld = false;
};

/**
 * The player's side of interaction (on AVaesenPlayerCharacter). The local player sweeps a small sphere along the
 * camera's view: it starts level with the character (so nothing between the camera and the character counts) and
 * reaches TraceDistance in front, the same in first and third person. The first actor hit with a
 * UVaesenInteractionComponent that allows this player (CanInteract) becomes the focus: its card and outline show.
 *
 * BeginInteract / EndInteract (the interact key) run here and on the server. Holding starts a timer of the
 * component's InteractionTime (or interacts at once); the server runs its own timer and is the one that interacts.
 * Looking away, moving out of reach or letting go ends it.
 */
UCLASS(ClassGroup = (Vaesen), meta = (BlueprintSpawnableComponent))
class VAESENGAME_API UVaesenInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVaesenInteractorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Interact key pressed. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void BeginInteract();

	/** Interact key released. */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void EndInteract();

	UFUNCTION(BlueprintPure, Category = "Interaction")
	UVaesenInteractionComponent* GetViewedInteractionComponent() const { return InteractionData.ViewedInteractionComponent; }

	UFUNCTION(BlueprintPure, Category = "Interaction")
	const FVaesenInteractionData& GetInteractionData() const { return InteractionData; }

	/** Holding the key on something that takes time. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	bool IsInteracting() const;

	/** Seconds of holding left (0 when not interacting). */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetRemainingInteractTime() const;

	/** 0..1 of the current hold. */
	UFUNCTION(BlueprintPure, Category = "Interaction")
	float GetInteractionProgress() const;

	/** How far in front of the character the trace reaches. Each interactable also has its own InteractionDistance. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float TraceDistance = 200.0f;

	/** Radius of the swept sphere: forgiving aim on small things without catching what's beside them. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "cm"))
	float TraceRadius = 10.0f;

	/** Seconds between traces; 0 = every frame. */
	UPROPERTY(EditDefaultsOnly, Category = "Interaction", meta = (ClampMin = "0", Units = "s"))
	float CheckInterval = 0.0f;

private:
	AVaesenPlayerCharacter* GetCharacter() const;

	/** Traces for an interactable and moves the focus to what it finds. Local player only. */
	void PerformInteractionCheck();
	void FoundNewInteractable(UVaesenInteractionComponent* Interactable);
	void CouldntFindInteractable();

	/** Begins interacting with the focused component: the timer, or Interact right away. */
	void StartInteracting();
	void Interact();

	/** The player pressed interact while looking at this actor; the server checks it itself. */
	UFUNCTION(Server, Reliable)
	void ServerBeginInteract(AActor* InteractableActor);

	UFUNCTION(Server, Reliable)
	void ServerEndInteract();

	UPROPERTY(Transient)
	FVaesenInteractionData InteractionData;

	FTimerHandle InteractTimer;
};
