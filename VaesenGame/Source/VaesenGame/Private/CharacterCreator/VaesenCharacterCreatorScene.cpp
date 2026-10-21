#include "CharacterCreator/VaesenCharacterCreatorScene.h"

#include "Appearance/VaesenAppearanceComponent.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Components/LODSyncComponent.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"

AVaesenCharacterCreatorScene::AVaesenCharacterCreatorScene()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetRelativeTransform(BodyFraming);
	Appearance = CreateDefaultSubobject<UVaesenAppearanceComponent>(TEXT("Appearance"));
	// The MetaHuman faces the camera (MetaHuman Blueprints face +Y; yaw 90 turns them toward -X, where the camera is).
	Appearance->MetaHumanOffset = FTransform(FRotator(0.f, 90.f, 0.f));
}

void AVaesenCharacterCreatorScene::Open(APlayerController* Player, FName Sex)
{
	if (!Creator)
	{
		Creator = NewObject<UVaesenCharacterCreator>(this);
		Creator->OnChanged.AddDynamic(this, &AVaesenCharacterCreatorScene::HandleChanged);
	}
	Appearance->PresetTable = Presets;
	Appearance->SkinToneTable = SkinToneTable;
	Framing = EVaesenCreatorFraming::Body;
	Zoom = ZoomTarget = 0.f;
	ApplyZoom();
	TurntableYaw = 0.f;
	if (Player)
	{
		Player->SetViewTarget(this);
	}
	Creator->Begin(Presets, Sex);
}

void AVaesenCharacterCreatorScene::Preview(const FVaesenAppearance& InAppearance)
{
	Appearance->PresetTable = Presets;
	Appearance->SkinToneTable = SkinToneTable;
	Framing = EVaesenCreatorFraming::Body;
	ZoomTarget = 0.f;
	if (!InAppearance.IsSet())
	{
		Appearance->ClearMetaHuman();
		return;
	}
	AActor* const Before = Appearance->GetSpawnedMetaHuman();
	Appearance->SetAppearance(InAppearance);
	if (AActor* const MetaHuman = Appearance->GetSpawnedMetaHuman(); MetaHuman && MetaHuman != Before)
	{
		TurntableYaw = 0.f;
		ApplyTurntable();
		MeasureHead(MetaHuman);
	}
}

void AVaesenCharacterCreatorScene::BecomeView(APlayerController* Player)
{
	if (Player)
	{
		Player->SetViewTarget(this);
	}
}

void AVaesenCharacterCreatorScene::BeginPlay()
{
	Super::BeginPlay();
	if (!OpenOnBeginPlay.IsNone())
	{
		Open(GetWorld()->GetFirstPlayerController(), OpenOnBeginPlay);
	}
}

void AVaesenCharacterCreatorScene::UpdateFieldOfView()
{
	// Cameras take a horizontal field of view; derive it from the vertical one and the viewport's shape.
	FVector2D Size(16.f, 9.f);
	if (const UGameViewportClient* const Viewport = GetWorld()->GetGameViewport())
	{
		Viewport->GetViewportSize(Size);
	}
	const double Aspect = Size.Y > 0.0 ? Size.X / Size.Y : 16.0 / 9.0;
	const double HalfVertical = FMath::DegreesToRadians(VerticalFieldOfView * 0.5);
	Camera->SetFieldOfView(FMath::Min(170.f, (float)FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(HalfVertical) * Aspect))));
}

void AVaesenCharacterCreatorScene::AddZoom(float Delta)
{
	ZoomTarget = FMath::Clamp(ZoomTarget + Delta, 0.f, 1.f);
}

void AVaesenCharacterCreatorScene::ApplyZoom()
{
	FTransform Blended;
	Blended.Blend(BodyFraming, FaceFraming, Zoom);
	Blended.AddToTranslation(FVector(0.f, 0.f, HeadHeightOffset * Zoom));
	Camera->SetRelativeTransform(Blended);
}

void AVaesenCharacterCreatorScene::MeasureHead(AActor* MetaHuman)
{
	// From the face skeleton's reference pose, so it doesn't depend on what the animation is doing.
	HeadHeightOffset = 0.f;
	TArray<USkeletalMeshComponent*> Meshes;
	MetaHuman->GetComponents(Meshes);
	for (const USkeletalMeshComponent* Mesh : Meshes)
	{
		const int32 Bone = Mesh->GetFName() == Appearance->FaceComponentName && Mesh->GetSkeletalMeshAsset()
			? Mesh->GetBoneIndex(TEXT("FACIAL_C_FacialRoot")) : INDEX_NONE;
		if (Bone != INDEX_NONE)
		{
			const FTransform RefPose = FAnimationRuntime::GetComponentSpaceTransformRefPose(Mesh->GetSkeletalMeshAsset()->GetRefSkeleton(), Bone);
			const float Height = (float)(Mesh->GetComponentTransform().TransformPosition(RefPose.GetLocation()).Z - GetActorLocation().Z);
			HeadHeightOffset = Height - FaceFramingHeadHeight;
			UE_LOG(LogTemp, Log, TEXT("Character creator: %s's head at %.1f cm (framing moves %.1f cm)"), *GetNameSafe(MetaHuman), Height, HeadHeightOffset);
			return;
		}
	}
}

void AVaesenCharacterCreatorScene::AddTurntableYaw(float Degrees)
{
	TurntableYaw = FMath::Fmod(TurntableYaw + Degrees, 360.f);
	ApplyTurntable();
}

void AVaesenCharacterCreatorScene::ApplyTurntable()
{
	if (AActor* MetaHuman = Appearance->GetSpawnedMetaHuman())
	{
		MetaHuman->SetActorRelativeRotation(Appearance->MetaHumanOffset.GetRotation() * FQuat(FRotator(0.f, TurntableYaw, 0.f)));
	}
}

void AVaesenCharacterCreatorScene::HandleChanged()
{
	AActor* const Before = Appearance->GetSpawnedMetaHuman();
	Appearance->SetAppearance(Creator->GetAppearance());
	if (AActor* const MetaHuman = Appearance->GetSpawnedMetaHuman(); MetaHuman && MetaHuman != Before)
	{
		// A new base face: keep the turntable's angle. The creator is a close-up of one character: full detail (strand
		// hair) whatever the distance, as its LOD Sync component decides every part's LOD.
		ApplyTurntable();
		if (ULODSyncComponent* LODSync = MetaHuman->FindComponentByClass<ULODSyncComponent>())
		{
			LODSync->ForcedLOD = 0;
		}
		MeasureHead(MetaHuman);
		ApplyZoom();
	}

	const EVaesenCreatorFraming NewFraming = Creator->GetFraming();
	if (NewFraming != Framing)
	{
		Framing = NewFraming;
		ZoomTarget = Framing == EVaesenCreatorFraming::Face ? 1.f : 0.f;
	}
}

void AVaesenCharacterCreatorScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFieldOfView();
	if (Zoom != ZoomTarget)
	{
		Zoom = FMath::Abs(ZoomTarget - Zoom) < 0.001f ? ZoomTarget : FMath::FInterpTo(Zoom, ZoomTarget, DeltaSeconds, ZoomSpeed);
		ApplyZoom();
	}
}
