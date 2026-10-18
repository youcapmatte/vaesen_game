#include "Characters/VaesenPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Interaction/VaesenInteractorComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Player/VaesenPlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"

AVaesenPlayerCharacter::AVaesenPlayerCharacter()
{
	// The camera follows the controller's rotation; the character itself turns toward its movement (see base class).
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;
	// Look over the shoulder from slightly above the head; the boom pulls in when something blocks the view.
	CameraBoom->SocketOffset = FVector(0.f, 0.f, 60.f);
	CameraBoom->bDoCollisionTest = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	Interactor = CreateDefaultSubobject<UVaesenInteractorComponent>(TEXT("Interactor"));

	// The player's own things: only they can open it.
	Inventory = CreateDefaultSubobject<UVaesenInventoryComponent>(TEXT("Inventory"));
	Inventory->DisplayName = NSLOCTEXT("VaesenInventory", "PlayerInventory", "Inventory");
	Inventory->Capacity = 24;
	Inventory->bAccessibleByOthers = false;
	bReplicateUsingRegisteredSubObjectList = true;

	// Movement feel (UE Third Person template values; tune in BP_VaesenPlayerCharacter).
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->MaxWalkSpeed = 500.f;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;
	Movement->JumpZVelocity = 700.f;
	Movement->AirControl = 0.35f;
}

void AVaesenPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// The owner switches view mode itself; everyone else learns it from the server.
	DOREPLIFETIME_CONDITION(AVaesenPlayerCharacter, bFirstPerson, COND_SkipOwner);
}

void AVaesenPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	DesiredCameraDistance = FMath::Clamp(CameraBoom->TargetArmLength, MinThirdPersonDistance, MaxCameraDistance);
	CameraBoom->TargetArmLength = DesiredCameraDistance;

	// Only matters while this camera is the view, i.e. for this character's own player.
	if (InteractionOutlineMaterial)
	{
		FollowCamera->PostProcessSettings.AddBlendable(InteractionOutlineMaterial, 1.0f);
	}
}

void AVaesenPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVaesenPlayerCharacter::Move);
	}
	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AVaesenPlayerCharacter::Look);
	}
	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (ZoomAction)
	{
		EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AVaesenPlayerCharacter::Zoom);
	}
	if (InteractAction)
	{
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::BeginInteract);
		EnhancedInput->BindAction(InteractAction, ETriggerEvent::Completed, this, &AVaesenPlayerCharacter::EndInteract);
	}
	if (InventoryAction)
	{
		EnhancedInput->BindAction(InventoryAction, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::ToggleInventory);
	}
}

void AVaesenPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (!Controller)
	{
		return;
	}

	const FVector2D Input = Value.Get<FVector2D>();
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void AVaesenPlayerCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void AVaesenPlayerCharacter::BeginInteract()
{
	Interactor->BeginInteract();
}

void AVaesenPlayerCharacter::EndInteract()
{
	Interactor->EndInteract();
}

void AVaesenPlayerCharacter::ToggleInventory()
{
	if (AVaesenPlayerController* PlayerController = Cast<AVaesenPlayerController>(GetController()))
	{
		PlayerController->ToggleInventory();
	}
}

void AVaesenPlayerCharacter::Zoom(const FInputActionValue& Value)
{
	const float Steps = Value.Get<float>();
	if (FMath::IsNearlyZero(Steps))
	{
		return;
	}

	if (Steps > 0.0f)
	{
		// Zooming in: down to the closest third-person distance, then on into the head.
		DesiredCameraDistance = DesiredCameraDistance <= MinThirdPersonDistance + KINDA_SMALL_NUMBER
			? 0.0f
			: FMath::Max(MinThirdPersonDistance, DesiredCameraDistance - ZoomStep * Steps);
		return;
	}

	// Zooming out: first person comes back out to the closest third-person distance.
	if (bFirstPerson || DesiredCameraDistance <= 0.0f)
	{
		SetFirstPerson(false);
		DesiredCameraDistance = MinThirdPersonDistance;
		return;
	}
	DesiredCameraDistance = FMath::Min(MaxCameraDistance, DesiredCameraDistance - ZoomStep * Steps);
}

void AVaesenPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, DesiredCameraDistance, DeltaSeconds, ZoomInterpSpeed);

	// Gliding into the head: switch to first person once the camera is nearly there.
	if (!bFirstPerson && DesiredCameraDistance <= 0.0f && CameraBoom->TargetArmLength <= FirstPersonSwitchDistance)
	{
		CameraBoom->TargetArmLength = 0.0f;
		SetFirstPerson(true);
	}

	if (bFirstPerson)
	{
		FollowCamera->SetWorldLocation(GetFirstPersonEyeLocation(DeltaSeconds));
	}
}

FVector AVaesenPlayerCharacter::GetFirstPersonEyeLocation(float DeltaSeconds)
{
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const bool bOnGround = Movement->IsMovingOnGround();
	const float Speed = GetVelocity().Size2D();

	// Bob grows with speed (walking = 1, running more), and fades out in the air.
	const float WalkSpeed = FMath::Max(Movement->MaxWalkSpeed, 1.0f);
	const float TargetWeight = bOnGround ? FMath::Clamp(Speed / WalkSpeed, 0.0f, 1.5f) : 0.0f;
	BobWeight = FMath::FInterpTo(BobWeight, TargetWeight, DeltaSeconds, 6.0f);
	if (bOnGround)
	{
		BobPhase = FMath::Fmod(BobPhase + DeltaSeconds * UE_TWO_PI * Speed / StrideLength, UE_TWO_PI);
	}

	// Two dips per stride (one per step) and one sway left-right per stride.
	const float Bob = FMath::Sin(2.0f * BobPhase) * BobHeight * BobWeight;
	const float Sway = FMath::Sin(BobPhase) * BobSway * BobWeight;

	// Slow breathing, only while standing still.
	const float Time = GetWorld()->GetTimeSeconds();
	const float Breathing = FMath::Sin(Time * UE_TWO_PI * BreathingRate) * BreathingHeight * (1.0f - FMath::Min(BobWeight, 1.0f));

	// Landing: remember how fast we fell, dip in Landed(), ease back up.
	if (Movement->IsFalling())
	{
		LastFallSpeed = FMath::Max(0.0f, -GetVelocity().Z);
	}
	LandingOffset = FMath::FInterpTo(LandingOffset, 0.0f, DeltaSeconds, 5.0f);

	const FVector TargetOffset = (GetActorRightVector() * Sway + FVector::UpVector * (Bob + Breathing + LandingOffset)) * GetCameraMotionScale();
	MotionOffset = FMath::VInterpTo(MotionOffset, TargetOffset, DeltaSeconds, MotionSmoothing);

	return GetActorLocation()
		+ FVector::UpVector * FirstPersonEyeHeight
		+ GetActorForwardVector() * FirstPersonForwardOffset
		+ MotionOffset;
}

void AVaesenPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// Small hops don't dip; a long fall dips up to LandingDip.
	constexpr float NoDipBelowSpeed = 300.0f;
	constexpr float FullDipAtSpeed = 1200.0f;
	const float Alpha = FMath::Clamp((LastFallSpeed - NoDipBelowSpeed) / (FullDipAtSpeed - NoDipBelowSpeed), 0.0f, 1.0f);
	LandingOffset = -LandingDip * Alpha;
	LastFallSpeed = 0.0f;
}

namespace VaesenCameraSettings
{
	const TCHAR* Section = TEXT("Vaesen.Camera");
	const TCHAR* MotionScaleKey = TEXT("MotionScale");
}

void AVaesenPlayerCharacter::SetCameraMotionScale(float Scale)
{
	GConfig->SetFloat(VaesenCameraSettings::Section, VaesenCameraSettings::MotionScaleKey, FMath::Clamp(Scale, 0.0f, 1.0f), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

float AVaesenPlayerCharacter::GetCameraMotionScale()
{
	float Scale = 1.0f;
	GConfig->GetFloat(VaesenCameraSettings::Section, VaesenCameraSettings::MotionScaleKey, Scale, GGameUserSettingsIni);
	return FMath::Clamp(Scale, 0.0f, 1.0f);
}

void AVaesenPlayerCharacter::SetFirstPerson(bool bInFirstPerson)
{
	if (bFirstPerson == bInFirstPerson)
	{
		return;
	}

	bFirstPerson = bInFirstPerson;
	ApplyViewMode();
	if (!HasAuthority())
	{
		ServerSetFirstPerson(bInFirstPerson);
	}
}

void AVaesenPlayerCharacter::ServerSetFirstPerson_Implementation(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	ApplyViewMode();
}

void AVaesenPlayerCharacter::OnRep_FirstPerson()
{
	ApplyViewMode();
}

void AVaesenPlayerCharacter::ApplyViewMode()
{
	// Everyone (owner, server, other players): in first person the body turns with the view, otherwise it turns
	// toward where it walks. Must match on owner and server for movement to replicate smoothly.
	bUseControllerRotationYaw = bFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bFirstPerson;

	if (!IsLocallyControlled())
	{
		return;
	}

	// Owner only: in first person the head isn't drawn for this camera (the torso's neck is closed, so there's no
	// hole) but keeps casting its shadow. Its depth must go too: the camera sits inside the head, and depth alone
	// would black out the view. Everyone else keeps seeing the head.
	if (HeadMesh)
	{
		HeadMesh->SetRenderInMainPass(!bFirstPerson);
		HeadMesh->SetRenderInDepthPass(!bFirstPerson);
	}
	if (!bFirstPerson)
	{
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
	}
	FollowCamera->bUsePawnControlRotation = bFirstPerson;

	// Limit looking down in first person; the camera manager applies it to the view (and the control rotation).
	const APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager.Get() : nullptr)
	{
		CameraManager->ViewPitchMin = bFirstPerson ? FirstPersonMinPitch : GetDefault<APlayerCameraManager>()->ViewPitchMin;
	}
}
