#include "Characters/VaesenPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
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

	// First person: the eyes ride on the head bone, so the view moves with the body (walking, jumping).
	if (bFirstPerson && GetMesh()->DoesSocketExist(HeadBoneName))
	{
		const FVector Eyes = GetMesh()->GetSocketLocation(HeadBoneName)
			+ GetActorForwardVector() * FirstPersonForwardOffset
			+ FVector::UpVector * FirstPersonUpOffset;
		FollowCamera->SetWorldLocation(Eyes);
	}
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

	// Only the owner loses the head (so it doesn't fill the view); everyone else still sees it.
	if (bFirstPerson)
	{
		GetMesh()->HideBoneByName(HeadBoneName, EPhysBodyOp::PBO_None);
	}
	else
	{
		GetMesh()->UnHideBoneByName(HeadBoneName);
		FollowCamera->SetRelativeLocation(FVector::ZeroVector);
	}
	FollowCamera->bUsePawnControlRotation = bFirstPerson;
}
