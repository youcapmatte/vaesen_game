#include "Combat/VaesenDamageExecution.h"

#include "AbilitySystemComponent.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayEffects.h"
#include "Combat/VaesenGameplayTags.h"
#include "Items/VaesenWeaponItem.h"
#include "GameFramework/Character.h"
#include "Weapons/VaesenWeaponActor.h"

namespace VaesenDamageCapture
{
	struct FStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(ProtectionSlashing);
		DECLARE_ATTRIBUTE_CAPTUREDEF(ProtectionPiercing);
		DECLARE_ATTRIBUTE_CAPTUREDEF(ProtectionBlunt);

		FStatics()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UVaesenAttributeSet, ProtectionSlashing, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UVaesenAttributeSet, ProtectionPiercing, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UVaesenAttributeSet, ProtectionBlunt, Target, false);
		}
	};

	const FStatics& Get()
	{
		static FStatics Statics;
		return Statics;
	}
}

UVaesenDamageExecution::UVaesenDamageExecution()
{
	const VaesenDamageCapture::FStatics& Statics = VaesenDamageCapture::Get();
	RelevantAttributesToCapture.Add(Statics.ProtectionSlashingDef);
	RelevantAttributesToCapture.Add(Statics.ProtectionPiercingDef);
	RelevantAttributesToCapture.Add(Statics.ProtectionBluntDef);
}

void UVaesenDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const VaesenDamageCapture::FStatics& Statics = VaesenDamageCapture::Get();
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	AActor* TargetActor = TargetASC ? TargetASC->GetAvatarActor() : nullptr;
	if (!TargetASC || !TargetActor)
	{
		return;
	}
	if (TargetASC->HasMatchingGameplayTag(VaesenTags::State_Dead))
	{
		return;
	}

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	EvalParams.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// The packet: every Damage.* SetByCaller on the spec.
	FVaesenDamagePacket Packet;
	for (const TPair<FGameplayTag, float>& Pair : Spec.SetByCallerTagMagnitudes)
	{
		if (Pair.Key.MatchesTag(VaesenTags::Damage) && Pair.Value > 0.0f)
		{
			Packet.Entries.Emplace(Pair.Key, Pair.Value);
		}
	}
	if (Packet.IsEmpty())
	{
		return;
	}

	const auto Capture = [&](const FGameplayEffectAttributeCaptureDefinition& Def)
	{
		float Value = 0.0f;
		ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(Def, EvalParams, Value);
		return Value;
	};
	const float Slashing = Capture(Statics.ProtectionSlashingDef);
	const float Piercing = Capture(Statics.ProtectionPiercingDef);
	const float Blunt = Capture(Statics.ProtectionBluntDef);
	const auto GetProtection = [Slashing, Piercing, Blunt](const FGameplayTag& DamageType)
	{
		if (DamageType.MatchesTag(VaesenTags::Damage_Slashing)) { return Slashing; }
		if (DamageType.MatchesTag(VaesenTags::Damage_Piercing)) { return Piercing; }
		if (DamageType.MatchesTag(VaesenTags::Damage_Blunt))    { return Blunt; }
		return 0.0f;
	};

	// Blocking: facing the attacker, and with something that can stop this kind of hit.
	const FGameplayEffectContextHandle& Context = Spec.GetContext();
	const bool bUnblockable = Spec.GetSetByCallerMagnitude(VaesenTags::Data_Unblockable, false, 0.0f) > 0.5f;
	const bool bProjectile = Spec.GetSetByCallerMagnitude(VaesenTags::Data_Projectile, false, 0.0f) > 0.5f;
	const FVector From = Context.HasOrigin() ? Context.GetOrigin()
		: Context.GetInstigator() ? Context.GetInstigator()->GetActorLocation() : TargetActor->GetActorLocation();

	UVaesenWeaponItem* BlockingItem = nullptr;
	const FVaesenBlockProfile* Block = nullptr;
	bool bParried = false;
	if (!bUnblockable && TargetASC->HasMatchingGameplayTag(VaesenTags::State_Blocking))
	{
		const FVaesenBlockProfile* Profile = UVaesenCombatLibrary::FindBlockProfile(TargetActor, bProjectile, BlockingItem);
		if (Profile && VaesenDamageMath::IsInFront(TargetActor->GetActorLocation(), TargetActor->GetActorForwardVector(), From, Profile->BlockHalfAngle))
		{
			Block = Profile;
			// Parrying works on melee only.
			bParried = !bProjectile && TargetASC->HasMatchingGameplayTag(VaesenTags::State_ParryWindow);
		}
	}

	FVaesenDamageReport Report = VaesenDamageMath::Resolve(Packet, GetProtection, Block, bParried);
	if (const FHitResult* Hit = Context.GetHitResult())
	{
		Report.Location = Hit->ImpactPoint.IsZero() ? Hit->Location : Hit->ImpactPoint;
	}
	else
	{
		Report.Location = TargetActor->GetActorLocation();
	}

	if (Report.TotalFinal > 0.0f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UVaesenAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Report.TotalFinal));
	}

	// What the block took wears the blocking weapon (a parry costs nothing).
	if (BlockingItem && Report.Outcome == EVaesenHitOutcome::Blocked && Report.TotalAbsorbed > 0.0f)
	{
		BlockingItem->ApplyWear(Report.TotalAbsorbed);
	}
	if (AVaesenWeaponActor* BlockingActor = BlockingItem && Block ? BlockingItem->GetWeaponActor() : nullptr)
	{
		// Only once the server has decided: nobody hears a block that didn't happen.
		BlockingActor->MulticastPlaySound(bParried ? EVaesenWeaponSound::Parry : EVaesenWeaponSound::Block);
	}
	if (bParried && SourceASC && SourceASC != TargetASC)
	{
		const FGameplayEffectSpecHandle Stagger = SourceASC->MakeOutgoingSpec(UVaesenGE_Stagger::StaticClass(), 1.0f, TargetASC->MakeEffectContext());
		if (Stagger.IsValid())
		{
			SourceASC->ApplyGameplayEffectSpecToSelf(*Stagger.Data.Get());
		}
	}

	// Knockback (a heavy club, a shield bash): pushed away from the attacker; half when blocked, none when parried.
	const float Knockback = Spec.GetSetByCallerMagnitude(VaesenTags::Data_Knockback, false, 0.0f);
	if (ACharacter* Pushed = Cast<ACharacter>(TargetActor); Pushed && Knockback > 0.0f && Report.Outcome != EVaesenHitOutcome::Parried)
	{
		const FVector Away = (TargetActor->GetActorLocation() - From).GetSafeNormal2D();
		const float Strength = Report.Outcome == EVaesenHitOutcome::Blocked ? Knockback * 0.5f : Knockback;
		Pushed->LaunchCharacter(Away * Strength + FVector(0.0, 0.0, Strength * 0.25f), true, false);
	}

	// Everyone sees and hears it: blood, sparks on a block, the ring of a parry.
	FGameplayCueParameters Cue;
	Cue.Location = Report.Location;
	Cue.EffectContext = Context;
	Cue.RawMagnitude = Report.TotalFinal;
	Cue.AggregatedSourceTags.AddTag(UVaesenCombatLibrary::GetMainDamageType(Packet));
	if (const FHitResult* Hit = Context.GetHitResult())
	{
		Cue.Normal = Hit->ImpactNormal;
		Cue.PhysicalMaterial = Hit->PhysMaterial;
	}
	const FGameplayTag CueTag = Report.Outcome == EVaesenHitOutcome::Parried ? FGameplayTag(VaesenTags::GameplayCue_Hit_Parried)
		: Report.Outcome == EVaesenHitOutcome::Blocked ? FGameplayTag(VaesenTags::GameplayCue_Hit_Blocked)
		: VaesenTags::GetHitCueFor(UVaesenCombatLibrary::GetMainDamageType(Packet));
	TargetASC->ExecuteGameplayCue(CueTag, Cue);

	if (UVaesenAbilitySystemComponent* VaesenTarget = Cast<UVaesenAbilitySystemComponent>(TargetASC))
	{
		VaesenTarget->ReportDamageTaken(Report, SourceASC);
	}
}
