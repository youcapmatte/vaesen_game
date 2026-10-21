#include "Anim/VaesenAnimInstance.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
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

	// Aim offset: the view relative to the body.
	const FRotator Delta = (Character->GetBaseAimRotation() - Character->GetActorRotation()).GetNormalized();
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
	LeftHandIKAlpha = bTwoHanded && WeaponActor && WeaponActor->GetSocketTransform(TEXT("LeftHandIK"), LeftHand) && bHandsFree ? 1.0f : 0.0f;
	LeftHandIKTransform = LeftHand;
	LeftHandIKLocation = LeftHand.GetLocation();

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
