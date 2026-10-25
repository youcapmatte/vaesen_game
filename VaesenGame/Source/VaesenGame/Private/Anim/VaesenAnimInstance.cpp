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
#include "Items/VaesenRangedWeaponItem.h"
#include "KismetAnimationLibrary.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"
#include "HAL/IConsoleManager.h"

namespace
{
	// Off for now: the plain run's own wrist, untouched.
	TAutoConsoleVariable<int32> CVarCarryTurn(TEXT("vaesen.anim.carryturn"), 0,
		TEXT("1: at a free run, the wrist turns a carried one-handed blade forward, down and out, clear of the body."));
}

const FName UVaesenAnimInstance::HandIKSheathCurve(TEXT("HandIK_Sheath"));
const FName UVaesenAnimInstance::TriggerFingerCurve(TEXT("TriggerFinger"));
const FName UVaesenAnimInstance::BowArrowCurve(TEXT("BowArrow"));
const FName UVaesenAnimInstance::BowArrowTransformCurves[6] = { TEXT("BowArrowX"), TEXT("BowArrowY"), TEXT("BowArrowZ"), TEXT("BowArrowPitch"), TEXT("BowArrowYaw"), TEXT("BowArrowRoll") };
const FName UVaesenAnimInstance::BowStringCurves[3] = { TEXT("BowStringX"), TEXT("BowStringY"), TEXT("BowStringZ") };
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
	bool bCyclingNow = false;
	bool bGuardUp = false;
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
		bTargetLocked = Character->IsTargetLocked();

		const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
		const auto HasTag = [ASC](const FGameplayTag& Tag) { return ASC && ASC->HasMatchingGameplayTag(Tag); };
		bAiming = HasTag(VaesenTags::State_Aiming) || HasTag(VaesenTags::State_DrawingBow) || HasTag(VaesenTags::State_Throwing);
		bGuardUp = HasTag(VaesenTags::State_Blocking);
		// Not squared up to fight: weapon stances use the plain locomotion then, standing, running and jumping. The fighting
		// stance is for squaring up (aiming, a target lock); guarding without a lock keeps the plain stance.
		bRelaxed = !bStrafing || (bGuardUp && !bTargetLocked);
		bReloading = HasTag(VaesenTags::State_Reloading) || HasTag(VaesenTags::State_Cycling);
		bStaggered = HasTag(VaesenTags::State_Staggered);
		bShouldering = HasTag(VaesenTags::State_Aiming);
		// Not while the hands are busy with something else (reloading, drawing or sheathing). Working a bolt only takes
		// the right hand (below).
		bHandsFree = !HasTag(VaesenTags::State_Reloading) && !HasTag(VaesenTags::State_SwitchingWeapon);
		bCyclingNow = HasTag(VaesenTags::State_Cycling);
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
		bRelaxed = false;
		bTargetLocked = false;
		bCrouching = Rig->IsCrouchPose();
		bAiming = bShouldering = Rig->IsAimPose();
		bFalling = bGuardUp = bReloading = bStaggered = false;
		Aim = Rig->GetAim();
		Weapon = Rig->GetWeaponDefaults();
		Held = Rig->GetHeldWeapon();
		bInHand = Held.IsValid();
	}

	// Aim offset: the view relative to the body.
	const FRotator Delta = (Aim - GetOwningActor()->GetActorRotation()).GetNormalized();
	AimPitch = FMath::Clamp(Delta.Pitch, -90.0f, 90.0f);
	AimYaw = FMath::Clamp(Delta.Yaw, -120.0f, 120.0f);

	// The weapon stance holds an animated guard (bBlocking); a procedural one is the Shoulder Aim node's (below).
	bBlocking = bGuardUp && !(Weapon && Weapon->GetCategoryDefaults().bProceduralGuard);
	bWeaponDrawn = Weapon != nullptr;
	WeaponCategory = Weapon ? Weapon->Category : EVaesenWeaponCategory::Unarmed;
	// In both hands: always (rifles, bows) or held so now (a sword or an axe with the off hand free).
	bTwoHanded = Weapon && Weapon->IsHeldTwoHanded();
	LocomotionDatabase = Weapon ? Weapon->GetLocomotionDatabase() : UVaesenWeaponCategoryDefaults::Get()->GetCategory(WeaponCategory).LocomotionDatabase.Get();

	const AVaesenRangedWeaponActor* Ranged = Cast<AVaesenRangedWeaponActor>(WeaponActor);
	BowDrawAlpha = Ranged ? Ranged->GetDrawAlpha() : 0.0f;

	// Raised while aiming guns: two-handed into the shoulder, pistols at arm's length in one hand (not bows: they
	// have their own draw). The Shoulder Aim node in the graph does it on the frame's pose; this gathers what it needs.
	const bool bOneHandedGun = !bTwoHanded && Weapon && Weapon->IsRanged() && !Weapon->IsHeldInOffHand();
	const bool bShoulderAim = bShouldering && (bTwoHanded || bOneHandedGun) && bInHand && bHandsFree && Held.IsValid() && Held.Hold->bShoulderAim
		&& WeaponCategory != EVaesenWeaponCategory::Bow;
	const FTransform& Component = Body->GetComponentTransform();

	// Off hand on the fore-grip of two-handed weapons (and an aimed pistol): its palm (hand_l_weapon) on the LeftGrip
	// socket, turned as the socket is. LeftHandIK (older meshes) gives only the place.
	FTransform LeftGrip;
	FVector LeftHandIKOnly;
	const bool bLeftGrip = Held.IsValid() && Held.GetLocalSocketTransform(TEXT("LeftGrip"), LeftGrip);
	const bool bLeftPlace = bLeftGrip || (Held.IsValid() && Held.GetLocalSocket(TEXT("LeftHandIK"), LeftHandIKOnly));
	// A pistol takes the left hand too while aimed (a two-handed hold), where it has a LeftGrip. A bow is in the left hand.
	const bool bLeftHolds = (bTwoHanded || (bOneHandedGun && bShoulderAim)) && bInHand && bHandsFree && bLeftPlace && !(Weapon && Weapon->IsHeldInLeftHand());
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
		FTransform Sight(FVector(0.0, 0.0, 6.0));
		Held.GetLocalSocketTransform(TEXT("Sight"), Sight);
		In.SightLocation = Sight.GetLocation();
		In.SightRotation = Sight.GetRotation();
		In.bHasStock = Held.GetLocalSocket(TEXT("Stock"), In.StockLocation);
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
		In.RecoilBodyPitch = Hold.RecoilBodyPitch;
	}

	// Every frame, for the weapon bone (ik_hand_gun, which the weapon in the right hand rides on) and the hands.
	FVaesenShoulderAimInput& In = ShoulderAimInput;
	const USkeletalMeshSocket* PalmSocket = Body->GetSocketByName(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket);
	In.PalmOnHand = PalmSocket ? PalmSocket->GetSocketLocalTransform() : FTransform::Identity;
	In.bHeld = bInHand && Held.IsValid() && Weapon && !Weapon->IsHeldInLeftHand();
	In.GripOnWeaponBone = In.bHeld ? Held.Hold->InHand * Weapon->GetCategoryDefaults().InHand : FTransform::Identity;
	In.bShoulder = bShoulderAim || (In.bShoulder && AimIKAlpha > 0.0f);
	In.bLeftHand = bLeftHolds;
	In.bLeftHandTurns = bLeftGrip;
	In.LeftHandInWeapon = HandInWeapon;
	In.Kick = RecoilKick;

	// A one-handed weapon carried at a run (the plain run's arm swing): the blade forward, down and out to the right,
	// clear of the body and the other arm. Not while a montage (a swing, a dodge, a draw) has the arm.
	const bool bCarry = CVarCarryTurn.GetValueOnGameThread() > 0 && bRelaxed && Speed > 60.0f && In.bHeld && bHandsFree && Weapon && !Weapon->IsRanged() && !Weapon->IsHeldTwoHanded() && !IsAnyMontagePlaying();
	CarryBlend = FMath::FInterpTo(CarryBlend, bCarry ? 1.0f : 0.0f, DeltaSeconds, 8.0f);
	In.CarryBlend = CarryBlend;
	In.CarryDirection = CarryDirection;

	// Working a bolt by hand: how far through it (the right hand goes to the Bolt socket and back).
	const UVaesenRangedWeaponItem* RangedItem = Cast<UVaesenRangedWeaponItem>(Weapon);
	const bool bWorksBolt = bCyclingNow && In.bHeld && RangedItem && RangedItem->Action == EVaesenWeaponAction::BoltAction
		&& Held.GetLocalSocket(TEXT("Bolt"), In.BoltLocation);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (!bWorksBolt)
	{
		CycleStartTime = -1.0;
	}
	else if (CycleStartTime < 0.0)
	{
		CycleStartTime = Now;
	}
	In.bCycling = bWorksBolt;
	In.CyclePhase = bWorksBolt ? FMath::Clamp(static_cast<float>((Now - CycleStartTime) / FMath::Max(RangedItem->CycleTime, 0.05f)), 0.0f, 1.0f) : 0.0f;
	const float BlendSpeed = Held.Hold ? Held.Hold->AimBlendSpeed : 12.0f;
	AimIKAlpha = Rig ? (bShoulderAim ? 1.0f : 0.0f) : FMath::FInterpTo(AimIKAlpha, bShoulderAim ? 1.0f : 0.0f, DeltaSeconds, BlendSpeed);
	if (!bInHand)
	{
		AimIKAlpha = 0.0f;
	}
	In.ShoulderBlend = AimIKAlpha;

	// The fingers closed round the grips, of each hand that holds the weapon.
	GripPose = UVaesenWeaponCategoryDefaults::Get()->GetCategory(WeaponCategory).GripPose;
	// A bow: the left fist round it, the right hand free (the drawing hand, its fingers as the animation has them).
	const bool bLeftFist = bInHand && Weapon && Weapon->IsHeldInLeftHand();
	RightFingerGrip = GripPose && bInHand && bHandsFree && !bWorksBolt && !bLeftFist ? 1.0f : 0.0f;
	LeftFingerGrip = GripPose && (bLeftHolds || bLeftFist) ? 1.0f : 0.0f;
	// (The Shoulder Aim node holds the left hand on the weapon.)
	LeftHandIKAlpha = bLeftHolds ? 1.0f : 0.0f;
	LeftHandRotationAlpha = bLeftGrip ? LeftHandIKAlpha : 0.0f;
	RecoilKick = KickAt(static_cast<float>(Now - ShotTime), Held.Hold ? Held.Hold->RecoilRecovery : 0.35f);
	ShoulderAimInput.Kick = RecoilKick;

	// A guard held procedurally (bProceduralGuard: no guard animation, or one that turns side-on): the Shoulder Aim node
	// holds the weapon in front of the chest (a long gun across it in both hands); a blow knocks it back.
	const bool bGuard = bGuardUp && (In.bHeld || bLeftFist) && bHandsFree && !bShoulderAim && Weapon && Weapon->GetCategoryDefaults().bProceduralGuard;
	GuardAlpha = !(In.bHeld || bLeftFist) ? 0.0f : Rig ? (bGuard ? 1.0f : 0.0f) : FMath::FInterpTo(GuardAlpha, bGuard ? 1.0f : 0.0f, DeltaSeconds, 10.0f);
	In.GuardBlend = GuardAlpha;
	// Manny faces +Y; his left is +X. A gun's barrel (X) goes across, its top away from him. A bow (in the left fist; its
	// limbs along Y, the string to +Z) goes across by its limbs, the string toward him, the right hand on the upper limb.
	// A blade (or a club or an axe) held in one hand: raised in front of the right side of the chest, the blade (X)
	// nearly upright (not across the face), the edge (Z) to the front; facing ahead, not turned side-on.
	const FVector Across = FVector(0.85, 0.1, 0.5).GetSafeNormal();
	const bool bBladeGuard = Weapon && !Weapon->IsRanged();
	const FVector Ahead(0.0, 1.0, 0.0);
	In.GuardRotation = (bLeftFist ? FRotationMatrix::MakeFromYZ(Across, -Ahead)
		: FRotationMatrix::MakeFromXZ(bBladeGuard ? FVector(0.12, 0.3, 0.95).GetSafeNormal() : Across, Ahead)).ToQuat();
	In.GuardFromChest = bBladeGuard && !bLeftHolds ? FVector(-16.0, 42.0, -26.0) : FVector(-2.0, 30.0, -10.0);
	In.GuardMiddleInWeapon = bLeftFist ? FVector::ZeroVector : bLeftHolds ? LeftGrip.GetLocation() * 0.5 : bBladeGuard ? FVector::ZeroVector : FVector(20.0, 0.0, 0.0);
	In.bLeftHeld = bLeftFist;
	In.GripOnLeftHand = bLeftFist && Held.IsValid() ? Held.Hold->InHand * Weapon->GetCategoryDefaults().InHand : FTransform::Identity;
	In.RightHandInWeapon = FTransform(FVector(0.0, 30.0, 2.0));
	In.GuardKnock = KickAt(static_cast<float>(Now - GuardKnockTime), 0.3f);

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
				bSheathIKLeftHand = Found->GetWeaponItem() && Found->GetWeaponItem()->IsHeldInLeftHand();
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

void UVaesenAnimInstance::PulseTriggerFinger()
{
	TriggerPulse = 1.0f;
	ShotTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

void UVaesenAnimInstance::KnockGuard()
{
	GuardKnockTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float UVaesenAnimInstance::KickAt(float SinceShot, float Recovery)
{
	constexpr float Rise = 0.03f;
	if (SinceShot < 0.0f)
	{
		return 0.0f;
	}
	if (SinceShot < Rise)
	{
		return FMath::Sin(HALF_PI * SinceShot / Rise);
	}
	// Settling: most of the way back over Recovery (e^-3 left at its end).
	return FMath::Exp(-3.0f * (SinceShot - Rise) / FMath::Max(Recovery, 0.05f));
}

bool UVaesenAnimInstance::GetShoulderAimInput(FVaesenShoulderAimInput& Out) const
{
	Out = ShoulderAimInput;
	return true;
}
