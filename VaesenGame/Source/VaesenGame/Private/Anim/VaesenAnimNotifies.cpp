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
	if (const AVaesenWeaponActor* Weapon = VaesenNotifies::GetDrawnWeaponActor(MeshComp))
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
