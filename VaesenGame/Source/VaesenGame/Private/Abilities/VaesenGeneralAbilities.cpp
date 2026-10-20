#include "Abilities/VaesenGeneralAbilities.h"

#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenGameplayTags.h"
#include "Engine/World.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenPouchItem.h"
#include "Items/VaesenWeaponItem.h"
#include "Weapons/VaesenThrownItem.h"
#include "Weapons/VaesenWeaponActor.h"

// --- Holster ---

UVaesenGA_Holster::UVaesenGA_Holster()
{
	InputBinding = EVaesenAbilityInput::Holster;
	// The request goes to the server through the equipment; nothing to predict here.
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	SetAssetTags(FGameplayTagContainer(VaesenTags::Ability_Holster));
	ActivationBlockedTags.AddTag(VaesenTags::State_Attacking);
	ActivationBlockedTags.AddTag(VaesenTags::State_Throwing);
}

UVaesenGA_SwapWeapon::UVaesenGA_SwapWeapon()
{
	InputBinding = EVaesenAbilityInput::SwapWeapon;
	bSwap = true;
}

void UVaesenGA_Holster::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (UVaesenEquipmentComponent* Equipment = GetEquipment(ActorInfo))
		{
			bSwap ? Equipment->RequestSwapWeapon() : Equipment->RequestToggleWeapon();
		}
	}
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

// --- Throw ---

UVaesenGA_Throw::UVaesenGA_Throw()
{
	InputBinding = EVaesenAbilityInput::Throw;
	SetAssetTags(FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{ VaesenTags::Ability_Throw, VaesenTags::Ability_Attack }));
	ActivationOwnedTags.AddTag(VaesenTags::State_Throwing);
	for (const FGameplayTag& Tag : TArray<FGameplayTag>{ VaesenTags::State_Attacking, VaesenTags::State_SwitchingWeapon, VaesenTags::State_Reloading, VaesenTags::State_Blocking })
	{
		ActivationBlockedTags.AddTag(Tag);
	}
	CancelAbilitiesWithTag.AddTag(VaesenTags::Ability_Aim);
}

FVaesenThrowable UVaesenGA_Throw::FindThrowable(const AActor* Actor)
{
	FVaesenThrowable Result;
	const AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(Actor);
	if (!Player)
	{
		return Result;
	}

	// The weapon in the hand first (a broken one can still be thrown).
	if (const UVaesenEquipmentComponent* Equipment = Player->GetEquipment())
	{
		for (const EVaesenEquipmentSlot Slot : { EVaesenEquipmentSlot::MainHand, EVaesenEquipmentSlot::Sidearm, EVaesenEquipmentSlot::OffHand })
		{
			UVaesenWeaponItem* Weapon = Equipment->GetWeaponInSlot(Slot);
			if (Weapon && Equipment->IsInHand(Slot) && Weapon->CanBeThrown())
			{
				Result.Item = Weapon;
				Result.bEquipped = true;
				Result.EquippedSlot = static_cast<uint8>(Slot);
				return Result;
			}
		}
	}

	// Otherwise the first throwable thing carried (stones), in the slots, then in pouches.
	if (const UVaesenInventoryComponent* Inventory = Player->GetInventory())
	{
		for (UVaesenItem* Item : Inventory->GetSlots())
		{
			if (Item && Item->CanBeThrown())
			{
				Result.Item = Item;
				return Result;
			}
		}
		for (UVaesenItem* Item : Inventory->GetSlots())
		{
			if (const UVaesenPouchItem* Pouch = Cast<UVaesenPouchItem>(Item))
			{
				for (UVaesenItem* Inner : Pouch->GetContents())
				{
					if (Inner && Inner->CanBeThrown())
					{
						Result.Item = Inner;
						return Result;
					}
				}
			}
		}
	}
	return Result;
}

void UVaesenGA_Throw::GetThrowLaunch(const AActor* Character, const UVaesenItem* Item, const FVector& AimDirection, FVector& OutStart, FVector& OutVelocity)
{
	const FVector Direction = AimDirection.GetSafeNormal();
	// From the throwing hand's height, a little in front.
	OutStart = Character->GetActorLocation() + FVector(0.0, 0.0, 55.0) + Character->GetActorForwardVector() * 30.0f + Character->GetActorRightVector() * 15.0f;
	const float Speed = Item ? Item->Throwing.Speed : 1200.0f;
	// A lob rises more; everything gets a little lift so it doesn't skid off the floor.
	const float Lift = Item && Item->Throwing.Style == EVaesenThrowStyle::Lob ? 0.25f : 0.08f;
	OutVelocity = (Direction + FVector(0.0, 0.0, Lift)).GetSafeNormal() * Speed;
}

float UVaesenGA_Throw::GetStaminaCost(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const FVaesenThrowable Throwable = FindThrowable(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	return Throwable.IsValid() ? Throwable.Item->Throwing.StaminaCost : 0.0f;
}

bool UVaesenGA_Throw::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& FindThrowable(ActorInfo->AvatarActor.Get()).IsValid();
}

void UVaesenGA_Throw::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// The cost is paid on release; holding only aims.
	bReleased = false;
	if (!IsLocallyControlled())
	{
		WaitForAim();
	}
}

void UVaesenGA_Throw::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (bReleased || !IsLocallyControlled())
	{
		return;
	}
	bReleased = true;
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		SendAimCancelled();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FVaesenThrowable Throwable = FindThrowable(ActorInfo->AvatarActor.Get());
	const UVaesenWeaponItem* Weapon = Throwable.bEquipped ? Cast<UVaesenWeaponItem>(Throwable.Item) : nullptr;
	PlayAbilityMontage(Weapon ? Weapon->GetMontage(&FVaesenWeaponMontages::Throw)
		: UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).Montages.Throw.Get());
	if (AVaesenBaseCharacter* Character = GetVaesenCharacter(ActorInfo))
	{
		Character->PlayVocal(false);
	}

	FVector Origin, Direction;
	GetAimViewPoint(Origin, Direction);
	SendAim(Origin, Direction, 1.0f);
}

void UVaesenGA_Throw::OnAimReceived(const FVector& Origin, const FVector& Direction, float Strength)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	AVaesenPlayerCharacter* Player = Cast<AVaesenPlayerCharacter>(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
	// The server's copy of a remote player's throw pays its cost now (the player's machine did on release).
	const bool bPaid = IsLocallyControlled() || CommitAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo);
	if (HasAuthority(&CurrentActivationInfo) && Player && bPaid)
	{
		// The server finds what to throw itself (the client only says where).
		const FVaesenThrowable Throwable = FindThrowable(Player);
		if (Throwable.IsValid())
		{
			FVector Start, Velocity;
			GetThrowLaunch(Player, Throwable.Item, Direction, Start, Velocity);

			FActorSpawnParameters Params;
			Params.Owner = Player;
			Params.Instigator = Player;
			Params.bDeferConstruction = true;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FTransform Transform(Velocity.Rotation(), Start);
			if (AVaesenThrownItem* Thrown = GetWorld()->SpawnActor<AVaesenThrownItem>(AVaesenThrownItem::StaticClass(), Transform, Params))
			{
				// A thrown weapon comes out of the hand (unequipped first); a stone comes off its stack.
				UVaesenItem* Copy = Throwable.bEquipped
					? Player->GetEquipment()->TakeOutForThrow(static_cast<EVaesenEquipmentSlot>(Throwable.EquippedSlot), Thrown)
					: Throwable.Item->DuplicateItem(Thrown, 1);
				if (Copy && (Throwable.bEquipped || Player->GetInventory()->RemoveItem(Throwable.Item, 1) > 0))
				{
					Thrown->InitThrow(Copy, Velocity);
					Thrown->FinishSpawning(Transform);
				}
				else
				{
					Thrown->Destroy();
				}
			}
		}
	}
	EndAbility(CurrentSpecHandle, ActorInfo, CurrentActivationInfo, true, false);
}

void UVaesenGA_Throw::OnAimCancelled()
{
	EndAbility(CurrentSpecHandle, GetCurrentActorInfo(), CurrentActivationInfo, true, true);
}
