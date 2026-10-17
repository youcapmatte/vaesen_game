#include "UI/Loading/VaesenLoadingScreenSubsystem.h"

#include "Core/VaesenSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "PipelineStateCache.h"
#include "ShaderPipelineCache.h"
#include "UI/Loading/VaesenLoadingScreenWidget.h"
#include "UObject/Package.h"
#include "WorldPartition/WorldPartitionSubsystem.h"

DEFINE_LOG_CATEGORY(LogVaesenLoading);

#define LOCTEXT_NAMESPACE "VaesenLoadingScreen"

namespace VaesenLoading
{
	/** Above every other viewport widget. */
	constexpr int32 ZOrder = 10000;

	// Where each phase puts the progress bar (0..1).
	constexpr float PreloadStart = 0.05f;
	constexpr float PreloadEnd = 0.65f;
	constexpr float MapLoading = 0.7f;
	constexpr float WorldStart = 0.75f;
	constexpr float WorldEnd = 0.95f;

	/** How fast the bar catches up with the real progress, in bar widths per second. */
	constexpr float ProgressSpeed = 1.2f;

	const FVaesenMapLoadingInfo* FindMapInfo(const FString& MapPackageName)
	{
		return UVaesenSettings::Get()->MapLoadingInfo.FindByPredicate([&MapPackageName](const FVaesenMapLoadingInfo& Info)
		{
			return Info.Map.ToSoftObjectPath().GetLongPackageName() == MapPackageName;
		});
	}

	FText GetMapDisplayName(const FString& MapPackageName)
	{
		const FVaesenMapLoadingInfo* Info = FindMapInfo(MapPackageName);
		if (Info && !Info->DisplayName.IsEmpty())
		{
			return Info->DisplayName;
		}

		FString Name = FPackageName::GetShortName(MapPackageName);
		Name.RemoveFromStart(TEXT("L_"));
		return FText::FromString(Name);
	}

	UTexture2D* LoadBackground(const FString& MapPackageName)
	{
		const UVaesenSettings* Settings = UVaesenSettings::Get();
		const FVaesenMapLoadingInfo* Info = FindMapInfo(MapPackageName);
		if (Info && !Info->Background.IsNull())
		{
			return Info->Background.LoadSynchronous();
		}
		if (!Settings->LoadingBackgrounds.IsEmpty())
		{
			return Settings->LoadingBackgrounds[FMath::RandRange(0, Settings->LoadingBackgrounds.Num() - 1)].LoadSynchronous();
		}
		return nullptr;
	}
}

bool UVaesenLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVaesenLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UVaesenLoadingScreenSubsystem::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UVaesenLoadingScreenSubsystem::HandlePostLoadMap);
	if (GEngine)
	{
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UVaesenLoadingScreenSubsystem::HandleTravelFailure);
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UVaesenLoadingScreenSubsystem::HandleNetworkFailure);
	}
}

void UVaesenLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	if (GEngine)
	{
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}

	RemoveWidget();
	PreloadedMap = nullptr;
	Phase = EPhase::Hidden;
	Super::Deinitialize();
}

void UVaesenLoadingScreenSubsystem::TravelToMap(const TSoftObjectPtr<UWorld>& Map)
{
	const FString MapPackage = Map.ToSoftObjectPath().GetLongPackageName();
	if (MapPackage.IsEmpty())
	{
		UE_LOG(LogVaesenLoading, Warning, TEXT("TravelToMap: no map given."));
		return;
	}

	bConnecting = false;
	StartTravel(MapPackage, MapPackage);
}

void UVaesenLoadingScreenSubsystem::TravelToServer(const FString& Address, const TSoftObjectPtr<UWorld>& Map, const FString& Options)
{
	bConnecting = true;
	StartTravel(Address + Options, Map.ToSoftObjectPath().GetLongPackageName());
}

void UVaesenLoadingScreenSubsystem::StartTravel(const FString& URL, const FString& MapPackageName)
{
	if (Phase != EPhase::Hidden && Phase != EPhase::FadingOut)
	{
		UE_LOG(LogVaesenLoading, Warning, TEXT("Already traveling; ignoring travel to %s."), *URL);
		return;
	}

	RemoveWidget();
	PendingURL = URL;
	PendingMapPackage = MapPackageName;
	TargetProgress = 0.0f;
	DisplayedProgress = 0.0f;

	if (!ShowWidget(MapPackageName, /*bFadeIn*/ true))
	{
		// No loading screen configured: just go.
		Travel();
		return;
	}

	SetPhase(EPhase::FadingIn);
}

void UVaesenLoadingScreenSubsystem::BeginPreload()
{
	// Nothing to preload: unknown destination, already in memory, or PIE (whose maps are separate in-memory copies).
	const UWorld* World = GetGameInstance()->GetWorld();
	const bool bPIE = World && World->WorldType == EWorldType::PIE;
	if (PendingMapPackage.IsEmpty() || bPIE || FindPackage(nullptr, *PendingMapPackage))
	{
		Travel();
		return;
	}

	SetPhase(EPhase::Preloading);
	SetTargetProgress(VaesenLoading::PreloadStart);
	PreloadRequest = LoadPackageAsync(PendingMapPackage, FLoadPackageAsyncDelegate::CreateWeakLambda(this,
		[this](const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)
		{
			if (Phase != EPhase::Preloading)
			{
				return;
			}

			if (Result == EAsyncLoadingResult::Succeeded && Package)
			{
				// Keep it in memory until the travel picks it up (LoadMap reuses a loaded package).
				PreloadedMap = Package;
			}
			else
			{
				UE_LOG(LogVaesenLoading, Warning, TEXT("Preloading %s failed; the travel will load it instead."), *PackageName.ToString());
			}
			Travel();
		}));
}

void UVaesenLoadingScreenSubsystem::Travel()
{
	PreloadRequest = INDEX_NONE;
	UWorld* World = GetGameInstance()->GetWorld();
	if (!World || !GEngine)
	{
		return;
	}

	UE_LOG(LogVaesenLoading, Log, TEXT("Traveling to %s"), *PendingURL);
	if (Phase != EPhase::Hidden)
	{
		SetPhase(EPhase::LoadingMap);
		SetTargetProgress(bConnecting ? VaesenLoading::PreloadEnd : VaesenLoading::MapLoading);
	}
	GEngine->SetClientTravel(World, *PendingURL, TRAVEL_Absolute);
}

void UVaesenLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance != GetGameInstance())
	{
		return;
	}

	const FString MapPackage = UWorld::RemovePIEPrefix(MapName);
	if (Phase == EPhase::Hidden || Phase == EPhase::FadingOut)
	{
		// A map change we didn't start (e.g. the server moved us): show the screen right away.
		RemoveWidget();
		PendingMapPackage = MapPackage;
		TargetProgress = DisplayedProgress = VaesenLoading::PreloadEnd;
		if (!ShowWidget(MapPackage, /*bFadeIn*/ false))
		{
			return;
		}
	}

	bConnecting = false;
	SetPhase(EPhase::LoadingMap);
	SetTargetProgress(VaesenLoading::MapLoading);
	if (Widget)
	{
		Widget->SetProgress(DisplayedProgress);
	}

	// LoadMap blocks the game thread from here on; draw the screen once now so it's what stays on screen.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick();
	}
}

void UVaesenLoadingScreenSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (Phase == EPhase::Hidden || (World && World->GetGameInstance() != GetGameInstance()))
	{
		return;
	}

	PreloadedMap = nullptr;
	if (!World)
	{
		UE_LOG(LogVaesenLoading, Warning, TEXT("Map load failed."));
		FinishLoading();
		return;
	}

	SetPhase(EPhase::WaitingForWorld);
	SetTargetProgress(VaesenLoading::WorldStart);
}

void UVaesenLoadingScreenSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	if (Phase != EPhase::Hidden)
	{
		UE_LOG(LogVaesenLoading, Warning, TEXT("Travel failed (%s): %s"), ETravelFailure::ToString(FailureType), *Error);
		PreloadedMap = nullptr;
		FinishLoading();
	}
}

void UVaesenLoadingScreenSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	if (Phase != EPhase::Hidden)
	{
		UE_LOG(LogVaesenLoading, Warning, TEXT("Network failure while loading (%s): %s"), ENetworkFailure::ToString(FailureType), *Error);
		PreloadedMap = nullptr;
		FinishLoading();
	}
}

bool UVaesenLoadingScreenSubsystem::ShowWidget(const FString& MapPackageName, bool bFadeIn)
{
	UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient();
	const TSubclassOf<UVaesenLoadingScreenWidget> WidgetClass = UVaesenSettings::Get()->LoadingScreenWidget.LoadSynchronous();
	if (!Viewport || !WidgetClass)
	{
		return false;
	}

	// Owned by the game instance and added to the viewport directly (not AddToViewport, which ties it to the
	// current world), so it stays up while the world is replaced.
	Widget = CreateWidget<UVaesenLoadingScreenWidget>(GetGameInstance(), WidgetClass);
	if (!Widget)
	{
		return false;
	}

	const UVaesenSettings* Settings = UVaesenSettings::Get();
	MapDisplayName = VaesenLoading::GetMapDisplayName(MapPackageName);
	Widget->SetMapName(MapDisplayName);
	Widget->SetBackground(VaesenLoading::LoadBackground(MapPackageName));
	Widget->SetTip(Settings->LoadingTips.IsEmpty() ? FText::GetEmpty() : Settings->LoadingTips[FMath::RandRange(0, Settings->LoadingTips.Num() - 1)]);
	Widget->SetProgress(DisplayedProgress);

	Opacity = bFadeIn && Settings->LoadingScreenFadeInSeconds > 0.0f ? 0.0f : 1.0f;
	Widget->SetRenderOpacity(Opacity);

	ViewportContent = Widget->TakeWidget();
	Viewport->AddViewportWidgetContent(ViewportContent.ToSharedRef(), VaesenLoading::ZOrder);
	ShownAt = FPlatformTime::Seconds();
	Widget->OnLoadingScreenShown();
	return true;
}

void UVaesenLoadingScreenSubsystem::RemoveWidget()
{
	if (ViewportContent.IsValid())
	{
		if (UGameViewportClient* Viewport = GetGameInstance()->GetGameViewportClient())
		{
			Viewport->RemoveViewportWidgetContent(ViewportContent.ToSharedRef());
		}
	}
	ViewportContent.Reset();
	Widget = nullptr;
}

void UVaesenLoadingScreenSubsystem::FinishLoading()
{
	SetTargetProgress(1.0f);
	if (Widget)
	{
		Widget->OnLoadingFinished();
	}
	SetPhase(EPhase::FadingOut);
}

int32 UVaesenLoadingScreenSubsystem::CountReadyChecks(int32& OutTotal) const
{
	OutTotal = 5;
	const UWorld* World = GetGameInstance()->GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Ready = 0;

	// 1. The world is running.
	Ready += World->HasBegunPlay() ? 1 : 0;

	// 2. Streaming levels it wants are loaded and shown.
	bool bLevelsReady = true;
	for (const ULevelStreaming* Level : World->GetStreamingLevels())
	{
		if (Level && (Level->HasLoadRequestPending()
			|| (Level->ShouldBeLoaded() && !Level->IsLevelLoaded())
			|| (Level->ShouldBeVisible() && !Level->IsLevelVisible())))
		{
			bLevelsReady = false;
			break;
		}
	}
	Ready += bLevelsReady ? 1 : 0;

	// 3. World Partition has streamed in the cells around the player.
	const UWorldPartitionSubsystem* WorldPartition = World->GetSubsystem<UWorldPartitionSubsystem>();
	Ready += (!World->IsPartitionedWorld() || !WorldPartition || WorldPartition->IsStreamingCompleted()) ? 1 : 0;

	// 4. On a server: the game state has arrived and we control our character.
	bool bPlayerReady = true;
	if (World->GetNetMode() == NM_Client)
	{
		const APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController(World);
		bPlayerReady = World->GetGameState() && PlayerController && PlayerController->GetPawn();
	}
	Ready += bPlayerReady ? 1 : 0;

	// 5. Shaders for what's on screen are compiled, so the reveal doesn't stutter.
	Ready += (FShaderPipelineCache::NumPrecompilesRemaining() == 0 && PipelineStateCache::NumActivePrecacheRequests() == 0) ? 1 : 0;

	return Ready;
}

void UVaesenLoadingScreenSubsystem::Tick(float DeltaTime)
{
	const UVaesenSettings* Settings = UVaesenSettings::Get();
	const double Now = FPlatformTime::Seconds();
	const double InPhase = Now - PhaseStartedAt;

	switch (Phase)
	{
	case EPhase::FadingIn:
		Opacity = Settings->LoadingScreenFadeInSeconds > 0.0f ? FMath::Min(1.0f, Opacity + DeltaTime / Settings->LoadingScreenFadeInSeconds) : 1.0f;
		if (Opacity >= 1.0f)
		{
			BeginPreload();
		}
		break;

	case EPhase::Preloading:
	{
		const float Percent = GetAsyncLoadPercentage(FName(*PendingMapPackage));
		if (Percent >= 0.0f)
		{
			SetTargetProgress(FMath::Lerp(VaesenLoading::PreloadStart, VaesenLoading::PreloadEnd, Percent / 100.0f));
		}
		break;
	}

	case EPhase::WaitingForWorld:
	{
		int32 Total = 0;
		const int32 Ready = CountReadyChecks(Total);
		SetTargetProgress(FMath::Lerp(VaesenLoading::WorldStart, VaesenLoading::WorldEnd, Total > 0 ? float(Ready) / Total : 1.0f));

		const bool bShownLongEnough = Now - ShownAt >= Settings->LoadingScreenMinimumSeconds;
		if (Ready == Total && bShownLongEnough)
		{
			SetPhase(EPhase::Settling);
		}
		else if (InPhase >= Settings->LoadingScreenTimeoutSeconds)
		{
			UE_LOG(LogVaesenLoading, Warning, TEXT("Map still not ready after %.0f s (%d of %d checks pass); showing it anyway."),
				Settings->LoadingScreenTimeoutSeconds, Ready, Total);
			SetPhase(EPhase::Settling);
		}
		break;
	}

	case EPhase::Settling:
		SetTargetProgress(1.0f);
		if (InPhase >= Settings->LoadingScreenSettleSeconds && DisplayedProgress >= 1.0f)
		{
			FinishLoading();
		}
		break;

	case EPhase::FadingOut:
		Opacity = Settings->LoadingScreenFadeOutSeconds > 0.0f ? FMath::Max(0.0f, Opacity - DeltaTime / Settings->LoadingScreenFadeOutSeconds) : 0.0f;
		if (Opacity <= 0.0f)
		{
			RemoveWidget();
			SetPhase(EPhase::Hidden);
			return;
		}
		break;

	default:
		break;
	}

	DisplayedProgress = FMath::FInterpConstantTo(DisplayedProgress, TargetProgress, DeltaTime, VaesenLoading::ProgressSpeed);
	if (Widget)
	{
		Widget->SetRenderOpacity(Opacity);
		Widget->SetProgress(DisplayedProgress);
	}
}

void UVaesenLoadingScreenSubsystem::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartedAt = FPlatformTime::Seconds();
	if (!Widget)
	{
		return;
	}

	switch (Phase)
	{
	case EPhase::FadingIn:
	case EPhase::Preloading:
		Widget->SetStatus(FText::Format(LOCTEXT("Loading", "Loading {0}..."), MapDisplayName));
		break;
	case EPhase::LoadingMap:
		Widget->SetStatus(bConnecting
			? LOCTEXT("Connecting", "Connecting...")
			: FText::Format(LOCTEXT("Loading", "Loading {0}..."), MapDisplayName));
		break;
	case EPhase::WaitingForWorld:
		Widget->SetStatus(FText::Format(LOCTEXT("Entering", "Entering {0}..."), MapDisplayName));
		break;
	case EPhase::Settling:
		Widget->SetStatus(LOCTEXT("Preparing", "Preparing..."));
		break;
	default:
		break;
	}
}

void UVaesenLoadingScreenSubsystem::SetTargetProgress(float Progress)
{
	// The bar only moves forward.
	TargetProgress = FMath::Max(TargetProgress, FMath::Clamp(Progress, 0.0f, 1.0f));
}

#undef LOCTEXT_NAMESPACE
