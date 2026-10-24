#include "Combat/VaesenCombatLibrary.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimMontage.h"
#include "Anim/VaesenAnimNotifies.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Combat/VaesenGameplayEffects.h"
#include "Combat/VaesenGameplayTags.h"
#include "Core/VaesenSettings.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Weapons/VaesenWeaponData.h"

DEFINE_LOG_CATEGORY(LogVaesenCombat);

static TAutoConsoleVariable<int32> CVarVaesenCombatDebug(
	TEXT("vaesen.combat.debug"), 0,
	TEXT("1 = draw melee traces, projectile paths and hit points."),
	ECVF_Cheat);

// While animations are being worked on: guns fire without ammunition. Off in shipping builds.
static TAutoConsoleVariable<int32> CVarVaesenFreeFire(
	TEXT("vaesen.combat.freefire"), UE_BUILD_SHIPPING ? 0 : 1,
	TEXT("1 = guns and bows fire without ammunition (none used; an empty gun fires its usual round). 0 = the ammo rules."),
	ECVF_Cheat);

bool UVaesenCombatLibrary::IsFreeFire()
{
	return CVarVaesenFreeFire.GetValueOnAnyThread() > 0;
}

bool UVaesenCombatLibrary::IsCombatDebugEnabled()
{
	return CVarVaesenCombatDebug.GetValueOnAnyThread() > 0;
}

bool UVaesenCombatLibrary::ApplyDamagePacket(AActor* DamageInstigator, AActor* Target, const FVaesenDamagePacket& Packet, const FHitResult& Hit,
	UObject* SourceObject, bool bCanBeBlocked, bool bProjectile, float Knockback)
{
	if (!Target || Packet.IsEmpty() || (DamageInstigator && !DamageInstigator->HasAuthority()))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
	if (!TargetASC)
	{
		UE_LOG(LogVaesenCombat, Verbose, TEXT("%s hit %s (no ability system): %s"), *GetNameSafe(DamageInstigator), *GetNameSafe(Target), *Packet.ToString());
		return false;
	}
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageInstigator);

	FGameplayEffectContextHandle Context = (SourceASC ? SourceASC : TargetASC)->MakeEffectContext();
	Context.AddInstigator(DamageInstigator, SourceObject ? Cast<AActor>(SourceObject->GetOuter()) : DamageInstigator);
	Context.AddSourceObject(SourceObject);
	if (Hit.bBlockingHit || !Hit.ImpactPoint.IsZero())
	{
		Context.AddHitResult(Hit, true);
	}
	if (DamageInstigator)
	{
		Context.AddOrigin(DamageInstigator->GetActorLocation());
	}

	TSubclassOf<UGameplayEffect> EffectClass = UVaesenSettings::Get()->DamageEffect.LoadSynchronous();
	if (!EffectClass)
	{
		EffectClass = UVaesenGE_Damage::StaticClass();
	}
	FGameplayEffectSpec Spec(EffectClass->GetDefaultObject<UGameplayEffect>(), Context, 1.0f);

	// One magnitude per damage type (entries of the same type add up).
	TMap<FGameplayTag, float> PerType;
	for (const FVaesenDamageEntry& Entry : Packet.Entries)
	{
		if (Entry.DamageType.IsValid() && Entry.Amount > 0.0f)
		{
			PerType.FindOrAdd(Entry.DamageType) += Entry.Amount;
		}
	}
	for (const TPair<FGameplayTag, float>& Pair : PerType)
	{
		Spec.SetSetByCallerMagnitude(Pair.Key, Pair.Value);
	}
	Spec.SetSetByCallerMagnitude(VaesenTags::Data_Unblockable, bCanBeBlocked ? 0.0f : 1.0f);
	Spec.SetSetByCallerMagnitude(VaesenTags::Data_Projectile, bProjectile ? 1.0f : 0.0f);
	Spec.SetSetByCallerMagnitude(VaesenTags::Data_Knockback, Knockback);

	if (SourceASC)
	{
		SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
	}
	else
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return true;
}

EPhysicalSurface UVaesenCombatLibrary::GetSurface(const FHitResult& Hit)
{
	if (const UPhysicalMaterial* Material = Hit.PhysMaterial.Get())
	{
		return Material->SurfaceType;
	}
	return SurfaceType_Default;
}

void UVaesenCombatLibrary::PlayImpactEffects(const UObject* WorldContext, FGameplayTag DamageType, const FHitResult& Hit, EVaesenImpactDelivery Delivery)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const FVector Location = Hit.ImpactPoint.IsZero() ? Hit.Location : Hit.ImpactPoint;
	if (IsCombatDebugEnabled())
	{
		DrawDebugSphere(World, Location, 6.0f, 8, FColor::Red, false, 2.0f);
	}

	const UVaesenImpactEffects* Table = UVaesenWeaponCategoryDefaults::Get()->ImpactEffects;
	const FVaesenImpactEntry* Entry = Table ? Table->FindEntry(DamageType, GetSurface(Hit), Delivery) : nullptr;
	if (!Entry)
	{
		return;
	}

	const FRotator Rotation = Hit.ImpactNormal.IsNearlyZero() ? FRotator::ZeroRotator : Hit.ImpactNormal.Rotation();
	if (Entry->Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Entry->Sound, Location);
	}
	if (UNiagaraSystem* Niagara = Cast<UNiagaraSystem>(Entry->Effect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Niagara, Location, Rotation);
	}
	else if (UParticleSystem* Cascade = Cast<UParticleSystem>(Entry->Effect))
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, Cascade, Location, Rotation);
	}
	if (Delivery == EVaesenImpactDelivery::Shot && Table->RicochetSound && FMath::FRand() < Entry->RicochetChance)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Table->RicochetSound, Location);
	}
}

const FVaesenBlockProfile* UVaesenCombatLibrary::FindBlockProfile(const AActor* Target, bool bProjectile, UVaesenWeaponItem*& OutBlockingItem)
{
	OutBlockingItem = nullptr;
	const AVaesenBaseCharacter* Character = Cast<AVaesenBaseCharacter>(Target);
	const UVaesenEquipmentComponent* Equipment = Character ? Character->GetEquipment() : nullptr;
	if (!Equipment)
	{
		return nullptr;
	}

	// A shield in the off hand blocks with its own profile; otherwise the weapon in the hand. Broken things don't block.
	if (UVaesenMeleeWeaponItem* Shield = Equipment->GetDrawnShield())
	{
		if (!Shield->IsBroken())
		{
			OutBlockingItem = Shield;
			return &Shield->GetBlockProfile();
		}
	}
	if (UVaesenWeaponItem* Weapon = Equipment->GetDrawnWeapon())
	{
		if (Weapon->IsBroken())
		{
			return nullptr;
		}
		OutBlockingItem = Weapon;
		const FVaesenBlockProfile& Profile = Weapon->GetBlockProfile();
		return bProjectile && !Profile.bCanBlockProjectiles ? nullptr : &Profile;
	}
	if (bProjectile)
	{
		return nullptr;
	}
	return &UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).BlockProfile;
}

bool UVaesenCombatLibrary::MontageHasNotify(const UAnimMontage* Montage, TSubclassOf<UObject> NotifyClass)
{
	if (!Montage || !NotifyClass)
	{
		return false;
	}
	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		const UObject* Notify = Event.Notify ? static_cast<const UObject*>(Event.Notify.Get()) : static_cast<const UObject*>(Event.NotifyStateClass.Get());
		if (Notify && Notify->IsA(NotifyClass))
		{
			return true;
		}
	}
	return false;
}

bool UVaesenCombatLibrary::MontageHasWeaponHandoffNotify(const UAnimMontage* Montage)
{
	return MontageHasNotify(Montage, UAnimNotify_VaesenWeaponHandoff::StaticClass());
}

bool UVaesenCombatLibrary::MontageHasHitWindow(const UAnimMontage* Montage)
{
	return MontageHasNotify(Montage, UAnimNotifyState_VaesenMeleeHitWindow::StaticClass());
}

bool UVaesenCombatLibrary::MontageHasCasingNotify(const UAnimMontage* Montage)
{
	return MontageHasNotify(Montage, UAnimNotify_VaesenEjectCasing::StaticClass());
}

FGameplayTag UVaesenCombatLibrary::GetMainDamageType(const FVaesenDamagePacket& Packet)
{
	const FVaesenDamageEntry* Main = nullptr;
	for (const FVaesenDamageEntry& Entry : Packet.Entries)
	{
		if (!Main || Entry.Amount > Main->Amount)
		{
			Main = &Entry;
		}
	}
	return Main ? Main->DamageType : FGameplayTag();
}
