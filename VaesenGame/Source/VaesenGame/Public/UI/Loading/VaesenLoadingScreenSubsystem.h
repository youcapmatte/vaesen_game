#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "VaesenLoadingScreenSubsystem.generated.h"

class UNetDriver;
class UPackage;
class UVaesenLoadingScreenWidget;
class SWidget;
struct FWorldContext;

DECLARE_LOG_CATEGORY_EXTERN(LogVaesenLoading, Log, All);

/**
 * Shows the loading screen (UVaesenSettings::LoadingScreenWidget) whenever the client changes map, and hides it with
 * a fade once the new map is really ready. Clients only; dedicated servers never create it.
 *
 * Travel through TravelToMap / TravelToServer for the full experience:
 *   1. the loading screen fades in over the current map,
 *   2. the destination map loads in the background (the progress bar shows real progress),
 *   3. the travel itself, which is quick because the map is already in memory (the screen holds still meanwhile),
 *   4. waiting until the map is ready: level/World Partition streaming done, your character spawned (on a server),
 *      shaders compiled,
 *   5. a short settle so textures stream in, then the screen fades out to reveal the map.
 * Any other map change (e.g. a server sending you elsewhere) still shows the screen, from step 3.
 *
 * The widget lives in the game viewport, not in a world, so it survives the map change.
 */
UCLASS()
class VAESENGAME_API UVaesenLoadingScreenSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Opens a map locally (front end, standalone). Without the loading screen, the map just opens (e.g. login to character select). */
	void TravelToMap(const TSoftObjectPtr<UWorld>& Map, bool bShowLoadingScreen = true);

	/** Connects to a game server ("ip:port", plus URL options such as "?PlayerSessionId=..."); Map is preloaded meanwhile. */
	void TravelToServer(const FString& Address, const TSoftObjectPtr<UWorld>& Map, const FString& Options = FString());

	bool IsShowingLoadingScreen() const { return Phase != EPhase::Hidden; }

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
	virtual bool IsTickable() const override { return Phase != EPhase::Hidden && !HasAnyFlags(RF_ClassDefaultObject); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UVaesenLoadingScreenSubsystem, STATGROUP_Tickables); }

private:
	enum class EPhase : uint8
	{
		Hidden,
		FadingIn,
		Preloading,
		LoadingMap,
		WaitingForWorld,
		Settling,
		FadingOut,
	};

	void StartTravel(const FString& URL, const FString& MapPackageName);
	void BeginPreload();
	void Travel();

	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* World);
	void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);

	/** Keeps a travel/connection error for character select to show. */
	void RememberTravelError(const FText& Error);

	/** Creates the widget and puts it on top of the game viewport. False when there's no widget class or viewport. */
	bool ShowWidget(const FString& MapPackageName, bool bFadeIn);
	void RemoveWidget();
	void FinishLoading();

	/** How many of the readiness checks pass, out of OutTotal; all pass = ready to reveal the map. */
	int32 CountReadyChecks(int32& OutTotal) const;

	void SetPhase(EPhase NewPhase);
	void SetTargetProgress(float Progress);

	UPROPERTY(Transient)
	TObjectPtr<UVaesenLoadingScreenWidget> Widget;

	/** The destination map, preloaded and kept in memory until the travel has used it. */
	UPROPERTY(Transient)
	TObjectPtr<UPackage> PreloadedMap;

	TSharedPtr<SWidget> ViewportContent;

	EPhase Phase = EPhase::Hidden;
	/** The next map load is one we started without a loading screen. */
	bool bSkipNextLoadingScreen = false;

	/** The travel connects to a server (status says "Connecting..." until the map starts loading). */
	bool bConnecting = false;
	FText MapDisplayName;
	FString PendingURL;
	FString PendingMapPackage;
	int32 PreloadRequest = INDEX_NONE;

	double ShownAt = 0.0;
	double PhaseStartedAt = 0.0;
	float Opacity = 0.0f;
	float TargetProgress = 0.0f;
	float DisplayedProgress = 0.0f;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle TravelFailureHandle;
	FDelegateHandle NetworkFailureHandle;
};
