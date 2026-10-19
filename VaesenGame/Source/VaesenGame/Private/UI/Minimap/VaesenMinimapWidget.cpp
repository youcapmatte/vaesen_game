#include "UI/Minimap/VaesenMinimapWidget.h"

#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UI/Minimap/VaesenMinimapCapture.h"

void UVaesenMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UWorld* World = GetWorld();
	if (!World || !MapImage)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	MapCapture = World->SpawnActor<AVaesenMinimapCapture>(CaptureClass ? CaptureClass.Get() : AVaesenMinimapCapture::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!MapCapture)
	{
		return;
	}

	if (MapMaterial)
	{
		MapMaterialInstance = UMaterialInstanceDynamic::Create(MapMaterial, this);
		MapMaterialInstance->SetTextureParameterValue(TEXT("MapTexture"), MapCapture->GetRenderTarget());
		MapImage->SetBrushFromMaterial(MapMaterialInstance);
	}
	else
	{
		MapImage->SetBrushResourceObject(MapCapture->GetRenderTarget());
	}
}

void UVaesenMinimapWidget::NativeDestruct()
{
	if (MapCapture)
	{
		MapCapture->Destroy();
		MapCapture = nullptr;
	}
	Super::NativeDestruct();
}

void UVaesenMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const APlayerController* Player = GetOwningPlayer();
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	if (!MapCapture || !Pawn)
	{
		return;
	}

	// Over the player; turned with their view so up on the map is where they look.
	const float Yaw = bRotateWithView ? Player->GetControlRotation().Yaw : 0.0f;
	MapCapture->Follow(Pawn->GetActorLocation(), Yaw);
}
