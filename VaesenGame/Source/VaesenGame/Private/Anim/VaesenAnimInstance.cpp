#include "Anim/VaesenAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Anim/VaesenWeaponFitRig.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "KismetAnimationLibrary.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"

const FName UVaesenAnimInstance::HandIKSheathCurve(TEXT("HandIK_Sheath"));
const FName UVaesenAnimInstance::TriggerFingerCurve(TEXT("TriggerFinger"));

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

	// Off hand on the fore-grip of two-handed weapons.
	FVector LeftHandLocal;
	LeftHandIKAlpha = bTwoHanded && bInHand && bHandsFree && Held.IsValid() && Held.GetLocalSocket(TEXT("LeftHandIK"), LeftHandLocal) ? 1.0f : 0.0f;
	if (LeftHandIKAlpha > 0.0f)
	{
		const FVector LeftHand = Held.Grip->GetComponentTransform().TransformPosition(LeftHandLocal + Held.Hold->LeftHandOffset);
		LeftHandIKTransform = FTransform(Held.Grip->GetComponentQuat(), LeftHand);
		LeftHandIKLocation = LeftHand;
		// Relative to the right hand, which holds the weapon rigidly: right however the arm moves this frame.
		LeftHandIKInHand = Body->GetSocketTransform(TEXT("hand_r")).InverseTransformPosition(LeftHand);
	}

	// Shouldered while aiming two-handed guns (not bows: they have their own draw).
	const bool bShoulderAim = bShouldering && bTwoHanded && bInHand && bHandsFree && Held.IsValid() && Held.Hold->bShoulderAim
		&& WeaponCategory != EVaesenWeaponCategory::Bow;
	const FTransform& Component = Body->GetComponentTransform();
	if (bShoulderAim)
	{
		// The player's own view: the barrel converges on what the crosshair is on (30 m out), not parallel to the camera.
		const APlayerController* Controller = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
		if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager)
		{
			const FVector Target = Controller->PlayerCameraManager->GetCameraLocation() + Controller->PlayerCameraManager->GetCameraRotation().Vector() * 3000.0f;
			const FVector Sight = Body->GetSocketLocation(TEXT("head")) + Aim.RotateVector(Held.Hold->AimSightOffset);
			Aim = (Target - Sight).Rotation();
		}

		// The hand holds the weapon rigidly: wherever the weapon goes, the hand goes the same. Kept relative to the
		// body, so it eases out from where the weapon was even while walking.
		const FTransform Placed = PlaceShouldered(Body, Held, Aim, RecoilKick);
		const FTransform WeaponInHand = Held.Grip->GetComponentTransform().GetRelativeTransform(Body->GetSocketTransform(TEXT("hand_r")));
		AimHandInComponent = (WeaponInHand.Inverse() * Placed).GetRelativeTransform(Component);
	}
	const float BlendSpeed = Held.Hold ? Held.Hold->AimBlendSpeed : 12.0f;
	AimIKAlpha = Rig ? (bShoulderAim ? 1.0f : 0.0f) : FMath::FInterpTo(AimIKAlpha, bShoulderAim ? 1.0f : 0.0f, DeltaSeconds, BlendSpeed);
	if (!bInHand)
	{
		AimIKAlpha = 0.0f;
	}
	const FTransform AimHandWorld = AimHandInComponent * Component;
	AimHandLocation = AimHandWorld.GetLocation();
	AimHandRotation = AimHandWorld.Rotator();
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

FTransform UVaesenAnimInstance::PlaceShouldered(const USkeletalMeshComponent* Body, const FVaesenHeldWeapon& Weapon, const FRotator& InAim, float Kick)
{
	const FVaesenWeaponHold& Hold = *Weapon.Hold;
	FRotator Aim = InAim;
	Aim.Roll = 0.0f;
	const FVector Along = Aim.Vector();

	// The sight point in front of the eye, pushed back by the kick of a shot (and the muzzle up).
	const FVector Sight = Body->GetSocketLocation(TEXT("head")) + Aim.RotateVector(Hold.AimSightOffset) - Along * Hold.RecoilBack * Kick;
	const FQuat Rotation = Aim.Quaternion() * Hold.AimRotation.Quaternion() * FRotator(Hold.RecoilPitch * Kick, 0.0, 0.0).Quaternion();

	// The weapon laid there: its Sight socket on the point (no Sight: a little over the grip).
	FVector SightLocal, StockLocal;
	Weapon.GetLocalSocket(TEXT("Sight"), SightLocal, FVector(0.0, 0.0, 6.0));
	FVector Location = Sight - Rotation.RotateVector(SightLocal);

	// With a Stock socket, slid along the aim so the stock meets the shoulder.
	if (Weapon.GetLocalSocket(TEXT("Stock"), StockLocal))
	{
		const FVector Stock = Location + Rotation.RotateVector(StockLocal);
		Location += Along * (Hold.StockToShoulder - FVector::DotProduct(Stock - Body->GetSocketLocation(TEXT("upperarm_r")), Along));
	}
	return FTransform(Rotation, Location);
}

void UVaesenAnimInstance::ShoulderHoldFromPlacement(const USkeletalMeshComponent* Body, const FVaesenHeldWeapon& Weapon, const FRotator& InAim,
	const FTransform& Placed, FVaesenWeaponHold& InOutHold)
{
	FRotator Aim = InAim;
	Aim.Roll = 0.0f;
	InOutHold.AimRotation = (Aim.Quaternion().Inverse() * Placed.GetRotation()).Rotator();

	FVector SightLocal, StockLocal;
	Weapon.GetLocalSocket(TEXT("Sight"), SightLocal, FVector(0.0, 0.0, 6.0));
	const FVector Sight = Placed.GetLocation() + Placed.GetRotation().RotateVector(SightLocal);
	InOutHold.AimSightOffset = Aim.UnrotateVector(Sight - Body->GetSocketLocation(TEXT("head")));
	if (Weapon.GetLocalSocket(TEXT("Stock"), StockLocal))
	{
		const FVector Stock = Placed.GetLocation() + Placed.GetRotation().RotateVector(StockLocal);
		InOutHold.StockToShoulder = FVector::DotProduct(Stock - Body->GetSocketLocation(TEXT("upperarm_r")), Aim.Vector());
	}
}
