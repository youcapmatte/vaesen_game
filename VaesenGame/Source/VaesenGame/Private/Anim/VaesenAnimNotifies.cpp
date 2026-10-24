#include "Anim/VaesenAnimNotifies.h"

#include "Characters/VaesenBaseCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "Weapons/VaesenRangedWeaponActor.h"

namespace VaesenNotifies
{
	AVaesenBaseCharacter* GetCharacter(const USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? Cast<AVaesenBaseCharacter>(MeshComp->GetOwner()) : nullptr;
	}

	AVaesenWeaponActor* GetDrawnWeaponActor(const USkeletalMeshComponent* MeshComp)
	{
		const AVaesenBaseCharacter* Character = GetCharacter(MeshComp);
		const UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr;
		const UVaesenWeaponItem* Weapon = Equipment ? Equipment->GetDrawnWeapon() : nullptr;
		return Weapon ? Weapon->GetWeaponActor() : nullptr;
	}
}

void UAnimNotify_VaesenWeaponHandoff::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	const AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp);
	if (UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr)
	{
		Equipment->HandleWeaponGrabNotify();
	}
}

void UAnimNotify_VaesenEjectCasing::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AVaesenRangedWeaponActor* Ranged = Cast<AVaesenRangedWeaponActor>(VaesenNotifies::GetDrawnWeaponActor(MeshComp)))
	{
		Ranged->EjectCasingsAt(Moment, Count);
	}
}

void UAnimNotify_VaesenWeaponSound::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// Drawing or sheathing: the weapon on the move (before the hand has it, it isn't drawn yet).
	const AVaesenBaseCharacter* Owner = VaesenNotifies::GetCharacter(MeshComp);
	const UVaesenEquipmentComponent* Equipment = Owner ? Owner->GetEquipment() : nullptr;
	const UVaesenWeaponItem* Switching = Equipment && (Sound == EVaesenWeaponSound::Draw || Sound == EVaesenWeaponSound::Sheath)
		? Equipment->GetWeaponBeingSwitched() : nullptr;
	if (const AVaesenWeaponActor* Moving = Switching ? Switching->GetWeaponActor() : nullptr)
	{
		Moving->PlayWeaponSound(Sound);
	}
	else if (const AVaesenWeaponActor* Weapon = VaesenNotifies::GetDrawnWeaponActor(MeshComp))
	{
		Weapon->PlayWeaponSound(Sound);
	}
	else if (const AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp))
	{
		// Bare hands: the unarmed category's sounds.
		const UVaesenWeaponSoundSet* Set = UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).Sounds;
		if (USoundBase* Played = Set ? Set->GetSound(Sound) : nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(Character, Played, Character->GetActorLocation(), FRotator::ZeroRotator, 1.0f, 1.0f, 0.0f, Set->ShortRangeAttenuation, Set->Concurrency);
		}
	}
}

bool UAnimNotify_VaesenWeaponSound::IsIn(const UAnimSequenceBase* Animation, EVaesenWeaponSound Sound)
{
	if (!Animation)
	{
		return false;
	}
	for (const FAnimNotifyEvent& Event : Animation->Notifies)
	{
		const UAnimNotify_VaesenWeaponSound* Notify = Cast<UAnimNotify_VaesenWeaponSound>(Event.Notify);
		if (Notify && Notify->Sound == Sound)
		{
			return true;
		}
	}
	return false;
}

FString UAnimNotify_VaesenWeaponSound::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Weapon Sound: %s"), *StaticEnum<EVaesenWeaponSound>()->GetNameStringByValue(static_cast<int64>(Sound)));
}

void UAnimNotify_VaesenEffortVocal::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp))
	{
		Character->PlayVocal(false);
	}
}

void UAnimNotifyState_VaesenMeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp))
	{
		Character->OnMeleeHitWindow.Broadcast(true);
	}
}

void UAnimNotifyState_VaesenMeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp))
	{
		Character->OnMeleeHitWindow.Broadcast(false);
	}
}

void UAnimNotify_VaesenFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AVaesenBaseCharacter* Character = VaesenNotifies::GetCharacter(MeshComp))
	{
		Character->OnFootstep();
	}
}
