#include "Weapons/VaesenWeaponData.h"

#include "Combat/VaesenGameplayTags.h"
#include "Core/VaesenSettings.h"

const FVaesenImpactEntry* UVaesenImpactEffects::FindEntry(const FGameplayTag& DamageType, EPhysicalSurface Surface, EVaesenImpactDelivery Delivery) const
{
	const FVaesenImpactEntry* Best = nullptr;
	int32 BestScore = -1;
	for (const FVaesenImpactEntry& Entry : Entries)
	{
		const bool bAnyType = !Entry.DamageType.IsValid();
		const bool bAnySurface = Entry.Surface == SurfaceType_Default;
		const bool bAnyDelivery = Entry.Delivery == EVaesenImpactDelivery::Any;
		if ((!bAnyType && !DamageType.MatchesTag(Entry.DamageType)) || (!bAnySurface && Entry.Surface != Surface)
			|| (!bAnyDelivery && Entry.Delivery != Delivery))
		{
			continue;
		}
		// Its delivery (4) first; then type and surface (3), surface only (2), type only (1), anything (0).
		const int32 Score = (bAnyDelivery ? 0 : 4) + (bAnySurface ? 0 : 2) + (bAnyType ? 0 : 1);
		if (Score > BestScore)
		{
			Best = &Entry;
			BestScore = Score;
		}
	}
	return Best;
}

namespace VaesenWeaponDefaults
{
	FVaesenMeleeAttack MakeAttack(const FGameplayTag& DamageType, float Amount, float Reach, float Stamina, float Duration, float WindowStart, float WindowEnd)
	{
		FVaesenMeleeAttack Attack;
		Attack.Damage.Entries.Emplace(DamageType, Amount);
		Attack.Reach = Reach;
		Attack.StaminaCost = Stamina;
		Attack.Duration = Duration;
		Attack.HitWindowStart = WindowStart;
		Attack.HitWindowEnd = WindowEnd;
		return Attack;
	}

	FVaesenBlockProfile MakeBlock(float Slashing, float Piercing, float Blunt, float ParryWindow, bool bProjectiles = false)
	{
		FVaesenBlockProfile Block;
		Block.Reduction.Add(VaesenTags::Damage_Slashing, Slashing);
		Block.Reduction.Add(VaesenTags::Damage_Piercing, Piercing);
		Block.Reduction.Add(VaesenTags::Damage_Blunt, Blunt);
		Block.ParryWindow = ParryWindow;
		Block.bCanBlockProjectiles = bProjectiles;
		return Block;
	}
}

UVaesenWeaponCategoryDefaults::UVaesenWeaponCategoryDefaults()
{
	SetBuiltInDefaults();
}

void UVaesenWeaponCategoryDefaults::SetBuiltInDefaults()
{
	using namespace VaesenWeaponDefaults;
	const FGameplayTag Slashing = VaesenTags::Damage_Slashing;
	const FGameplayTag Piercing = VaesenTags::Damage_Piercing;
	const FGameplayTag Blunt = VaesenTags::Damage_Blunt;

	const auto Add = [this](EVaesenWeaponCategory Category, const FVaesenMeleeAttack& Light, const FVaesenMeleeAttack* Heavy, const FVaesenBlockProfile& Block)
	{
		FVaesenCategoryDefaults& Entry = Categories.FindOrAdd(Category);
		Entry.LightAttack = Light;
		Entry.bHasHeavyAttack = Heavy != nullptr;
		if (Heavy)
		{
			Entry.HeavyAttack = *Heavy;
		}
		Entry.BlockProfile = Block;
	};
	const auto SetKnockback = [this](EVaesenWeaponCategory Category, float Light, float Heavy)
	{
		FVaesenCategoryDefaults& Entry = Categories.FindOrAdd(Category);
		Entry.LightAttack.Knockback = Light;
		Entry.HeavyAttack.Knockback = Heavy;
	};

	// Grey-box numbers: readable differences between weapons, not balance.
	//                                        type      dmg  reach stam  dur  window
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Blunt, 7, 90, 12, 0.8f, 0.30f, 0.50f);
	  Add(EVaesenWeaponCategory::Unarmed, MakeAttack(Blunt, 4, 90, 5, 0.5f, 0.15f, 0.30f), &Heavy, MakeBlock(0.15f, 0.1f, 0.25f, 0.15f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Piercing, 11, 85, 12, 0.6f, 0.20f, 0.35f);
	  Add(EVaesenWeaponCategory::Dagger, MakeAttack(Slashing, 5, 85, 6, 0.45f, 0.12f, 0.28f), &Heavy, MakeBlock(0.4f, 0.3f, 0.1f, 0.3f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Piercing, 16, 130, 18, 0.9f, 0.35f, 0.55f);
	  Add(EVaesenWeaponCategory::OneHandedSword, MakeAttack(Slashing, 12, 120, 10, 0.7f, 0.20f, 0.45f), &Heavy, MakeBlock(0.7f, 0.5f, 0.25f, 0.25f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Slashing, 22, 110, 22, 1.2f, 0.55f, 0.75f);
	  Add(EVaesenWeaponCategory::OneHandedAxe, MakeAttack(Slashing, 15, 110, 14, 0.9f, 0.35f, 0.55f), &Heavy, MakeBlock(0.4f, 0.3f, 0.3f, 0.15f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Blunt, 16, 110, 18, 1.0f, 0.40f, 0.60f);
	  Add(EVaesenWeaponCategory::Club, MakeAttack(Blunt, 10, 110, 10, 0.75f, 0.25f, 0.45f), &Heavy, MakeBlock(0.3f, 0.2f, 0.4f, 0.1f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Piercing, 18, 110, 22, 1.1f, 0.45f, 0.65f);
	  Add(EVaesenWeaponCategory::Warhammer, MakeAttack(Blunt, 14, 110, 14, 0.9f, 0.35f, 0.55f), &Heavy, MakeBlock(0.3f, 0.3f, 0.4f, 0.12f)); }
	{ const FVaesenMeleeAttack Heavy = MakeAttack(Blunt, 11, 100, 14, 0.9f, 0.35f, 0.55f);
	  Add(EVaesenWeaponCategory::Improvised, MakeAttack(Blunt, 7, 100, 8, 0.7f, 0.25f, 0.45f), &Heavy, MakeBlock(0.25f, 0.25f, 0.25f, 0.1f)); }
	Add(EVaesenWeaponCategory::Shield, MakeAttack(Blunt, 4, 80, 8, 0.6f, 0.20f, 0.35f), nullptr, MakeBlock(0.8f, 0.8f, 0.8f, 0.2f, true));
	Add(EVaesenWeaponCategory::Pistol, MakeAttack(Blunt, 5, 80, 6, 0.6f, 0.20f, 0.35f), nullptr, MakeBlock(0.0f, 0.0f, 0.0f, 0.0f));
	Add(EVaesenWeaponCategory::Revolver, MakeAttack(Blunt, 5, 80, 6, 0.6f, 0.20f, 0.35f), nullptr, MakeBlock(0.0f, 0.0f, 0.0f, 0.0f));
	Add(EVaesenWeaponCategory::Rifle, MakeAttack(Blunt, 8, 110, 10, 0.8f, 0.30f, 0.50f), nullptr, MakeBlock(0.3f, 0.2f, 0.3f, 0.1f));
	Add(EVaesenWeaponCategory::Bow, MakeAttack(Blunt, 3, 90, 6, 0.6f, 0.20f, 0.35f), nullptr, MakeBlock(0.1f, 0.1f, 0.1f, 0.0f));
	Add(EVaesenWeaponCategory::Crossbow, MakeAttack(Blunt, 6, 100, 8, 0.7f, 0.25f, 0.45f), nullptr, MakeBlock(0.2f, 0.2f, 0.2f, 0.05f));
	// Handguns can't parry: too small (the forearm would take it). Long guns and bows can (Ctrl + right mouse; right
	// mouse alone aims), but they aren't made for it: blocking wears them four times as hard, and even a parry wears
	// them. The rest parry once they have the guard animation.
	for (const EVaesenWeaponCategory Handgun : { EVaesenWeaponCategory::Pistol, EVaesenWeaponCategory::Revolver })
	{
		Categories.FindOrAdd(Handgun).bCanParry = false;
	}
	for (const EVaesenWeaponCategory Long : { EVaesenWeaponCategory::Rifle, EVaesenWeaponCategory::Bow, EVaesenWeaponCategory::Crossbow })
	{
		FVaesenCategoryDefaults& Entry = Categories.FindOrAdd(Long);
		// No guard animation for them yet: held across the chest procedurally.
		Entry.bProceduralGuard = true;
		FVaesenBlockProfile& Block = Entry.BlockProfile;
		Block.WearScale = 4.0f;
		Block.ParryWearShare = 0.5f;
	}

	// Heavy blunt blows and shield bashes push (cm/s).
	SetKnockback(EVaesenWeaponCategory::Unarmed, 0.0f, 150.0f);
	SetKnockback(EVaesenWeaponCategory::Club, 0.0f, 350.0f);
	SetKnockback(EVaesenWeaponCategory::Warhammer, 100.0f, 400.0f);
	SetKnockback(EVaesenWeaponCategory::OneHandedAxe, 0.0f, 250.0f);
	SetKnockback(EVaesenWeaponCategory::Shield, 300.0f, 0.0f);
	SetKnockback(EVaesenWeaponCategory::Rifle, 200.0f, 0.0f);

	// Where weapons hang (sockets on the mannequin skeleton; the commandlet adds them). Until a socket exists the
	// weapon hangs from the fallback bone at the fallback offset. Back and hip draws are different motions, so each
	// location has its own montage, shared by every weapon carried there.
	struct FLocation { const TCHAR* Name; const TCHAR* Bone; FVector Offset; FRotator Rotation; float Duration; };
	const FLocation Locations[] = {
		{ TEXT("Sheath_BackUpper"), TEXT("spine_05"), FVector(0.0, -18.0, 0.0), FRotator(0.0, 0.0, 200.0), 0.6f },
		{ TEXT("Sheath_BackLower"), TEXT("spine_02"), FVector(0.0, -18.0, 0.0), FRotator(0.0, 0.0, 160.0), 0.6f },
		{ TEXT("Sheath_BackSling"), TEXT("spine_04"), FVector(0.0, -18.0, 0.0), FRotator(45.0, 0.0, 180.0), 0.7f },
		{ TEXT("Sheath_HipLeft"), TEXT("pelvis"), FVector(0.0, 0.0, 18.0), FRotator(0.0, 0.0, 100.0), 0.45f },
		{ TEXT("Sheath_HipRight"), TEXT("pelvis"), FVector(0.0, 0.0, -18.0), FRotator(0.0, 0.0, 80.0), 0.4f },
		{ TEXT("Sheath_Belt"), TEXT("pelvis"), FVector(0.0, 12.0, 10.0), FRotator(0.0, 0.0, 90.0), 0.4f },
	};
	for (const FLocation& Location : Locations)
	{
		FVaesenSheathLocation& Entry = SheathLocations.FindOrAdd(Location.Name);
		Entry.FallbackBone = Location.Bone;
		Entry.FallbackOffset = FTransform(Location.Rotation, Location.Offset);
		Entry.Duration = Location.Duration;
		Entry.GrabTime = Location.Duration * 0.45f;
	}
}

const UVaesenWeaponCategoryDefaults* UVaesenWeaponCategoryDefaults::Get()
{
	static TWeakObjectPtr<const UVaesenWeaponCategoryDefaults> Cached;
	if (const UVaesenWeaponCategoryDefaults* Loaded = Cached.Get())
	{
		return Loaded;
	}

	const UVaesenWeaponCategoryDefaults* Found = UVaesenSettings::Get()->WeaponCategoryDefaults.LoadSynchronous();
	if (!Found)
	{
		// The commandlet's asset, even before it's set in Project Settings.
		Found = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (Found)
	{
		Cached = Found;
		return Found;
	}
	return GetDefault<UVaesenWeaponCategoryDefaults>();
}

const FVaesenCategoryDefaults& UVaesenWeaponCategoryDefaults::GetCategory(EVaesenWeaponCategory Category) const
{
	if (const FVaesenCategoryDefaults* Found = Categories.Find(Category))
	{
		return *Found;
	}
	// An asset that leaves a category out still gets the built-in values for it.
	const UVaesenWeaponCategoryDefaults* BuiltIn = GetDefault<UVaesenWeaponCategoryDefaults>();
	if (BuiltIn != this)
	{
		return BuiltIn->GetCategory(Category);
	}
	static const FVaesenCategoryDefaults Empty;
	return Empty;
}

TArray<FName> UVaesenWeaponCategoryDefaults::GetSheathSocketNames() const
{
	TArray<FName> Names;
	SheathLocations.GetKeys(Names);
	return Names;
}
