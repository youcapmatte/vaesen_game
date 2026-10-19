#include "UI/Inventory/VaesenCharacterPreview.h"

#include "Animation/AnimationAsset.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UObject/ConstructorHelpers.h"

namespace VaesenCharacterPreview
{
	/** Everything in the studio sees only the studio's lights. */
	void UseStudioLighting(UPrimitiveComponent* Component)
	{
		Component->SetLightingChannels(false, false, true);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCanEverAffectNavigation(false);
	}

	/** How many body parts the characters have besides the torso (see AVaesenBaseCharacter::GetBodyParts). */
	constexpr int32 PartCount = 10;
}

AVaesenCharacterPreview::AVaesenCharacterPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(RootComponent);

	// Like the character's own mesh: feet at the origin, facing +X (the camera looks back at it from there).
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Turntable);
	Body->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	VaesenCharacterPreview::UseStudioLighting(Body);

	for (int32 Index = 0; Index < VaesenCharacterPreview::PartCount; ++Index)
	{
		USkeletalMeshComponent* Part = CreateDefaultSubobject<USkeletalMeshComponent>(*FString::Printf(TEXT("Part%d"), Index));
		Part->SetupAttachment(Body);
		VaesenCharacterPreview::UseStudioLighting(Part);
		Parts.Add(Part);
	}

	// A portrait camera in front, a little above the waist, narrow so the figure isn't distorted.
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(RootComponent);
	Capture->SetRelativeLocationAndRotation(FVector(520.0, 0.0, 98.0), FRotator(-1.0, 180.0, 0.0));
	Capture->FOVAngle = 16.0f;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->bCaptureEveryFrame = true;
	Capture->bCaptureOnMovement = false;
	Capture->bAlwaysPersistRenderingState = true;
	// None of the level's atmosphere: just the figure and the studio lights.
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	Capture->ShowFlags.SetSkyLighting(false);
	Capture->ShowFlags.SetLumenGlobalIllumination(false);
	Capture->ShowFlags.SetLumenReflections(false);
	Capture->ShowFlags.SetMotionBlur(false);

	// Warm key from the front left, cool fill from the right, a rim from behind to lift the figure off the dark.
	KeyLight = MakeLight(TEXT("KeyLight"), FVector(300.0, 160.0, 230.0), 45.0f, FLinearColor(1.0f, 0.88f, 0.72f), true);
	FillLight = MakeLight(TEXT("FillLight"), FVector(300.0, -220.0, 120.0), 14.0f, FLinearColor(0.7f, 0.78f, 1.0f), false);
	RimLight = MakeLight(TEXT("RimLight"), FVector(-200.0, -60.0, 220.0), 40.0f, FLinearColor(1.0f, 0.82f, 0.6f), false);

	static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Idle.MM_Idle"));
	IdleAnimation = Idle.Object;
}

UPointLightComponent* AVaesenCharacterPreview::MakeLight(FName Name, const FVector& Location, float Intensity, const FLinearColor& Color, bool bShadows)
{
	UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(Name);
	Light->SetupAttachment(RootComponent);
	Light->SetRelativeLocation(Location);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetIntensity(Intensity);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(1200.0f);
	Light->SetCastShadows(bShadows);
	Light->SetSourceRadius(40.0f);
	Light->LightingChannels.bChannel0 = false;
	Light->LightingChannels.bChannel2 = true;
	return Light;
}

void AVaesenCharacterPreview::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitCustomFormat(RenderTargetSize.X, RenderTargetSize.Y, PF_B8G8R8A8, false);
	Capture->TextureTarget = RenderTarget;
	Capture->ShowOnlyActors.Add(this);

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
	Post.VignetteIntensity = 0.6f;

	for (USkeletalMeshComponent* Part : Parts)
	{
		Part->SetLeaderPoseComponent(Body);
	}
	if (IdleAnimation)
	{
		Body->PlayAnimation(IdleAnimation, true);
	}
}

void AVaesenCharacterPreview::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Capture->TextureTarget = nullptr;
	if (RenderTarget)
	{
		RenderTarget->ReleaseResource();
	}
	Super::EndPlay(EndPlayReason);
}

void AVaesenCharacterPreview::CopyAppearance(const AVaesenBaseCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// Same parts, same order: the torso first, then the rest.
	const TArray<USkeletalMeshComponent*> Source = Character->GetBodyParts();
	if (Source.Num() > 0 && Source[0] && Body->GetSkeletalMeshAsset() != Source[0]->GetSkeletalMeshAsset())
	{
		Body->SetSkeletalMeshAsset(Source[0]->GetSkeletalMeshAsset());
		if (IdleAnimation)
		{
			Body->PlayAnimation(IdleAnimation, true);
		}
	}
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		const USkeletalMeshComponent* From = Source.IsValidIndex(Index + 1) ? Source[Index + 1] : nullptr;
		USkeletalMesh* Mesh = From ? From->GetSkeletalMeshAsset() : nullptr;
		if (Parts[Index]->GetSkeletalMeshAsset() != Mesh)
		{
			Parts[Index]->SetSkeletalMeshAsset(Mesh);
		}
	}

	// Attached items (weapons, tools, talismans, bags): copies of what hangs off the character's torso.
	for (UStaticMeshComponent* Attachment : Attachments)
	{
		Attachment->DestroyComponent();
	}
	Attachments.Reset();
	TArray<USceneComponent*> TorsoChildren;
	Character->GetMesh()->GetChildrenComponents(false, TorsoChildren);
	for (const USceneComponent* Child : TorsoChildren)
	{
		const UStaticMeshComponent* Held = Cast<UStaticMeshComponent>(Child);
		if (!Held || !Held->GetStaticMesh() || !Held->IsVisible())
		{
			continue;
		}
		UStaticMeshComponent* Copy = NewObject<UStaticMeshComponent>(this);
		Copy->SetStaticMesh(Held->GetStaticMesh());
		VaesenCharacterPreview::UseStudioLighting(Copy);
		Copy->SetupAttachment(Body, Held->GetAttachSocketName());
		Copy->SetRelativeTransform(Held->GetRelativeTransform());
		Copy->RegisterComponent();
		Attachments.Add(Copy);
	}
}

void AVaesenCharacterPreview::AddYaw(float Degrees)
{
	Turntable->AddRelativeRotation(FRotator(0.0, Degrees, 0.0));
}
