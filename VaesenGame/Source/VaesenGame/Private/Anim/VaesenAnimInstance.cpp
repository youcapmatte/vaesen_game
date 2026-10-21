#include "Anim/VaesenAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenGameplayTags.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "KismetAnimationLibrary.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"

const FName UVaesenAnimInstance::HandIKSheathCurve(TEXT("HandIK_Sheath"));
const FName UVaesenAnimInstance::TriggerFingerCurve(TEXT("TriggerFinger"));

void UVaesenAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AVaesenBaseCharacter* Character = Cast<AVaesenBaseCharacter>(TryGetPawnOwner());
	if (!Character)
	{
		return;
	}

	// A linked layer (the drawn weapon's locomotion) holds the same vaesen.weaponfit pose as the main graph.
	if (const UVaesenAnimInstance* Main = Cast<UVaesenAnimInstance>(GetOwningComponent()->GetAnimInstance()); Main && Main != this)
	{
		FitPose = Main->FitPose;
		FitAim = Main->FitAim;
	}

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
	bool bShouldering = HasTag(VaesenTags::State_Aiming);

	// vaesen.weaponfit: a held pose, standing still.
	if (FitPose != EFitPose::None)
	{
		bAiming = bShouldering = FitPose == EFitPose::Aim || FitPose == EFitPose::CrouchAim;
		bCrouching = FitPose == EFitPose::Crouch || FitPose == EFitPose::CrouchAim;
		bFalling = bReloading = bBlocking = bStaggered = false;
		Speed = Direction = 0.0f;
	}

	// Aim offset: the view relative to the body.
	const FRotator Delta = FitPose != EFitPose::None ? FitAim : (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
	AimPitch = FMath::Clamp(Delta.Pitch, -90.0f, 90.0f);
	AimYaw = FMath::Clamp(Delta.Yaw, -120.0f, 120.0f);

	const UVaesenEquipmentComponent* Equipment = Character->GetEquipment();
	const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
	const AVaesenWeaponActor* WeaponActor = Weapon ? Weapon->GetWeaponActor() : nullptr;
	bWeaponDrawn = Weapon != nullptr;
	WeaponCategory = Weapon ? Weapon->Category : EVaesenWeaponCategory::Unarmed;
	bTwoHanded = Weapon && Weapon->bTwoHanded;
	LocomotionDatabase = UVaesenWeaponCategoryDefaults::Get()->GetCategory(WeaponCategory).LocomotionDatabase.Get();

	const AVaesenRangedWeaponActor* Ranged = Cast<AVaesenRangedWeaponActor>(WeaponActor);
	BowDrawAlpha = Ranged ? Ranged->GetDrawAlpha() : 0.0f;

	// Off hand on the fore-grip of two-handed weapons.
	FTransform LeftHand;
	// Not while the hands are busy with something else (reloading, drawing or sheathing).
	const bool bHandsFree = !bReloading && !HasTag(VaesenTags::State_SwitchingWeapon);
	const bool bInHand = WeaponActor && WeaponActor->IsInHand();
	LeftHandIKAlpha = bTwoHanded && bInHand && WeaponActor->GetSocketTransform(TEXT("LeftHandIK"), LeftHand) && bHandsFree ? 1.0f : 0.0f;
	if (LeftHandIKAlpha > 0.0f)
	{
		LeftHand.AddToTranslation(WeaponActor->GetActorQuat().RotateVector(WeaponActor->Hold.LeftHandOffset));
		// Relative to the right hand, which holds the weapon rigidly: right however the arm moves this frame.
		LeftHandIKInHand = LeftHand.GetRelativeTransform(Character->GetMesh()->GetSocketTransform(TEXT("hand_r"))).GetLocation();
	}
	LeftHandIKTransform = LeftHand;
	LeftHandIKLocation = LeftHand.GetLocation();

	// Shouldered while aiming two-handed guns (not bows: they have their own draw).
	FTransform AimHand;
	const bool bShoulderAim = bShouldering && bTwoHanded && bInHand && bHandsFree && WeaponActor->Hold.bShoulderAim
		&& WeaponCategory != EVaesenWeaponCategory::Bow && ComputeShoulderAim(Character, WeaponActor, AimHand);
	const float BlendSpeed = WeaponActor ? WeaponActor->Hold.AimBlendSpeed : 12.0f;
	AimIKAlpha = FitPose != EFitPose::None ? (bShoulderAim ? 1.0f : 0.0f) : FMath::FInterpTo(AimIKAlpha, bShoulderAim ? 1.0f : 0.0f, DeltaSeconds, BlendSpeed);
	// Kept relative to the body, so it eases out from where the weapon was even while walking.
	const FTransform& Component = Character->GetMesh()->GetComponentTransform();
	if (bShoulderAim)
	{
		AimHandInComponent = AimHand.GetRelativeTransform(Component);
	}
	else if (!bInHand)
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
				SheathTargetInComponent = Found->GetSheathedTransform().GetRelativeTransform(Character->GetMesh()->GetComponentTransform());
				bSheathIKLeftHand = Found->GetWeaponItem() && Found->GetWeaponItem()->IsHeldInOffHand();
			}
		}
		if (SheathTarget.IsValid())
		{
			SheathHandIKTransform = SheathTargetInComponent * Character->GetMesh()->GetComponentTransform();
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

bool UVaesenAnimInstance::ComputeShoulderAim(const AVaesenBaseCharacter* Character, const AVaesenWeaponActor* Weapon, FTransform& OutHand) const
{
	const USkeletalMeshComponent* Body = Character->GetMesh();
	const FVaesenWeaponHold& Hold = Weapon->Hold;

	// Where it looks: the view, or the aim held by vaesen.weaponfit.
	FRotator Aim = FitPose != EFitPose::None ? Character->GetActorRotation() + FitAim : Character->GetBaseAimRotation();
	const FVector Head = Body->GetSocketLocation(TEXT("head"));
	FVector Sight = Head + Aim.RotateVector(Hold.AimSightOffset);

	// The player's own view: the barrel converges on what the crosshair is on (30 m out), not parallel to the camera.
	const APlayerController* Controller = Cast<APlayerController>(Character->GetController());
	if (FitPose == EFitPose::None && Controller && Controller->IsLocalController() && Controller->PlayerCameraManager)
	{
		const FVector Target = Controller->PlayerCameraManager->GetCameraLocation() + Controller->PlayerCameraManager->GetCameraRotation().Vector() * 3000.0f;
		Aim = (Target - Sight).Rotation();
		Sight = Head + Aim.RotateVector(Hold.AimSightOffset);
	}
	Aim.Roll = 0.0f;

	// The kick of a shot: back along the aim and the muzzle up.
	const FVector Along = Aim.Vector();
	Sight -= Along * Hold.RecoilBack * RecoilKick;
	const FQuat Rotation = Aim.Quaternion() * Hold.AimRotation.Quaternion() * FRotator(Hold.RecoilPitch * RecoilKick, 0.0, 0.0).Quaternion();

	// The weapon laid there: its Sight socket on the point (no Sight: a little over the grip).
	const FTransform Current = Weapon->GetActorTransform();
	FTransform Socket;
	const FVector SightLocal = Weapon->GetSocketTransform(TEXT("Sight"), Socket) ? Current.InverseTransformPosition(Socket.GetLocation()) : FVector(0.0, 0.0, 6.0);
	FVector Location = Sight - Rotation.RotateVector(SightLocal);

	// With a Stock socket, slid along the aim so the stock meets the shoulder.
	if (Weapon->GetSocketTransform(TEXT("Stock"), Socket))
	{
		const FVector Stock = Location + Rotation.RotateVector(Current.InverseTransformPosition(Socket.GetLocation()));
		const FVector Shoulder = Body->GetSocketLocation(TEXT("upperarm_r"));
		Location += Along * (Hold.StockToShoulder - FVector::DotProduct(Stock - Shoulder, Along));
	}

	// The hand that holds it, which holds it rigidly: wherever the weapon goes, the hand goes the same.
	const FTransform WeaponInHand = Current.GetRelativeTransform(Body->GetSocketTransform(TEXT("hand_r")));
	OutHand = WeaponInHand.Inverse() * FTransform(Rotation, Location);
	return true;
}

void UVaesenAnimInstance::SetFitPose(EFitPose Pose)
{
	FitPose = Pose;
	if (const APawn* Pawn = TryGetPawnOwner())
	{
		FitAim = (Pawn->GetBaseAimRotation() - Pawn->GetActorRotation()).GetNormalized();
		FitAim.Roll = 0.0f;
	}
}

namespace VaesenWeaponFit
{
	void Print(UWorld* World, const FString& Text, bool bError = false)
	{
		UE_LOG(LogTemp, Display, TEXT("[weaponfit] %s"), *Text);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 8.0f, bError ? FColor::Orange : FColor::Cyan, FString::Printf(TEXT("[weaponfit] %s"), *Text));
		}
	}

	void Run(const TArray<FString>& Args, UWorld* World)
	{
		const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
		const AVaesenBaseCharacter* Character = Controller ? Cast<AVaesenBaseCharacter>(Controller->GetPawn()) : nullptr;
		UVaesenAnimInstance* Anim = Character ? Cast<UVaesenAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
		const UVaesenWeaponItem* Weapon = Character && Character->GetEquipment() ? Character->GetEquipment()->GetDrawnWeapon() : nullptr;
		AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr;
		if (!Anim)
		{
			Print(World, TEXT("No player character with a Vaesen anim Blueprint here."), true);
			return;
		}

		const FString Command = Args.Num() > 0 ? Args[0].ToLower() : FString();
		using EFitPose = UVaesenAnimInstance::EFitPose;
		const TMap<FString, EFitPose> Poses = {
			{ TEXT("hip"), EFitPose::Hip }, { TEXT("aim"), EFitPose::Aim }, { TEXT("crouch"), EFitPose::Crouch },
			{ TEXT("crouchaim"), EFitPose::CrouchAim }, { TEXT("off"), EFitPose::None } };
		if (const EFitPose* Pose = Poses.Find(Command))
		{
			Anim->SetFitPose(*Pose);
			if (*Pose == EFitPose::None)
			{
				Print(World, TEXT("Off."));
			}
			else if (!Actor)
			{
				Print(World, TEXT("Holding the pose, but no weapon is drawn: draw one (H)."), true);
			}
			else
			{
				Print(World, FString::Printf(TEXT("Holding %s with %s. F8, click the weapon, edit Hold in Details; 'vaesen.weaponfit save' keeps it."),
					*Command, *Weapon->ItemName.ToString()));
			}
			return;
		}

		if (Command == TEXT("save"))
		{
			if (!Actor)
			{
				Print(World, TEXT("No weapon drawn."), true);
				return;
			}
#if WITH_EDITOR
			UVaesenWeaponItem* Defaults = Weapon->GetClass()->GetDefaultObject<UVaesenWeaponItem>();
			Defaults->Modify();
			Defaults->Hold = Actor->Hold;
			Defaults->MarkPackageDirty();
			Print(World, FString::Printf(TEXT("Saved into %s. Save it (Save All) to keep it after the editor closes."), *Weapon->GetClass()->GetName()));
#else
			Print(World, TEXT("Saving needs the editor."), true);
#endif
			return;
		}

		if (Command == TEXT("print") && Actor)
		{
			const FVaesenWeaponHold& Hold = Actor->Hold;
			Print(World, FString::Printf(TEXT("%s: InHand %s / %s, LeftHandOffset %s, AimSightOffset %s, StockToShoulder %.1f, AimRotation %s"),
				*Weapon->GetClass()->GetName(), *Hold.InHand.GetLocation().ToCompactString(), *Hold.InHand.Rotator().ToCompactString(),
				*Hold.LeftHandOffset.ToCompactString(), *Hold.AimSightOffset.ToCompactString(), Hold.StockToShoulder, *Hold.AimRotation.ToCompactString()));
			return;
		}

		Print(World, TEXT("vaesen.weaponfit hip | aim | crouch | crouchaim | off | print | save"), true);
	}

	FAutoConsoleCommandWithWorldAndArgs Command(TEXT("vaesen.weaponfit"),
		TEXT("Holds a pose to tune the drawn weapon's Hold: hip, aim, crouch, crouchaim, off; print shows it; save writes it into the weapon's Blueprint."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Run));
}
