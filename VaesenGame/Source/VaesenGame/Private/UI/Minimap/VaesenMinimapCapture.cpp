#include "UI/Minimap/VaesenMinimapCapture.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

const FName AVaesenMinimapCapture::HiddenTag(TEXT("MapHidden"));

AVaesenMinimapCapture::AVaesenMinimapCapture()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	RootComponent = Capture;

	// Straight down, flat like a map; filmed on a timer rather than every frame.
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;

	// Just the ground plan: no sky, fog or atmosphere, no trees or grass, no characters or effects.
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetCloud(false);
	Capture->ShowFlags.SetInstancedFoliage(false);
	Capture->ShowFlags.SetInstancedGrass(false);
	Capture->ShowFlags.SetSkeletalMeshes(false);
	Capture->ShowFlags.SetParticles(false);
	Capture->ShowFlags.SetLumenGlobalIllumination(false);
	Capture->ShowFlags.SetLumenReflections(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetBloom(false);
	Capture->ShowFlags.SetLensFlares(false);
}

void AVaesenMinimapCapture::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitCustomFormat(Resolution, Resolution, PF_B8G8R8A8, false);
	Capture->TextureTarget = RenderTarget;
	Capture->OrthoWidth = MapWidth;

	// A steady exposure, whatever the level's.
	Capture->PostProcessBlendWeight = 1.0f;
	FPostProcessSettings& Post = Capture->PostProcessSettings;
	Post.bOverride_AutoExposureMethod = true;
	Post.AutoExposureMethod = AEM_Manual;
	Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Post.AutoExposureApplyPhysicalCameraExposure = false;
	Post.bOverride_AutoExposureBias = true;
	Post.AutoExposureBias = ExposureBias;
	Post.bOverride_VignetteIntensity = true;
	Post.VignetteIntensity = 0.0f;

	// Actors tagged to stay off the map.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(HiddenTag))
		{
			Capture->HiddenActors.Add(*It);
		}
	}

	GetWorldTimerManager().SetTimer(CaptureTimer, this, &AVaesenMinimapCapture::CaptureNow, 1.0f / FMath::Max(UpdatesPerSecond, 1.0f), true);
}

void AVaesenMinimapCapture::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CaptureTimer);
	Capture->TextureTarget = nullptr;
	if (RenderTarget)
	{
		RenderTarget->ReleaseResource();
	}
	Super::EndPlay(EndPlayReason);
}

void AVaesenMinimapCapture::Follow(const FVector& Location, float Yaw)
{
	// Looking down, with the map's up pointing along Yaw.
	SetActorLocationAndRotation(Location + FVector(0.0, 0.0, CameraHeight), FRotator(-90.0, Yaw, 0.0));
}

void AVaesenMinimapCapture::CaptureNow()
{
	Capture->CaptureScene();
}
