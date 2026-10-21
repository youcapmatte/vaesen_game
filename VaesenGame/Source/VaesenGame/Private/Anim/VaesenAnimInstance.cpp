#include "Anim/VaesenAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Anim/VaesenWeaponFitRig.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenGameplayTags.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "KismetAnimationLibrary.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"

const FName UVaesenAnimInstance::HandIKSheathCurve(TEXT("HandIK_Sheath"));
const FName UVaesenAnimInstance::TriggerFingerCurve(TEXT("TriggerFinger"));
const FName UVaesenAnimInstance::LeftHandGripSocket(TEXT("hand_l_weapon"));
const FName UVaesenAnimInstance::ShoulderPocketSocket(TEXT("ShoulderPocket"));
const FName UVaesenAnimInstance::EyeSocket(TEXT("Eye_R"));

bool FVaesenHeldWeapon::GetLocalSocketTransform(FName Socket, FTransform& OutTransform) const
{
	if (Mesh && Grip && Mesh->DoesSocketExist(Socket))
	{
		OutTransform = Mesh->GetSocketTransform(Socket).GetRelativeTransform(Grip->GetComponentTransform());
		OutTransform.SetScale3D(FVector::OneVector);
		return true;
	}
	OutTransform = FTransform::Identity;
	return false;
}

bool FVaesenHeldWeapon::GetLocalSocket(FName Socket, FVector& OutLocation, const FVector& Fallback) const
{
	if (Mesh && Grip && Mesh->DoesSocketExist(Socket))
	{
		OutLocation = Grip->GetComponentTransform().InverseTransformPosition(Mesh->GetSocketLocation(Socket));
		return true;
	}
	OutLocation = Fallback;
	return false;
}

void UVaesenAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// Posed by a character in the game, or by the Weapon Fit rig in the editor.
	const AVaesenBaseCharacter* Character = Cast<AVaesenBaseCharacter>(TryGetPawnOwner());
	const AVaesenWeaponFitRig* Rig = Character ? nullptr : Cast<AVaesenWeaponFitRig>(GetOwningActor());
	const USkeletalMeshComponent* Body = GetOwningComponent();
	if ((!Character && !Rig) || !Body)
	{
		return;
	}

	const UVaesenWeaponItem* Weapon = nullptr;
	FVaesenHeldWeapon Held;
	FRotator Aim;
	bool bShouldering = false;
	bool bHandsFree = true;
	bool bInHand = false;
	const UVaesenEquipmentComponent* Equipment = nullptr;
	const AVaesenWeaponActor* WeaponActor = nullptr;

	if (Character)
	{
		const FVector Velocity = Character->GetVelocity();
		Speed = Velocity.Size2D();
		Direction = Speed > 1.0f ? UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation()) : 0.0f;
		bStrafing = !Character->GetCharacterMovement()->bOrientRotationToMovement;
		bCrouching = Character->bIsCrouched;
		bFalling = Character->GetCharacterMovement()->IsFalling();

		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		const auto HasTag = [ASC](const FGameplayTag& Tag) { return ASC && ASC->HasMatchingGameplayTag(Tag); };
		bAiming = HasTag(VaesenTags::State_Aiming) || HasTag(VaesenTags::State_DrawingBow) || HasTag(VaesenTags::State_Throwing);
		bBlocking = HasTag(VaesenTags::State_Blocking);
		bReloading = HasTag(VaesenTags::State_Reloading) || HasTag(VaesenTags::State_Cycling);
		bStaggered = HasTag(VaesenTags::State_Staggered);
		bShouldering = HasTag(VaesenTags::State_Aiming);
		// Not while the hands are busy with something else (reloading, drawing or sheathing).
		bHandsFree = !bReloading && !HasTag(VaesenTags::State_SwitchingWeapon);
		Aim = Character->GetBaseAimRotation();

		Equipment = Character->GetEquipment();
		Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
		WeaponActor = Weapon ? Weapon->GetWeaponActor() : nullptr;
		bInHand = WeaponActor && WeaponActor->IsInHand();
		Held = { WeaponActor ? WeaponActor->GetRootComponent() : nullptr, WeaponActor ? WeaponActor->GetWeaponMesh() : nullptr,
			WeaponActor ? &WeaponActor->Hold : nullptr };
	}
	else
	{
		Speed = Rig->Speed;
		Direction = Rig->Direction;
		bStrafing = true;
		bCrouching = Rig->IsCrouchPose();
		bAiming = bShouldering = Rig->IsAimPose();
		bFalling = bBlocking = bReloading = bStaggered = false;
		Aim = Rig->GetAim();
		Weapon = Rig->GetWeaponDefaults();
		Held = Rig->GetHeldWeapon();
		bInHand = Held.IsValid();
	}

	// Aim offset: the view relative to the body.
	const FRotator Delta = (Aim - GetOwningActor()->GetActorRotation()).GetNormalized();
	AimPitch = FMath::Clamp(Delta.Pitch, -90.0f, 90.0f);
	AimYaw = FMath::Clamp(Delta.Yaw, -120.0f, 120.0f);

	bWeaponDrawn = Weapon != nullptr;
	WeaponCategory = Weapon ? Weapon->Category : EVaesenWeaponCategory::Unarmed;
	bTwoHanded = Weapon && Weapon->bTwoHanded;
	LocomotionDatabase = UVaesenWeaponCategoryDefaults::Get()->GetCategory(WeaponCategory).LocomotionDatabase.Get();

	const AVaesenRangedWeaponActor* Ranged = Cast<AVaesenRangedWeaponActor>(WeaponActor);
	BowDrawAlpha = Ranged ? Ranged->GetDrawAlpha() : 0.0f;

	// Shouldered while aiming two-handed guns (not bows: they have their own draw). The Shoulder Aim node in the
	// graph does the shouldering on the frame's pose; this gathers what it needs.
	const bool bShoulderAim = bShouldering && bTwoHanded && bInHand && bHandsFree && Held.IsValid() && Held.Hold->bShoulderAim
		&& WeaponCategory != EVaesenWeaponCategory::Bow;
	const FTransform& Component = Body->GetComponentTransform();
	const FTransform WeaponInRightHand = Held.Grip ? Held.Grip->GetComponentTransform().GetRelativeTransform(Body->GetSocketTransform(TEXT("hand_r")))
		: FTransform::Identity;

	// Off hand on the fore-grip of two-handed weapons: its palm (hand_l_weapon) on the LeftGrip socket, turned as the
	// socket is. LeftHandIK (older meshes) gives only the place.
	FTransform LeftGrip;
	FVector LeftHandIKOnly;
	const bool bLeftGrip = Held.IsValid() && Held.GetLocalSocketTransform(TEXT("LeftGrip"), LeftGrip);
	const bool bLeftPlace = bLeftGrip || (Held.IsValid() && Held.GetLocalSocket(TEXT("LeftHandIK"), LeftHandIKOnly));
	const bool bLeftHolds = bTwoHanded && bInHand && bHandsFree && bLeftPlace;
	FTransform HandInWeapon;
	if (bLeftHolds)
	{
		if (!bLeftGrip)
		{
			LeftGrip = FTransform(LeftHandIKOnly);
		}
		LeftGrip.AddToTranslation(Held.Hold->LeftHandOffset);
		const USkeletalMeshSocket* Palm = bLeftGrip ? Body->GetSocketByName(LeftHandGripSocket) : nullptr;
		// The hand bone, in the weapon's space: the palm socket put on the grip.
		HandInWeapon = Palm ? Palm->GetSocketLocalTransform().Inverse() * LeftGrip : LeftGrip;
		const FTransform LeftHand = HandInWeapon * Held.Grip->GetComponentTransform();
		LeftHandIKTransform = LeftHand;
		LeftHandIKLocation = LeftHand.GetLocation();
		LeftHandIKRotation = LeftHand.GetRelativeTransform(Component).Rotator();
		// Relative to the right hand, which holds the weapon rigidly: right however the arm moves this frame.
		LeftHandIKInHand = (HandInWeapon * WeaponInRightHand).GetLocation();
	}

	if (bShoulderAim)
	{
		// The player's own view: the barrel converges on what the crosshair is on (30 m out), not parallel to the camera.
		const APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager)
		{
			const FVector Target = Controller->PlayerCameraManager->GetCameraLocation() + Controller->PlayerCameraManager->GetCameraRotation().Vector() * 3000.0f;
			const FVector Eye = EyePoint(Body) + Aim.RotateVector(Held.Hold->AimSightOffset);
			Aim = (Target - Eye).Rotation();
		}
		Aim.Roll = 0.0f;

		const FVaesenWeaponHold& Hold = *Held.Hold;
		FVaesenShoulderAimInput& In = ShoulderAimInput;
		In.Aim = Component.GetRotation().Inverse() * Aim.Quaternion();
		In.WeaponInHand = WeaponInRightHand;
		FTransform Sight(FVector(0.0, 0.0, 6.0));
		Held.GetLocalSocketTransform(TEXT("Sight"), Sight);
		In.SightLocation = Sight.GetLocation();
		In.SightRotation = Sight.GetRotation();
		In.bHasStock = Held.GetLocalSocket(TEXT("Stock"), In.StockLocation);
		In.bLeftHand = bLeftHolds;
		In.LeftHandInWeapon = HandInWeapon;
		const USkeletalMeshSocket* Pocket = Body->GetSocketByName(ShoulderPocketSocket);
		In.bHasPocketSocket = Pocket && Pocket->BoneName == TEXT("clavicle_r");
		In.PocketOnClavicle = In.bHasPocketSocket ? Pocket->GetSocketLocalTransform() : FTransform::Identity;
		In.EyeOffset = Hold.AimSightOffset;
		const USkeletalMeshSocket* EyeOnHead = Body->GetSocketByName(EyeSocket);
		In.bHasEyeSocket = EyeOnHead && EyeOnHead->BoneName == TEXT("head");
		In.EyeOnHead = In.bHasEyeSocket ? EyeOnHead->GetSocketLocalTransform() : FTransform::Identity;
		In.ShoulderOffset = Hold.ShoulderOffset;
		In.AimRotation = Hold.AimRotation.Quaternion();
		In.ShoulderShare = Hold.ShoulderShare;
		In.RecoilBack = Hold.RecoilBack;
		In.RecoilPitch = Hold.RecoilPitch;
	}
	ShoulderAimInput.Kick = RecoilKick;
	const float BlendSpeed = Held.Hold ? Held.Hold->AimBlendSpeed : 12.0f;
	AimIKAlpha = Rig ? (bShoulderAim ? 1.0f : 0.0f) : FMath::FInterpTo(AimIKAlpha, bShoulderAim ? 1.0f : 0.0f, DeltaSeconds, BlendSpeed);
	if (!bInHand)
	{
		AimIKAlpha = 0.0f;
	}

	// The fingers closed round the grips, of each hand that holds the weapon.
	GripPose = UVaesenWeaponCategoryDefaults::Get()->GetCategory(WeaponCategory).GripPose;
	RightFingerGrip = GripPose && bInHand && bHandsFree ? 1.0f : 0.0f;
	LeftFingerGrip = GripPose && bLeftHolds ? 1.0f : 0.0f;
	// While shouldered the node holds the left hand; these IK nodes take over as it lets go.
	LeftHandIKAlpha = bLeftHolds ? 1.0f - AimIKAlpha : 0.0f;
	LeftHandRotationAlpha = bLeftGrip ? LeftHandIKAlpha : 0.0f;
	RecoilKick = FMath::FInterpTo(RecoilKick, 0.0f, DeltaSeconds, 10.0f);

	// Drawing or sheathing: the hand reaches for the actual grip of the weapon in its sheath.
	SheathHandIKAlpha = FMath::Clamp(GetCurveValue(HandIKSheathCurve), 0.0f, 1.0f);
	if (SheathHandIKAlpha > 0.0f && Equipment)
	{
		if (!SheathTarget.IsValid())
		{
			// The weapon being drawn or sheathed. Its origin is its grip; remember where that is relative to the body
			// when the reach begins: at the grab the weapon moves to the hand, and the hand must keep reaching for
			// where it was, not chase it.
			if (AVaesenWeaponActor* Found = Equipment->GetSwitchingWeaponActor())
			{
				SheathTarget = Found;
				SheathTargetInComponent = Found->GetSheathedTransform().GetRelativeTransform(Component);
				bSheathIKLeftHand = Found->GetWeaponItem() && Found->GetWeaponItem()->IsHeldInOffHand();
			}
		}
		if (SheathTarget.IsValid())
		{
			SheathHandIKTransform = SheathTargetInComponent * Component;
			SheathHandIKLocation = SheathHandIKTransform.GetLocation();
		}
		else
		{
			SheathHandIKAlpha = 0.0f;
		}
	}
	else
	{
		SheathTarget.Reset();
		SheathHandIKAlpha = 0.0f;
	}

	TriggerPulse = FMath::FInterpTo(TriggerPulse, 0.0f, DeltaSeconds, 12.0f);
	TriggerCurl = FMath::Max(TriggerPulse, GetCurveValue(TriggerFingerCurve));
}

FVector UVaesenAnimInstance::ShoulderPocket(const USkeletalMeshComponent* Body, const FRotator& Aim)
{
	if (Body->DoesSocketExist(ShoulderPocketSocket))
	{
		return Body->GetSocketLocation(ShoulderPocketSocket);
	}
	// In front of the shoulder joint and a little inside it, toward the chest.
	return Body->GetSocketLocation(TEXT("upperarm_r")) + Aim.RotateVector(FVector(6.0, -5.0, 0.0));
}

FVector UVaesenAnimInstance::EyePoint(const USkeletalMeshComponent* Body)
{
	if (Body->DoesSocketExist(EyeSocket))
	{
		return Body->GetSocketLocation(EyeSocket);
	}
	// Manny's mesh space: forward +Y, right -X, up Z.
	return Body->GetSocketLocation(TEXT("head")) + Body->GetComponentTransform().TransformVector(FVector(-3.0, 9.0, 9.0));
}

bool UVaesenAnimInstance::GetShoulderAimInput(FVaesenShoulderAimInput& Out) const
{
	Out = ShoulderAimInput;
	return AimIKAlpha > 0.0f;
}
