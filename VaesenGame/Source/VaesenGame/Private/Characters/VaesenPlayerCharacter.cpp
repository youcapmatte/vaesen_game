#include "Characters/VaesenPlayerCharacter.h"

#include "Anim/VaesenAnimInstance.h"
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
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Inventory/VaesenQuickSlotComponent.h"
#include "Player/VaesenPlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenGameplayTags.h"
#include "Items/VaesenRangedWeaponItem.h"

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

	QuickSlots = CreateDefaultSubobject<UVaesenQuickSlotComponent>(TEXT("QuickSlots"));

	// Players dress piece by piece; full-body armor is for NPCs.
	Equipment->bAllowFullBodyArmor = false;

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
	DefaultFieldOfView = FollowCamera->FieldOfView;
	DefaultSocketOffset = CameraBoom->SocketOffset;

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
	const auto BindAbility = [this, EnhancedInput](UInputAction* Action, EVaesenAbilityInput Input)
	{
		if (Action)
		{
			EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::PressAbility, Input);
			EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, &AVaesenPlayerCharacter::ReleaseAbility, Input);
			EnhancedInput->BindAction(Action, ETriggerEvent::Canceled, this, &AVaesenPlayerCharacter::ReleaseAbility, Input);
		}
	};
	BindAbility(AttackAction, EVaesenAbilityInput::Attack);
	BindAbility(HeavyAttackAction, EVaesenAbilityInput::HeavyAttack);
	BindAbility(AimOrBlockAction, EVaesenAbilityInput::AimOrBlock);
	BindAbility(ReloadAction, EVaesenAbilityInput::Reload);
	BindAbility(ThrowAction, EVaesenAbilityInput::Throw);
	BindAbility(HolsterAction, EVaesenAbilityInput::Holster);
	BindAbility(SwapWeaponAction, EVaesenAbilityInput::SwapWeapon);
	BindAbility(DodgeAction, EVaesenAbilityInput::Dodge);
	BindAbility(RangedGuardAction, EVaesenAbilityInput::RangedGuard);
	if (CrouchAction)
	{
		EnhancedInput->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::ToggleCrouch);
	}
	if (SwitchGripAction)
	{
		EnhancedInput->BindAction(SwitchGripAction, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::ToggleGrip);
	}
	if (LockOnAction)
	{
		EnhancedInput->BindAction(LockOnAction, ETriggerEvent::Started, this, &AVaesenBaseCharacter::ToggleTargetLock);
	}
	if (TriggerAttackAction)
	{
		EnhancedInput->BindAction(TriggerAttackAction, ETriggerEvent::Started, this, &AVaesenPlayerCharacter::PressTriggerAttack);
		EnhancedInput->BindAction(TriggerAttackAction, ETriggerEvent::Completed, this, &AVaesenPlayerCharacter::ReleaseTriggerAttack);
	}
	for (int32 Index = 0; Index < QuickSlotActions.Num(); ++Index)
	{
		if (QuickSlotActions[Index])
		{
			EnhancedInput->BindAction(QuickSlotActions[Index], ETriggerEvent::Started, this, &AVaesenPlayerCharacter::UseQuickSlot, Index);
		}
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
	// Hands free for doors, chests and people.
	Equipment->RequestSheathe();
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

void AVaesenPlayerCharacter::UseQuickSlot(int32 Index)
{
	QuickSlots->RequestUse(Index);
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

void AVaesenPlayerCharacter::AddCameraKick(float Degrees, float Recovery)
{
	CameraKickTime = GetWorld()->GetTimeSeconds();
	CameraKickDegrees = Degrees;
	CameraKickRecovery = Recovery;
}

void AVaesenPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocallyControlled())
	{
		return;
	}

	UpdateAimCamera(DeltaSeconds);

	// A shot's kick, up and settling with the weapon's.
	const float Kick = UVaesenAnimInstance::KickAt(static_cast<float>(GetWorld()->GetTimeSeconds() - CameraKickTime), CameraKickRecovery);
	FollowCamera->SetRelativeRotation(FRotator(CameraKickDegrees * Kick, 0.0, 0.0));

	// Aiming pulls the camera in over the shoulder (third person only).
	const float WantedDistance = DesiredCameraDistance > 0.0f
		? FMath::Lerp(DesiredCameraDistance, FMath::Min(DesiredCameraDistance, AimCameraDistance), AimAlpha)
		: DesiredCameraDistance;
	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, WantedDistance, DeltaSeconds, ZoomInterpSpeed);

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
	UpdateRotationMode();

	if (!IsLocallyControlled())
	{
		return;
	}

	// Owner only: in first person the head, and whatever is worn over it, isn't drawn for this camera (the torso's
	// neck is closed, so there's no hole) but keeps casting its shadow. Its depth must go too: the camera sits inside
	// the head, and depth alone would black out the view. Everyone else keeps seeing the head.
	for (USkeletalMeshComponent* HeadLayer : { HeadMesh.Get(), HeadOverMesh.Get() })
	{
		if (HeadLayer)
		{
			HeadLayer->SetRenderInMainPass(!bFirstPerson);
			HeadLayer->SetRenderInDepthPass(!bFirstPerson);
		}
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

// --- Combat input and the aiming camera ---

void AVaesenPlayerCharacter::PressAbility(EVaesenAbilityInput Input)
{
	if (AbilitySystem)
	{
		AbilitySystem->PressAbilityInput(Input);
	}
}

void AVaesenPlayerCharacter::ReleaseAbility(EVaesenAbilityInput Input)
{
	if (AbilitySystem)
	{
		AbilitySystem->ReleaseAbilityInput(Input);
	}
}

void AVaesenPlayerCharacter::PressTriggerAttack()
{
	// The right trigger fires a gun, and is the heavy attack with anything else.
	const UVaesenWeaponItem* Weapon = Equipment->GetDrawnWeapon();
	TriggerAttackInput = Weapon && Weapon->IsRanged() ? EVaesenAbilityInput::Attack : EVaesenAbilityInput::HeavyAttack;
	PressAbility(TriggerAttackInput);
}

void AVaesenPlayerCharacter::ReleaseTriggerAttack()
{
	ReleaseAbility(TriggerAttackInput);
	TriggerAttackInput = EVaesenAbilityInput::None;
}

void AVaesenPlayerCharacter::UpdateAimCamera(float DeltaSeconds)
{
	// (Not a bow nocked but held low: it isn't aimed.)
	const bool bAiming = AbilitySystem && (AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Aiming)
		|| AbilitySystem->HasMatchingGameplayTag(VaesenTags::State_Throwing));
	AimAlpha = FMath::FInterpTo(AimAlpha, bAiming ? 1.0f : 0.0f, DeltaSeconds, AimInterpSpeed);

	const UVaesenRangedWeaponItem* Ranged = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
	const float AimFieldOfView = Ranged ? Ranged->AimFieldOfView : DefaultAimFieldOfView;
	FollowCamera->SetFieldOfView(FMath::Lerp(DefaultFieldOfView, AimFieldOfView, AimAlpha));
	if (!bFirstPerson)
	{
		CameraBoom->SocketOffset = FMath::Lerp(DefaultSocketOffset, AimSocketOffset, AimAlpha);
	}
}

void AVaesenPlayerCharacter::ToggleCrouch()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void AVaesenPlayerCharacter::ToggleGrip()
{
	if (UVaesenEquipmentComponent* Gear = GetEquipment())
	{
		Gear->RequestToggleGrip();
	}
}
