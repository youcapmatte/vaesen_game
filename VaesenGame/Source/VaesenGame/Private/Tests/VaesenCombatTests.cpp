#include "Tests/VaesenCombatTestTypes.h"

#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenDamageTypes.h"
#include "Combat/VaesenGameplayTags.h"
#include "Combat/VaesenTrainingDummy.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Inventory/VaesenLootContainer.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "Misc/AutomationTest.h"
#include "AIController.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Items/VaesenPickup.h"
#include "Weapons/VaesenProjectile.h"
#include "Weapons/VaesenThrownItem.h"
#include "Abilities/VaesenMeleeAbilities.h"
#include "Anim/VaesenWeaponFitRig.h"
#include "Items/VaesenWeaponItem.h"
#include "Abilities/VaesenRangedAbilities.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponData.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VaesenCombatTests
{
	constexpr EAutomationTestFlags CombatTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	FVaesenDamagePacket Packet(std::initializer_list<FVaesenDamageEntry> Entries)
	{
		FVaesenDamagePacket Result;
		Result.Entries = Entries;
		return Result;
	}

	/** Protection: Slashing 5, Piercing 2, Blunt -3 (a weakness). */
	float ArmorProtection(const FGameplayTag& Type)
	{
		if (Type.MatchesTag(VaesenTags::Damage_Slashing)) { return 5.0f; }
		if (Type.MatchesTag(VaesenTags::Damage_Piercing)) { return 2.0f; }
		if (Type.MatchesTag(VaesenTags::Damage_Blunt))    { return -3.0f; }
		return 0.0f;
	}

	FVaesenBlockProfile SwordBlock()
	{
		FVaesenBlockProfile Block;
		Block.Reduction.Add(VaesenTags::Damage_Slashing, 0.7f);
		Block.Reduction.Add(VaesenTags::Damage_Blunt, 0.25f);
		return Block;
	}

	/** A small game world for tests that need actors (spawning, ability systems, inventories). */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		int32 FreeFireBefore = 0;

		FTestWorld()
		{
			// The ammo rules are under test: free fire off while it runs.
			if (IConsoleVariable* FreeFire = IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire")))
			{
				FreeFireBefore = FreeFire->GetInt();
				FreeFire->Set(0, ECVF_SetByCode);
			}
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("VaesenCombatTestWorld"));
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			// No game mode here to start play: begin it directly, so spawned actors get BeginPlay.
			World->GetWorldSettings()->NotifyBeginPlay();
		}

		~FTestWorld()
		{
			if (IConsoleVariable* FreeFire = IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire")))
			{
				FreeFire->Set(FreeFireBefore, ECVF_SetByCode);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		template <typename T>
		T* Spawn(const FVector& Location = FVector::ZeroVector, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<T>(T::StaticClass(), FTransform(Rotation, Location), Params);
		}

		/** A big floor just under the feet of characters standing at Z = 0 (controlled characters fall otherwise). */
		void AddFloor()
		{
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.0, 0.0, -146.0), FRotator::ZeroRotator);
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->SetActorScale3D(FVector(100.0, 100.0, 1.0));
		}

		/** Runs the world for Seconds (timers, ticks), in small steps. */
		void Run(float Seconds)
		{
			for (float Time = 0.0f; Time < Seconds; Time += 0.05f)
			{
				// Timers only tick once per engine frame.
				++GFrameCounter;
				World->Tick(LEVELTICK_All, 0.05f);
			}
		}
	};
}

using namespace VaesenCombatTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageProtectionTest, "Vaesen.Combat.Damage.Protection", CombatTestFlags)
bool FVaesenDamageProtectionTest::RunTest(const FString& Parameters)
{
	// Each entry is reduced by its own protection, never below zero; a weakness adds.
	const FVaesenDamageReport Report = VaesenDamageMath::Resolve(
		Packet({ { VaesenTags::Damage_Slashing, 12.0f }, { VaesenTags::Damage_Piercing, 1.0f }, { VaesenTags::Damage_Blunt, 4.0f } }), &ArmorProtection);
	TestEqual(TEXT("Three lines"), Report.Lines.Num(), 3);
	TestEqual(TEXT("Slashing 12 - 5"), Report.Lines[0].Final, 7.0f);
	TestEqual(TEXT("Piercing 1 - 2 stops at zero"), Report.Lines[1].Final, 0.0f);
	TestEqual(TEXT("Blunt 4 against a weakness of 3"), Report.Lines[2].Final, 7.0f);
	TestEqual(TEXT("Total"), Report.TotalFinal, 14.0f);
	TestEqual(TEXT("Outcome"), Report.Outcome, EVaesenHitOutcome::Hit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageUnprotectedTypeTest, "Vaesen.Combat.Damage.TypeWithoutProtection", CombatTestFlags)
bool FVaesenDamageUnprotectedTypeTest::RunTest(const FString& Parameters)
{
	// An iron blade: Slashing is reduced by armor, Iron isn't (no protection attribute for it).
	const FVaesenDamageReport Report = VaesenDamageMath::Resolve(
		Packet({ { VaesenTags::Damage_Slashing, 10.0f }, { VaesenTags::Damage_Iron, 4.0f } }), &ArmorProtection);
	TestEqual(TEXT("Slashing"), Report.Lines[0].Final, 5.0f);
	TestEqual(TEXT("Iron untouched"), Report.Lines[1].Final, 4.0f);
	TestEqual(TEXT("Total"), Report.TotalFinal, 9.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageBlockTest, "Vaesen.Combat.Damage.Block", CombatTestFlags)
bool FVaesenDamageBlockTest::RunTest(const FString& Parameters)
{
	const FVaesenBlockProfile Block = SwordBlock();
	const FVaesenDamageReport Report = VaesenDamageMath::Resolve(
		Packet({ { VaesenTags::Damage_Slashing, 15.0f }, { VaesenTags::Damage_Blunt, 8.0f }, { VaesenTags::Damage_Piercing, 6.0f } }),
		[](const FGameplayTag&) { return 0.0f; }, &Block);
	TestEqual(TEXT("Outcome"), Report.Outcome, EVaesenHitOutcome::Blocked);
	TestEqual(TEXT("Slashing: 70% absorbed"), Report.Lines[0].Final, 4.5f);
	TestEqual(TEXT("Blunt: 25% absorbed"), Report.Lines[1].Final, 6.0f);
	TestEqual(TEXT("Piercing: not in the profile, not reduced"), Report.Lines[2].Final, 6.0f);
	TestEqual(TEXT("Absorbed (wears the weapon)"), Report.TotalAbsorbed, 10.5f + 2.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageProtectionThenBlockTest, "Vaesen.Combat.Damage.ProtectionThenBlock", CombatTestFlags)
bool FVaesenDamageProtectionThenBlockTest::RunTest(const FString& Parameters)
{
	// Protection first, then the block takes its share of what's left.
	const FVaesenBlockProfile Block = SwordBlock();
	const FVaesenDamageReport Report = VaesenDamageMath::Resolve(Packet({ { VaesenTags::Damage_Slashing, 15.0f } }), &ArmorProtection, &Block);
	TestEqual(TEXT("After protection"), Report.Lines[0].AfterProtection, 10.0f);
	TestEqual(TEXT("Absorbed"), Report.Lines[0].Absorbed, 7.0f);
	TestEqual(TEXT("Final"), Report.Lines[0].Final, 3.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageParryTest, "Vaesen.Combat.Damage.Parry", CombatTestFlags)
bool FVaesenDamageParryTest::RunTest(const FString& Parameters)
{
	const FVaesenBlockProfile Block = SwordBlock();
	const FVaesenDamageReport Report = VaesenDamageMath::Resolve(Packet({ { VaesenTags::Damage_Slashing, 15.0f } }),
		[](const FGameplayTag&) { return 0.0f; }, &Block, true);
	TestEqual(TEXT("Outcome"), Report.Outcome, EVaesenHitOutcome::Parried);
	TestEqual(TEXT("No damage"), Report.TotalFinal, 0.0f);
	TestEqual(TEXT("No wear"), Report.TotalAbsorbed, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenBlockProfileTest, "Vaesen.Combat.Damage.BlockProfileLookup", CombatTestFlags)
bool FVaesenBlockProfileTest::RunTest(const FString& Parameters)
{
	FVaesenBlockProfile Block;
	Block.Reduction.Add(VaesenTags::Damage, 0.1f);
	Block.Reduction.Add(VaesenTags::Damage_Slashing, 0.8f);
	Block.Reduction.Add(VaesenTags::Damage_Blunt, 1.7f);
	TestEqual(TEXT("Most specific wins"), Block.GetReduction(VaesenTags::Damage_Slashing), 0.8f);
	TestEqual(TEXT("Parent covers the rest"), Block.GetReduction(VaesenTags::Damage_Fire), 0.1f);
	TestEqual(TEXT("Clamped to 1"), Block.GetReduction(VaesenTags::Damage_Blunt), 1.0f);
	TestEqual(TEXT("Unknown = none"), FVaesenBlockProfile().GetReduction(VaesenTags::Damage_Slashing), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenRangedParryTest, "Vaesen.Combat.Damage.RangedParry", CombatTestFlags)
bool FVaesenRangedParryTest::RunTest(const FString& Parameters)
{
	// The built-in defaults: handguns can't parry; long guns and bows can, and wear hard doing it; blades as before.
	const UVaesenWeaponCategoryDefaults* Defaults = GetDefault<UVaesenWeaponCategoryDefaults>();
	TestFalse(TEXT("Pistol can't parry"), Defaults->GetCategory(EVaesenWeaponCategory::Pistol).bCanParry);
	TestFalse(TEXT("Revolver can't parry"), Defaults->GetCategory(EVaesenWeaponCategory::Revolver).bCanParry);
	for (const EVaesenWeaponCategory Long : { EVaesenWeaponCategory::Rifle, EVaesenWeaponCategory::Bow, EVaesenWeaponCategory::Crossbow })
	{
		const FVaesenCategoryDefaults& Entry = Defaults->GetCategory(Long);
		TestTrue(TEXT("Long gun or bow can parry"), Entry.bCanParry);
		TestTrue(TEXT("Blocking wears it hard"), Entry.BlockProfile.WearScale > 1.0f);
		TestTrue(TEXT("Even a parry wears it"), Entry.BlockProfile.ParryWearShare > 0.0f);
	}
	const FVaesenBlockProfile& Sword = Defaults->GetCategory(EVaesenWeaponCategory::OneHandedSword).BlockProfile;
	TestEqual(TEXT("A sword wears as before"), Sword.WearScale, 1.0f);
	TestEqual(TEXT("A sword's parry costs nothing"), Sword.ParryWearShare, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFacingTest, "Vaesen.Combat.Damage.BlockFacing", CombatTestFlags)
bool FVaesenFacingTest::RunTest(const FString& Parameters)
{
	const FVector Blocker = FVector::ZeroVector;
	const FVector Forward = FVector::ForwardVector;
	TestTrue(TEXT("In front"), VaesenDamageMath::IsInFront(Blocker, Forward, FVector(100, 0, 0), 70.0f));
	TestTrue(TEXT("Front-left, within 70"), VaesenDamageMath::IsInFront(Blocker, Forward, FVector(100, 100, 0), 70.0f));
	TestFalse(TEXT("To the side"), VaesenDamageMath::IsInFront(Blocker, Forward, FVector(0, 100, 0), 70.0f));
	TestFalse(TEXT("Behind"), VaesenDamageMath::IsInFront(Blocker, Forward, FVector(-100, 0, 0), 70.0f));
	TestTrue(TEXT("Height doesn't matter"), VaesenDamageMath::IsInFront(Blocker, Forward, FVector(100, 0, 500), 70.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenPacketTest, "Vaesen.Combat.Damage.Packet", CombatTestFlags)
bool FVaesenPacketTest::RunTest(const FString& Parameters)
{
	FVaesenDamagePacket Damage = Packet({ { VaesenTags::Damage_Piercing, 20.0f } });
	Damage.Add(VaesenTags::Damage_Piercing, 5.0f);
	Damage.Add(VaesenTags::Damage_Blunt, 3.0f);
	TestEqual(TEXT("Same type adds up"), Damage.Entries.Num(), 2);
	TestEqual(TEXT("Total"), Damage.GetTotal(), 28.0f);
	TestEqual(TEXT("Scaled (half draw)"), Damage.Scaled(0.5f).GetTotal(), 14.0f);
	TestTrue(TEXT("Empty"), FVaesenDamagePacket().IsEmpty());
	TestEqual(TEXT("Main type"), UVaesenCombatLibrary::GetMainDamageType(Damage), FGameplayTag(VaesenTags::Damage_Piercing));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDurabilityTest, "Vaesen.Combat.Durability", CombatTestFlags)
bool FVaesenDurabilityTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Wear"), VaesenDamageMath::ApplyWear(100.0f, 12.5f), 87.5f);
	TestEqual(TEXT("Never below zero"), VaesenDamageMath::ApplyWear(5.0f, 50.0f), 0.0f);
	TestEqual(TEXT("Negative wear does nothing"), VaesenDamageMath::ApplyWear(5.0f, -3.0f), 5.0f);

	// A fresh item is whole; at zero it's broken.
	const UVaesenWeaponItem* Revolver = GetDefault<UVaesenTestRevolver>();
	TestEqual(TEXT("New = max"), Revolver->GetDurability(), Revolver->MaxDurability);
	TestFalse(TEXT("Not broken"), Revolver->IsBroken());
	TestFalse(TEXT("Not near breaking"), Revolver->IsNearBreaking());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenCategoryDefaultsTest, "Vaesen.Combat.CategoryDefaults", CombatTestFlags)
bool FVaesenCategoryDefaultsTest::RunTest(const FString& Parameters)
{
	// Without any asset every category still has an attack, and every sheath location exists.
	const UVaesenWeaponCategoryDefaults* Defaults = GetDefault<UVaesenWeaponCategoryDefaults>();
	for (const EVaesenWeaponCategory Category : TEnumRange<EVaesenWeaponCategory>())
	{
		const FVaesenCategoryDefaults& Entry = Defaults->GetCategory(Category);
		TestFalse(FString::Printf(TEXT("%s has a light attack"), *UEnum::GetValueAsString(Category)), Entry.LightAttack.Damage.IsEmpty());
		TestTrue(FString::Printf(TEXT("%s's hit window is inside its duration"), *UEnum::GetValueAsString(Category)),
			Entry.LightAttack.HitWindowEnd <= Entry.LightAttack.Duration && Entry.LightAttack.HitWindowStart < Entry.LightAttack.HitWindowEnd);
	}
	for (const TCHAR* Socket : { TEXT("Sheath_BackUpper"), TEXT("Sheath_BackLower"), TEXT("Sheath_BackSling"), TEXT("Sheath_HipLeft"), TEXT("Sheath_HipRight"), TEXT("Sheath_Belt") })
	{
		TestNotNull(FString::Printf(TEXT("Sheath location %s"), Socket), Defaults->FindSheathLocation(Socket));
	}
	TestTrue(TEXT("Shields block projectiles"), Defaults->GetCategory(EVaesenWeaponCategory::Shield).BlockProfile.bCanBlockProjectiles);
	TestFalse(TEXT("Swords don't"), Defaults->GetCategory(EVaesenWeaponCategory::OneHandedSword).BlockProfile.bCanBlockProjectiles);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenImpactTableTest, "Vaesen.Combat.ImpactTable", CombatTestFlags)
bool FVaesenImpactTableTest::RunTest(const FString& Parameters)
{
	UVaesenImpactEffects* Table = NewObject<UVaesenImpactEffects>();
	FVaesenImpactEntry& Any = Table->Entries.AddDefaulted_GetRef();
	FVaesenImpactEntry& Blade = Table->Entries.AddDefaulted_GetRef();
	Blade.DamageType = VaesenTags::Damage_Slashing;
	FVaesenImpactEntry& Metal = Table->Entries.AddDefaulted_GetRef();
	Metal.Surface = SurfaceType4;
	FVaesenImpactEntry& BladeOnMetal = Table->Entries.AddDefaulted_GetRef();
	BladeOnMetal.DamageType = VaesenTags::Damage_Slashing;
	BladeOnMetal.Surface = SurfaceType4;

	TestTrue(TEXT("Type and surface"), Table->FindEntry(VaesenTags::Damage_Slashing, SurfaceType4) == &Table->Entries[3]);
	TestTrue(TEXT("Surface beats type"), Table->FindEntry(VaesenTags::Damage_Blunt, SurfaceType4) == &Table->Entries[2]);
	TestTrue(TEXT("Type only"), Table->FindEntry(VaesenTags::Damage_Slashing, SurfaceType1) == &Table->Entries[1]);
	TestTrue(TEXT("Anything"), Table->FindEntry(VaesenTags::Damage_Blunt, SurfaceType1) == &Table->Entries[0]);
	TestNull(TEXT("Empty table"), NewObject<UVaesenImpactEffects>()->FindEntry(VaesenTags::Damage_Blunt, SurfaceType1));

	// By hand or shot: a sword's stab has its own row, a bullet (also piercing) never gets it.
	FVaesenImpactEntry& StabByHand = Table->Entries.AddDefaulted_GetRef();
	StabByHand.DamageType = VaesenTags::Damage_Piercing;
	StabByHand.Delivery = EVaesenImpactDelivery::ByHand;
	TestTrue(TEXT("By hand: its own row first"), Table->FindEntry(VaesenTags::Damage_Piercing, SurfaceType4, EVaesenImpactDelivery::ByHand) == &Table->Entries[4]);
	TestTrue(TEXT("Shot: never the by-hand row"), Table->FindEntry(VaesenTags::Damage_Piercing, SurfaceType4, EVaesenImpactDelivery::Shot) == &Table->Entries[2]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAmmoAcceptTest, "Vaesen.Combat.Ammo.Accepts", CombatTestFlags)
bool FVaesenAmmoAcceptTest::RunTest(const FString& Parameters)
{
	const UVaesenTestRevolver* Revolver = GetDefault<UVaesenTestRevolver>();
	const UVaesenTestRifle* Rifle = GetDefault<UVaesenTestRifle>();
	TestTrue(TEXT("Revolver takes its cartridge"), Revolver->Accepts(UVaesenTestCartridge::StaticClass()));
	TestFalse(TEXT("Revolver refuses other ammo"), Revolver->Accepts(UVaesenTestBullet::StaticClass()));
	TestFalse(TEXT("Not ammo at all"), Revolver->Accepts(UVaesenTestCartridgeBox::StaticClass()));
	TestTrue(TEXT("Rifle takes any Item.Ammo"), Rifle->Accepts(UVaesenTestBullet::StaticClass()) && Rifle->Accepts(UVaesenTestCartridge::StaticClass()));
	TestTrue(TEXT("Bolt action cycles"), Rifle->HasCycleStep());
	TestTrue(TEXT("Cylinder reloads can be interrupted"), Revolver->CanInterruptReload());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAmmoInventoryTest, "Vaesen.Combat.Ammo.CountingThroughPouches", CombatTestFlags)
bool FVaesenAmmoInventoryTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenLootContainer* Chest = TestWorld.Spawn<AVaesenLootContainer>();
	UVaesenInventoryComponent* Inventory = Chest ? Chest->GetInventory() : nullptr;
	if (!TestNotNull(TEXT("Inventory"), Inventory))
	{
		return false;
	}

	// A cartridge box first, so ammo picked up goes into it.
	Inventory->TryAddItemOfClass(UVaesenTestCartridgeBox::StaticClass(), 1);
	Inventory->TryAddItemOfClass(UVaesenTestCartridge::StaticClass(), 14);
	Inventory->TryAddItemOfClass(UVaesenTestBullet::StaticClass(), 3);
	TestEqual(TEXT("All cartridges counted (box included)"), Inventory->GetItemCount(UVaesenTestCartridge::StaticClass()), 14);

	const UVaesenTestRevolver* Revolver = GetDefault<UVaesenTestRevolver>();
	TestEqual(TEXT("Revolver counts its cartridges"), Revolver->CountAmmoIn(Inventory), 14);
	TestEqual(TEXT("Revolver loads cartridges"), Revolver->FindAmmoIn(Inventory).Get(), UVaesenTestCartridge::StaticClass());

	// Loading takes them out (pouch contents first).
	TestEqual(TEXT("Took 6"), Inventory->RemoveItemsOfClass(UVaesenTestCartridge::StaticClass(), 6), 6);
	TestEqual(TEXT("8 left"), Revolver->CountAmmoIn(Inventory), 8);
	TestEqual(TEXT("Can't take more than there is"), Inventory->RemoveItemsOfClass(UVaesenTestCartridge::StaticClass(), 20), 8);
	TestEqual(TEXT("None left"), Revolver->CountAmmoIn(Inventory), 0);
	TestNull(TEXT("Nothing to load"), Revolver->FindAmmoIn(Inventory).Get());

	// The rifle still finds the bullets by tag.
	TestEqual(TEXT("Rifle finds bullets"), GetDefault<UVaesenTestRifle>()->CountAmmoIn(Inventory), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenDamageExecutionTest, "Vaesen.Combat.Damage.ExecutionOnDummy", CombatTestFlags)
bool FVaesenDamageExecutionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>();
	if (!TestNotNull(TEXT("Dummy"), Dummy))
	{
		return false;
	}
	UVaesenAbilitySystemComponent* ASC = Dummy->GetVaesenAbilitySystem();
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetProtectionSlashingAttribute(), 4.0f);
	const float Before = ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());

	// Through GE_Damage and the execution, as every attack does: Slashing 10 - 4, Blunt 3 - 0.
	UVaesenCombatLibrary::ApplyDamagePacket(nullptr, Dummy, Packet({ { VaesenTags::Damage_Slashing, 10.0f }, { VaesenTags::Damage_Blunt, 3.0f } }), FHitResult(), nullptr);
	const float After = ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
	TestEqual(TEXT("Health lost"), Before - After, 9.0f);

	// Out of health: dead (and blocked from further damage) until it resets.
	UVaesenCombatLibrary::ApplyDamagePacket(nullptr, Dummy, Packet({ { VaesenTags::Damage_Blunt, 500.0f } }), FHitResult(), nullptr);
	TestEqual(TEXT("At zero"), ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()), 0.0f);
	TestTrue(TEXT("Dead"), ASC->IsDead());
	ASC->ResetVitals();
	TestFalse(TEXT("Reset"), ASC->IsDead());
	TestEqual(TEXT("Full again"), ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()), ASC->GetNumericAttribute(UVaesenAttributeSet::GetMaxHealthAttribute()));
	return true;
}

// --- Flow tests: a character in a small world, through the real equipment, weapon actors and damage execution ---

namespace VaesenCombatTests
{
	/** Equips an item of ItemClass from the character's own inventory. */
	UVaesenWeaponItem* GiveAndEquip(AVaesenPlayerCharacter* Character, TSubclassOf<UVaesenItem> ItemClass)
	{
		UVaesenInventoryComponent* Inventory = Character->GetInventory();
		Inventory->TryAddItemOfClass(ItemClass, 1);
		UVaesenItem* Item = Inventory->FindItemOfClass(ItemClass);
		Character->GetEquipment()->EquipFromInventory(Inventory, Item);
		for (const FVaesenEquippedItem& Entry : Character->GetEquipment()->GetEquippedItems())
		{
			if (Entry.Item && Entry.Item->IsA(ItemClass))
			{
				return Cast<UVaesenWeaponItem>(Entry.Item);
			}
		}
		return nullptr;
	}

	FVector EyesOf(const AActor* Actor)
	{
		FVector Location;
		FRotator Rotation;
		Actor->GetActorEyesViewPoint(Location, Rotation);
		return Location;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFlowRevolverTest, "Vaesen.Combat.Flow.EquipDrawLoadFire", CombatTestFlags)
bool FVaesenFlowRevolverTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = TestWorld.Spawn<AVaesenPlayerCharacter>();
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(400.0, 0.0, 0.0));
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Dummy"), Dummy))
	{
		return false;
	}

	// Equipped: the server spawned the weapon actor, sheathed.
	UVaesenRangedWeaponItem* Revolver = Cast<UVaesenRangedWeaponItem>(GiveAndEquip(Player, UVaesenTestRevolver::StaticClass()));
	if (!TestNotNull(TEXT("Revolver equipped"), Revolver))
	{
		return false;
	}
	AVaesenRangedWeaponActor* Actor = Cast<AVaesenRangedWeaponActor>(Revolver->GetWeaponActor());
	TestNotNull(TEXT("Weapon actor spawned"), Actor);
	UVaesenEquipmentComponent* Equipment = Player->GetEquipment();
	TestFalse(TEXT("Sheathed at first"), Equipment->IsWeaponDrawn());
	TestFalse(TEXT("Actor in the sheath"), Actor && Actor->IsInHand());
	TestEqual(TEXT("Inventory gave it up"), Player->GetInventory()->GetItemCount(UVaesenTestRevolver::StaticClass()), 0);

	// Draw (no montage: the timer fallback moves it at the grab time).
	Equipment->RequestToggleWeapon();
	TestFalse(TEXT("Not in the hand before the grab moment"), Equipment->IsWeaponDrawn());
	TestWorld.Run(1.0f);
	TestTrue(TEXT("Drawn"), Equipment->IsWeaponDrawn());
	TestTrue(TEXT("Revolver is the drawn weapon"), Equipment->GetDrawnWeapon() == Revolver);
	TestTrue(TEXT("Actor in the hand"), Actor && Actor->IsInHand());
	TestTrue(TEXT("State.WeaponDrawn"), Player->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_WeaponDrawn));
	if (!Actor)
	{
		return false;
	}

	// Empty: can't fire.
	TestEqual(TEXT("Empty"), Actor->GetFireBlock(), EVaesenFireBlock::Empty);

	// Load from the inventory.
	Player->GetInventory()->TryAddItemOfClass(UVaesenTestCartridge::StaticClass(), 8);
	TestEqual(TEXT("Loaded 6"), Actor->ServerLoadRounds(6), 6);
	TestEqual(TEXT("Rounds in"), Revolver->GetLoadedRounds(), 6);
	TestEqual(TEXT("2 left in the inventory"), Player->GetInventory()->GetItemCount(UVaesenTestCartridge::StaticClass()), 2);
	TestEqual(TEXT("Full: nothing more goes in"), Actor->ServerLoadRounds(6), 0);

	// Fire at the dummy: a round goes, the dummy takes the cartridge's Piercing 20.
	const float Before = Dummy->GetVaesenAbilitySystem()->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
	const FVector Eyes = EyesOf(Player);
	const FVector ToDummy = (Dummy->GetActorLocation() - Eyes).GetSafeNormal();
	TestTrue(TEXT("Fired"), Actor->ServerFire(Eyes, ToDummy));
	TestEqual(TEXT("A round used"), Revolver->GetLoadedRounds(), 5);
	const float After = Dummy->GetVaesenAbilitySystem()->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
	TestEqual(TEXT("Dummy hit for 20"), Before - After, 20.0f);

	// Too soon: the server ignores it.
	TestFalse(TEXT("Faster than the fire interval"), Actor->ServerFire(Eyes, ToDummy));
	TestEqual(TEXT("No round used"), Revolver->GetLoadedRounds(), 5);
	TestWorld.Run(Revolver->FireInterval + 0.1f);
	TestTrue(TEXT("Fires again after the interval"), Actor->ServerFire(Eyes, ToDummy));

	// An aim that doesn't start at the shooter is refused.
	TestWorld.Run(Revolver->FireInterval + 0.1f);
	TestFalse(TEXT("Implausible aim origin"), Actor->ServerFire(Eyes + FVector(5000.0, 0.0, 0.0), ToDummy));

	// Sheathe: back in its socket.
	Equipment->RequestSheathe();
	TestWorld.Run(1.0f);
	TestFalse(TEXT("Sheathed"), Equipment->IsWeaponDrawn());
	TestFalse(TEXT("Actor back in the sheath"), Actor->IsInHand());

	// Unequip: the actor goes; the item keeps its loaded rounds in the inventory.
	Equipment->UnequipToInventory(EVaesenEquipmentSlot::Sidearm);
	TestWorld.Run(0.1f);
	const UVaesenRangedWeaponItem* Back = Cast<UVaesenRangedWeaponItem>(Player->GetInventory()->FindItemOfClass(UVaesenTestRevolver::StaticClass()));
	TestTrue(TEXT("Back in the inventory with its rounds"), Back && Back->GetLoadedRounds() == 4);
	TestTrue(TEXT("No weapon actor on the inventory copy"), Back && Back->GetWeaponActor() == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFlowBoltActionTest, "Vaesen.Combat.Flow.BoltActionNeedsCycle", CombatTestFlags)
bool FVaesenFlowBoltActionTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = TestWorld.Spawn<AVaesenPlayerCharacter>();
	TestWorld.Spawn<AVaesenTrainingDummy>(FVector(400.0, 0.0, 0.0));
	UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(GiveAndEquip(Player, UVaesenTestRifle::StaticClass()));
	if (!TestNotNull(TEXT("Rifle"), Rifle))
	{
		return false;
	}
	AVaesenRangedWeaponActor* Actor = Cast<AVaesenRangedWeaponActor>(Rifle->GetWeaponActor());
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	Player->GetInventory()->TryAddItemOfClass(UVaesenTestBullet::StaticClass(), 5);
	TestEqual(TEXT("Loads by tag"), Actor->ServerLoadRounds(5), 5);

	const FVector Eyes = EyesOf(Player);
	TestTrue(TEXT("First shot"), Actor->ServerFire(Eyes, FVector::ForwardVector));
	TestTrue(TEXT("Spent case in the chamber"), Rifle->NeedsCycle());
	TestWorld.Run(1.0f);
	TestEqual(TEXT("Blocked until cycled"), Actor->GetFireBlock(), EVaesenFireBlock::NeedsCycle);
	TestFalse(TEXT("Can't fire"), Actor->ServerFire(Eyes, FVector::ForwardVector));
	Actor->ServerFinishCycle();
	TestFalse(TEXT("Cycled"), Rifle->NeedsCycle());
	TestTrue(TEXT("Fires after the bolt is worked"), Actor->ServerFire(Eyes, FVector::ForwardVector));
	TestEqual(TEXT("Two rounds used"), Rifle->GetLoadedRounds(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFlowBlockParryTest, "Vaesen.Combat.Flow.BlockParryDurability", CombatTestFlags)
bool FVaesenFlowBlockParryTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	// The blocker faces +X; one attacker in front of them, one behind.
	AVaesenPlayerCharacter* Blocker = TestWorld.Spawn<AVaesenPlayerCharacter>();
	AVaesenTrainingDummy* Front = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(150.0, 0.0, 0.0));
	AVaesenTrainingDummy* Behind = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(-150.0, 0.0, 0.0));
	UVaesenWeaponItem* Sword = GiveAndEquip(Blocker, UVaesenTestSword::StaticClass());
	if (!TestNotNull(TEXT("Sword"), Sword))
	{
		return false;
	}
	Blocker->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	TestTrue(TEXT("Sword drawn"), Blocker->GetEquipment()->GetDrawnWeapon() == Sword);

	UVaesenAbilitySystemComponent* ASC = Blocker->GetVaesenAbilitySystem();
	const auto Health = [ASC]() { return ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()); };
	const FVaesenDamagePacket Swing = Packet({ { VaesenTags::Damage_Slashing, 10.0f } });

	// Not blocking: all of it.
	float Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Unblocked"), Before - Health(), 10.0f);

	// Blocking from the front: the sword takes 70% of Slashing, and that much durability.
	ASC->AddLooseGameplayTag(VaesenTags::State_Blocking);
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Blocked"), Before - Health(), 3.0f);
	TestEqual(TEXT("Sword wore by what it absorbed"), Sword->GetDurability(), Sword->MaxDurability - 7.0f);

	// From behind: not blocked.
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Behind, Blocker, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Behind isn't blocked"), Before - Health(), 10.0f);

	// Unblockable: not blocked either.
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Swing, FHitResult(), nullptr, false);
	TestEqual(TEXT("Unblockable"), Before - Health(), 10.0f);

	// A projectile: a sword can't block it.
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Swing, FHitResult(), nullptr, true, true);
	TestEqual(TEXT("Swords don't block bullets"), Before - Health(), 10.0f);

	// Parry: nothing gets through, no wear, the attacker is staggered.
	ASC->AddLooseGameplayTag(VaesenTags::State_ParryWindow);
	const float DurabilityBefore = Sword->GetDurability();
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Parried"), Before - Health(), 0.0f);
	TestEqual(TEXT("No wear on a parry"), Sword->GetDurability(), DurabilityBefore);
	TestTrue(TEXT("Attacker staggered"), Front->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_Staggered));
	TestWorld.Run(1.2f);
	TestFalse(TEXT("Stagger wears off"), Front->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_Staggered));
	ASC->RemoveLooseGameplayTag(VaesenTags::State_ParryWindow);

	// Block until it breaks: broken, it can't block any more.
	for (int32 Hit = 0; Hit < 40 && !Sword->IsBroken(); ++Hit)
	{
		UVaesenCombatLibrary::ApplyDamagePacket(Front, Blocker, Packet({ { VaesenTags::Damage_Slashing, 30.0f } }), FHitResult(), nullptr);
		ASC->ResetVitals();
	}
	TestTrue(TEXT("Broke"), Sword->IsBroken());
	UVaesenWeaponItem* BlockingItem = nullptr;
	TestNull(TEXT("A broken weapon can't block"), UVaesenCombatLibrary::FindBlockProfile(Blocker, false, BlockingItem));
	TestTrue(TEXT("Still throwable"), Sword->CanBeThrown());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenFlowStaminaTest, "Vaesen.Combat.Flow.StaminaCostAndRegen", CombatTestFlags)
bool FVaesenFlowStaminaTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = TestWorld.Spawn<AVaesenPlayerCharacter>();
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	const auto Stamina = [ASC]() { return ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()); };

	// The default abilities were granted with their inputs.
	TestNotNull(TEXT("GA_MeleeLight granted"), ASC->FindAbilitySpecFromClass(UVaesenGA_MeleeLight::StaticClass()));
	TestNotNull(TEXT("GA_FireWeapon granted"), ASC->FindAbilitySpecFromClass(UVaesenGA_FireWeapon::StaticClass()));

	// Stamina regenerates (and stops at the max).
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 20.0f);
	TestWorld.Run(1.0f);
	TestTrue(TEXT("Regenerated"), Stamina() > 30.0f);
	TestWorld.Run(10.0f);
	TestEqual(TEXT("Up to max"), Stamina(), ASC->GetNumericAttribute(UVaesenAttributeSet::GetMaxStaminaAttribute()));

	// Paused while blocking.
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 20.0f);
	ASC->AddLooseGameplayTag(VaesenTags::State_Blocking);
	TestWorld.Run(1.0f);
	TestEqual(TEXT("No regen while blocking"), Stamina(), 20.0f);
	ASC->RemoveLooseGameplayTag(VaesenTags::State_Blocking);

	// A punch costs the unarmed light attack's stamina; at zero it doesn't start.
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 50.0f);
	const float Cost = UVaesenWeaponCategoryDefaults::Get()->GetCategory(EVaesenWeaponCategory::Unarmed).LightAttack.StaminaCost;
	TestTrue(TEXT("Punch activates"), ASC->TryActivateAbilityByClass(UVaesenGA_MeleeLight::StaticClass()));
	TestEqual(TEXT("Stamina paid"), Stamina(), 50.0f - Cost);
	TestWorld.Run(2.0f);
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 0.0f);
	TestFalse(TEXT("No stamina, no punch"), ASC->TryActivateAbilityByClass(UVaesenGA_MeleeLight::StaticClass()));
	return true;
}

// --- Ability tests: the real abilities, through the ability inputs, on an AI-controlled character (locally
// controlled on the server, so every ability runs its whole path: input, cost, aim, timers, effects) ---

namespace VaesenCombatTests
{
	/** A player character possessed by an AI controller, facing +X with a level view. */
	AVaesenPlayerCharacter* SpawnFighter(FTestWorld& TestWorld, const FVector& Location = FVector::ZeroVector)
	{
		TestWorld.AddFloor();
		AVaesenPlayerCharacter* Character = TestWorld.Spawn<AVaesenPlayerCharacter>(Location);
		AAIController* Controller = TestWorld.Spawn<AAIController>(Location);
		if (Character && Controller)
		{
			Controller->bSetControlRotationFromPawnOrientation = false;
			Controller->Possess(Character);
			Controller->SetControlRotation(FRotator::ZeroRotator);
		}
		return Character;
	}

	float HealthOf(const AVaesenTrainingDummy* Dummy)
	{
		return Dummy->GetVaesenAbilitySystem()->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
	}

	int32 CountActors(UWorld* World, TSubclassOf<AActor> Class)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World, Class); It; ++It)
		{
			Count += IsValid(*It) ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityRangedTest, "Vaesen.Combat.Abilities.ReloadFireInterrupt", CombatTestFlags)
bool FVaesenAbilityRangedTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(400.0, 0.0, 0.0));
	UVaesenRangedWeaponItem* Revolver = Cast<UVaesenRangedWeaponItem>(GiveAndEquip(Player, UVaesenTestRevolver::StaticClass()));
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	if (!TestNotNull(TEXT("Revolver"), Revolver) || !TestTrue(TEXT("Locally controlled on the server"), Player->IsLocallyControlled()))
	{
		return false;
	}

	// H draws (through GA_Holster).
	ASC->PressAbilityInput(EVaesenAbilityInput::Holster);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Holster);
	TestWorld.Run(1.0f);
	TestTrue(TEXT("H drew the revolver"), Player->GetEquipment()->GetDrawnWeapon() == Revolver);

	// Fire with nothing loaded: nothing.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Dry: dummy untouched"), HealthOf(Dummy), 100.0f);

	// R with no ammo: nothing starts.
	ASC->PressAbilityInput(EVaesenAbilityInput::Reload);
	TestFalse(TEXT("No ammo, no reload"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Reload);

	// R with ammo: a cylinder fills round by round.
	Player->GetInventory()->TryAddItemOfClass(UVaesenTestCartridge::StaticClass(), 10);
	ASC->PressAbilityInput(EVaesenAbilityInput::Reload);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Reload);
	TestTrue(TEXT("Reloading"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	TestWorld.Run(0.3f);
	TestEqual(TEXT("One round after one reload time"), Revolver->GetLoadedRounds(), 1);
	TestWorld.Run(1.5f);
	TestEqual(TEXT("Full"), Revolver->GetLoadedRounds(), 6);
	TestFalse(TEXT("Reload ended by itself"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	TestEqual(TEXT("4 cartridges left"), Player->GetInventory()->GetItemCount(UVaesenTestCartridge::StaticClass()), 4);

	// Fire: through the ability, the aim and the server.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Hit the dummy"), HealthOf(Dummy), 80.0f);
	TestEqual(TEXT("Round used"), Revolver->GetLoadedRounds(), 5);

	// Again at once: the fire interval holds it back.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Too soon"), Revolver->GetLoadedRounds(), 5);

	// Firing interrupts a cylinder reload between rounds.
	TestWorld.Run(Revolver->FireInterval + 0.05f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Fired again"), Revolver->GetLoadedRounds(), 4);
	TestWorld.Run(Revolver->FireInterval + 0.05f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Reload);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Reload);
	TestWorld.Run(0.25f);
	TestEqual(TEXT("A round in"), Revolver->GetLoadedRounds(), 5);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestFalse(TEXT("Reload interrupted"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	TestEqual(TEXT("And the shot went"), Revolver->GetLoadedRounds(), 4);

	// Aim: right mouse with a gun aims (and never blocks).
	ASC->PressAbilityInput(EVaesenAbilityInput::AimOrBlock);
	TestTrue(TEXT("Aiming"), ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming));
	TestFalse(TEXT("Not blocking"), ASC->HasMatchingGameplayTag(VaesenTags::State_Blocking));
	TestTrue(TEXT("Faces the aim"), Player->bUseControllerRotationYaw);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::AimOrBlock);
	TestFalse(TEXT("Aim released"), ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming));

	// Sheathing cancels a reload in progress.
	TestWorld.Run(0.5f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Reload);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Reload);
	TestTrue(TEXT("Reloading again"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	Player->GetEquipment()->RequestSheathe();
	TestFalse(TEXT("Sheathing stopped the reload"), ASC->HasMatchingGameplayTag(VaesenTags::State_Reloading));
	TestWorld.Run(1.0f);
	TestFalse(TEXT("Sheathed"), Player->GetEquipment()->IsWeaponDrawn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityBoltTest, "Vaesen.Combat.Abilities.AutoCycle", CombatTestFlags)
bool FVaesenAbilityBoltTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(GiveAndEquip(Player, UVaesenTestRifle::StaticClass()));
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	Player->GetInventory()->TryAddItemOfClass(UVaesenTestBullet::StaticClass(), 5);
	ASC->PressAbilityInput(EVaesenAbilityInput::Reload);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Reload);
	TestWorld.Run(Rifle->ReloadTime + 0.1f);
	TestEqual(TEXT("Magazine loaded at once"), Rifle->GetLoadedRounds(), 5);

	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Fired"), Rifle->GetLoadedRounds(), 4);
	TestWorld.Run(0.1f);
	TestTrue(TEXT("The bolt is worked by itself"), ASC->HasMatchingGameplayTag(VaesenTags::State_Cycling));
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Can't fire while cycling"), Rifle->GetLoadedRounds(), 4);
	TestWorld.Run(Rifle->CycleTime + 0.1f);
	TestFalse(TEXT("Cycled"), Rifle->NeedsCycle());
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Fires after the cycle"), Rifle->GetLoadedRounds(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityMeleeTest, "Vaesen.Combat.Abilities.MeleeLightHeavyPunch", CombatTestFlags)
bool FVaesenAbilityMeleeTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(100.0, 0.0, 0.0));
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	const auto Stamina = [ASC]() { return ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()); };
	const UVaesenWeaponCategoryDefaults* Defaults = UVaesenWeaponCategoryDefaults::Get();

	// Bare hands: a punch.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestTrue(TEXT("Punching"), ASC->HasMatchingGameplayTag(VaesenTags::State_Attacking));
	TestWorld.Run(1.0f);
	TestEqual(TEXT("Punch"), 100.0f - HealthOf(Dummy), Defaults->GetCategory(EVaesenWeaponCategory::Unarmed).LightAttack.Damage.GetTotal());
	TestFalse(TEXT("Punch over"), ASC->HasMatchingGameplayTag(VaesenTags::State_Attacking));
	Dummy->GetVaesenAbilitySystem()->ResetVitals();

	// A sword: light (Slashing), with stamina and durability.
	UVaesenWeaponItem* Sword = GiveAndEquip(Player, UVaesenTestSword::StaticClass());
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 100.0f);
	const FVaesenCategoryDefaults& SwordDefaults = Defaults->GetCategory(EVaesenWeaponCategory::OneHandedSword);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Light stamina"), Stamina(), 100.0f - SwordDefaults.LightAttack.StaminaCost);
	TestWorld.Run(SwordDefaults.LightAttack.Duration + SwordDefaults.LightAttack.Cooldown + 0.1f);
	TestEqual(TEXT("Sword light"), 100.0f - HealthOf(Dummy), SwordDefaults.LightAttack.Damage.GetTotal());
	TestEqual(TEXT("Sword wore"), Sword->GetDurability(), Sword->MaxDurability - SwordDefaults.LightAttack.DurabilityCostPerHit);
	Dummy->GetVaesenAbilitySystem()->ResetVitals();

	// Heavy (Shift + left mouse).
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->PressAbilityInput(EVaesenAbilityInput::HeavyAttack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::HeavyAttack);
	TestEqual(TEXT("Heavy stamina"), Stamina(), 100.0f - SwordDefaults.HeavyAttack.StaminaCost);
	TestWorld.Run(SwordDefaults.HeavyAttack.Duration + 0.2f);
	TestEqual(TEXT("Sword heavy"), 100.0f - HealthOf(Dummy), SwordDefaults.HeavyAttack.Damage.GetTotal());
	Dummy->GetVaesenAbilitySystem()->ResetVitals();

	// No stamina: no attack.
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 0.0f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestFalse(TEXT("No stamina, no swing"), ASC->HasMatchingGameplayTag(VaesenTags::State_Attacking));

	// Broken: no attack either.
	ASC->SetNumericAttributeBase(UVaesenAttributeSet::GetStaminaAttribute(), 100.0f);
	Sword->ApplyWear(1000.0f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestFalse(TEXT("Broken, no swing"), ASC->HasMatchingGameplayTag(VaesenTags::State_Attacking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityBlockTest, "Vaesen.Combat.Abilities.BlockAndParryInput", CombatTestFlags)
bool FVaesenAbilityBlockTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	AVaesenTrainingDummy* Attacker = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(150.0, 0.0, 0.0));
	UVaesenWeaponItem* Sword = GiveAndEquip(Player, UVaesenTestSword::StaticClass());
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	const auto Health = [ASC]() { return ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()); };
	const FVaesenDamagePacket Swing = Packet({ { VaesenTags::Damage_Slashing, 10.0f } });
	const float BaseSpeed = Player->GetCharacterMovement()->MaxWalkSpeed;

	// Right mouse with a sword: block, and at first the parry window.
	ASC->PressAbilityInput(EVaesenAbilityInput::AimOrBlock);
	TestTrue(TEXT("Blocking"), ASC->HasMatchingGameplayTag(VaesenTags::State_Blocking));
	TestTrue(TEXT("Parry window"), ASC->HasMatchingGameplayTag(VaesenTags::State_ParryWindow));
	TestTrue(TEXT("Slower while blocking"), Player->GetCharacterMovement()->MaxWalkSpeed < BaseSpeed);
	float Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Attacker, Player, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Parried"), Before - Health(), 0.0f);
	TestTrue(TEXT("Attacker staggered"), Attacker->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_Staggered));

	// After the window: a plain block.
	TestWorld.Run(0.5f);
	TestFalse(TEXT("Window closed"), ASC->HasMatchingGameplayTag(VaesenTags::State_ParryWindow));
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Attacker, Player, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Blocked"), Before - Health(), 3.0f);
	TestEqual(TEXT("Wear"), Sword->GetDurability(), Sword->MaxDurability - 7.0f);

	// Holding block costs stamina and stops regen.
	const float StaminaBefore = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
	TestWorld.Run(1.0f);
	TestTrue(TEXT("Stamina drains while holding"), ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()) < StaminaBefore);

	// Release: no longer blocking, full speed again.
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::AimOrBlock);
	TestFalse(TEXT("Released"), ASC->HasMatchingGameplayTag(VaesenTags::State_Blocking));
	TestEqual(TEXT("Speed back"), Player->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed);
	Before = Health();
	UVaesenCombatLibrary::ApplyDamagePacket(Attacker, Player, Swing, FHitResult(), nullptr);
	TestEqual(TEXT("Not blocking any more"), Before - Health(), 10.0f);

	// Attacking out of a block drops it.
	ASC->PressAbilityInput(EVaesenAbilityInput::AimOrBlock);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	TestFalse(TEXT("Attack cancels the block"), ASC->HasMatchingGameplayTag(VaesenTags::State_Blocking));
	TestTrue(TEXT("Attacking"), ASC->HasMatchingGameplayTag(VaesenTags::State_Attacking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityThrowTest, "Vaesen.Combat.Abilities.Throw", CombatTestFlags)
bool FVaesenAbilityThrowTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(300.0, 0.0, 0.0));
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	Player->GetController()->SetControlRotation(FRotator(-10.0, 0.0, 0.0));

	// Nothing to throw: G does nothing.
	ASC->PressAbilityInput(EVaesenAbilityInput::Throw);
	TestFalse(TEXT("Nothing to throw"), ASC->HasMatchingGameplayTag(VaesenTags::State_Throwing));
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Throw);

	// A stone off the stack: aimed while held, thrown on release.
	Player->GetInventory()->TryAddItemOfClass(UVaesenTestStone::StaticClass(), 3);
	ASC->PressAbilityInput(EVaesenAbilityInput::Throw);
	TestTrue(TEXT("Aiming the throw"), ASC->HasMatchingGameplayTag(VaesenTags::State_Throwing));
	TestEqual(TEXT("Not thrown yet"), Player->GetInventory()->GetItemCount(UVaesenTestStone::StaticClass()), 3);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Throw);
	TestEqual(TEXT("One off the stack"), Player->GetInventory()->GetItemCount(UVaesenTestStone::StaticClass()), 2);
	TestEqual(TEXT("In flight"), CountActors(TestWorld.World, AVaesenThrownItem::StaticClass()), 1);
	TestWorld.Run(0.6f);
	TestEqual(TEXT("Hit the dummy (Blunt 6)"), 100.0f - HealthOf(Dummy), 6.0f);
	TestWorld.Run(9.0f);
	TestEqual(TEXT("Landed"), CountActors(TestWorld.World, AVaesenThrownItem::StaticClass()), 0);
	TestEqual(TEXT("Became a pickup"), CountActors(TestWorld.World, AVaesenPickup::StaticClass()), 1);

	// A drawn sword: unequipped first, then thrown.
	GiveAndEquip(Player, UVaesenTestSword::StaticClass());
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	ASC->PressAbilityInput(EVaesenAbilityInput::Throw);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Throw);
	TestNull(TEXT("Sword no longer equipped"), Player->GetEquipment()->GetItemInSlot(EVaesenEquipmentSlot::MainHand));
	TestFalse(TEXT("Nothing drawn"), Player->GetEquipment()->IsWeaponDrawn());
	TestEqual(TEXT("Stones untouched"), Player->GetInventory()->GetItemCount(UVaesenTestStone::StaticClass()), 2);
	TestEqual(TEXT("Sword in flight"), CountActors(TestWorld.World, AVaesenThrownItem::StaticClass()), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenAbilityBowTest, "Vaesen.Combat.Abilities.BowDraw", CombatTestFlags)
bool FVaesenAbilityBowTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	AVaesenTrainingDummy* Dummy = TestWorld.Spawn<AVaesenTrainingDummy>(FVector(400.0, 0.0, 0.0));
	UVaesenRangedWeaponItem* Bow = Cast<UVaesenRangedWeaponItem>(GiveAndEquip(Player, UVaesenTestBow::StaticClass()));
	UVaesenAbilitySystemComponent* ASC = Player->GetVaesenAbilitySystem();
	Player->GetEquipment()->RequestToggleWeapon();
	TestWorld.Run(1.0f);
	if (!TestTrue(TEXT("Bow drawn"), Player->GetEquipment()->GetDrawnWeapon() == Bow))
	{
		return false;
	}

	// No arrows: no draw.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	TestFalse(TEXT("No arrows"), ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow));
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);

	Player->GetInventory()->TryAddItemOfClass(UVaesenTestArrow::StaticClass(), 5);

	// Let go while still nocking (a click): a quick shot, nocked and drawn half way, then loosed by itself.
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	TestTrue(TEXT("Drawing"), ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow));
	TestWorld.Run(0.2f);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestTrue(TEXT("A click: still drawing"), ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow));
	TestEqual(TEXT("A click: no arrow yet"), CountActors(TestWorld.World, AVaesenProjectile::StaticClass()), 0);
	TestWorld.Run(Bow->NockTime + Bow->DrawTime * 0.5f + 0.2f);
	TestFalse(TEXT("A click: loosed"), ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow));
	TestEqual(TEXT("A click: an arrow used"), Player->GetInventory()->GetItemCount(UVaesenTestArrow::StaticClass()), 4);
	TestWorld.Run(1.0f);
	TestTrue(TEXT("A click: half a draw, a weaker hit"), 100.0f - HealthOf(Dummy) > 0.0f && 100.0f - HealthOf(Dummy) < 20.0f);
	Dummy->GetVaesenAbilitySystem()->ResetVitals();
	const int32 PickupsBefore = CountActors(TestWorld.World, AVaesenPickup::StaticClass());

	// Aimed, full draw and release: a strong shot at once (no raising first), that sticks and becomes a pickup.
	TestWorld.Run(Bow->FireInterval + 0.1f);
	const float StaminaBefore = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
	ASC->PressAbilityInput(EVaesenAbilityInput::AimOrBlock);
	ASC->PressAbilityInput(EVaesenAbilityInput::Attack);
	TestWorld.Run(Bow->NockTime + Bow->DrawTime + 0.2f);
	TestTrue(TEXT("Holding a drawn bow costs stamina"), ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()) < StaminaBefore);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::Attack);
	TestEqual(TEXT("Arrow used"), Player->GetInventory()->GetItemCount(UVaesenTestArrow::StaticClass()), 3);
	TestEqual(TEXT("Arrow in flight"), CountActors(TestWorld.World, AVaesenProjectile::StaticClass()), 1);
	TestWorld.Run(0.5f);
	TestEqual(TEXT("Full draw: full damage"), 100.0f - HealthOf(Dummy), 20.0f);
	TestEqual(TEXT("Stuck arrow became a pickup"), CountActors(TestWorld.World, AVaesenPickup::StaticClass()), PickupsBefore + 1);
	ASC->ReleaseAbilityInput(EVaesenAbilityInput::AimOrBlock);
	return true;
}

#endif

#if WITH_EDITOR
// --- Weapon Fit rig: the real anim Blueprint holds the weapon by its sockets; moving things moves the sockets ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenWeaponFitRigTest, "Vaesen.Combat.WeaponFitRig", CombatTestFlags)
bool FVaesenWeaponFitRigTest::RunTest(const FString& Parameters)
{
	UClass* Krag = LoadObject<UClass>(nullptr, TEXT("/Game/Vaesen/Blueprints/Items/Weapons/BP_Weapon_KragRifle.BP_Weapon_KragRifle_C"));
	if (!Krag)
	{
		AddInfo(TEXT("BP_Weapon_KragRifle not generated yet (run the asset commandlet); skipped."));
		return true;
	}
	FTestWorld TestWorld;
	AVaesenWeaponFitRig* Rig = TestWorld.Spawn<AVaesenWeaponFitRig>();
	Rig->Weapon = Krag;
	Rig->Pose = EVaesenFitPose::Aim;
	FPropertyChangedEvent Changed(nullptr);
	Rig->PostEditChangeProperty(Changed);

	// The mesh's sockets as they were (put back at the end: the test mustn't change the asset).
	UStaticMesh* Mesh = Rig->GetWeaponMeshHandle()->GetStaticMesh();
	if (!TestNotNull(TEXT("The rig holds the Krag's mesh"), Mesh))
	{
		return false;
	}
	TArray<TPair<FVector, FRotator>> Original;
	for (const UStaticMeshSocket* Socket : Mesh->Sockets)
	{
		Original.Emplace(Socket->RelativeLocation, Socket->RelativeRotation);
	}
	TestNotNull(TEXT("The Krag has a LeftGrip socket"), Mesh->FindSocket(TEXT("LeftGrip")));
	TestWorld.Run(0.5f);

	USceneComponent* Grip = Rig->GetGrip();
	const USkeletalMeshComponent* Body = Rig->GetBody();
	UVaesenFitHandleComponent* Weapon = Rig->GetWeaponMeshHandle();
	const auto Shouldered = [&](const TCHAR* When)
	{
		const FVector Muzzle = Weapon->GetSocketLocation(TEXT("Muzzle"));
		const FVector Stock = Weapon->GetSocketLocation(TEXT("Stock"));
		const float Along = FVector::DotProduct((Muzzle - Stock).GetSafeNormal(), Rig->GetAim().Vector());
		const float ToShoulder = FVector::Dist(Stock, UVaesenAnimInstance::ShoulderPocket(Body, Rig->GetAim()));
		const float BelowHead = Body->GetSocketLocation(TEXT("head")).Z - Stock.Z;
		const FTransform Sight = Weapon->GetSocketTransform(TEXT("Sight"));
		const FVector ToEye = UVaesenAnimInstance::EyePoint(Body) + Rig->GetAim().RotateVector(Rig->GetWeaponDefaults()->Hold.AimSightOffset) - Sight.GetLocation();
		const FVector Line = Sight.GetRotation().GetForwardVector();
		const float EyeGap = (ToEye - Line * FVector::DotProduct(ToEye, Line)).Size();
		TestTrue(FString::Printf(TEXT("%s: the sight line is at the eye (%.1f cm)"), When, EyeGap), EyeGap < 4.0f);
		TestTrue(FString::Printf(TEXT("%s: barrel along the aim (%.3f)"), When, Along), Along > 0.98f);
		TestTrue(FString::Printf(TEXT("%s: stock in the shoulder pocket (%.1f cm)"), When, ToShoulder), ToShoulder < 2.0f);
		UE_LOG(LogTemp, Display, TEXT("WeaponFitRig %s: barrel along the aim %.3f, stock %.1f cm from the shoulder pocket, %.0f cm below the head, sight line %.1f cm from the eye"), When, Along, ToShoulder, BelowHead, EyeGap);
	};
	// The left palm (hand_l_weapon) on LeftGrip, turned as it is.
	const auto LeftPalmOnGrip = [&](const TCHAR* When)
	{
		const FTransform Palm = Body->GetSocketTransform(TEXT("hand_l_weapon"));
		const FTransform LeftGrip = Weapon->GetSocketTransform(TEXT("LeftGrip"));
		const float Gap = FVector::Dist(Palm.GetLocation(), LeftGrip.GetLocation());
		const float Turn = FMath::RadiansToDegrees(Palm.GetRotation().AngularDistance(LeftGrip.GetRotation()));
		TestTrue(FString::Printf(TEXT("%s: the left palm is on LeftGrip (%.1f cm)"), When, Gap), Gap < 2.0f);
		TestTrue(FString::Printf(TEXT("%s: the left palm is turned as LeftGrip (%.0f degrees)"), When, Turn), Turn < 10.0f);
		UE_LOG(LogTemp, Display, TEXT("WeaponFitRig %s: left palm %.1f cm, %.0f degrees from LeftGrip"), When, Gap, Turn);
	};
	Shouldered(TEXT("Aimed"));
	LeftPalmOnGrip(TEXT("Aimed"));

	// Aimed: move the weapon 5 cm up; Stock moves on the weapon, and the weapon stays where it was put.
	const FVector Before = Grip->GetComponentLocation();
	const FVector StockBefore = Mesh->FindSocket(TEXT("Stock"))->RelativeLocation;
	Weapon->SetWorldLocation(Weapon->GetComponentLocation() + FVector(0.0, 0.0, 5.0));
	Rig->OnHandleMoved(Weapon, true);
	TestWorld.Run(0.3f);
	const float Raised = Grip->GetComponentLocation().Z - Before.Z;
	TestTrue(FString::Printf(TEXT("Moved up while aimed: it stays up (%.1f cm)"), Raised), FMath::Abs(Raised - 5.0f) < 1.5f);
	TestTrue(TEXT("Moving it while aimed moved the Stock socket (where the shoulder now is on it)"), !Mesh->FindSocket(TEXT("Stock"))->RelativeLocation.Equals(StockBefore, 0.5));
	Shouldered(TEXT("After the move"));

	// From the hip: slide the weapon 3 cm back in the hand; the Grip socket moves 3 cm forward on the weapon.
	Rig->Pose = EVaesenFitPose::Hip;
	Rig->PostEditChangeProperty(Changed);
	TestWorld.Run(0.5f);
	const FVector GripBefore = Mesh->FindSocket(TEXT("Grip"))->RelativeLocation;
	Weapon->SetWorldLocation(Weapon->GetComponentLocation() - Grip->GetForwardVector() * 3.0f);
	Rig->OnHandleMoved(Weapon, true);
	const float GripMoved = (Mesh->FindSocket(TEXT("Grip"))->RelativeLocation - GripBefore).Size();
	TestTrue(FString::Printf(TEXT("Slid in the hand: Grip moved %.1f cm on the weapon"), GripMoved), FMath::IsNearlyEqual(GripMoved, 3.0f, 0.3f));
	TestWorld.Run(0.3f);
	LeftPalmOnGrip(TEXT("From the hip"));

	// The LeftGrip handle: move it 4 cm toward the body and turn it; the socket follows, and the left hand with it.
	UVaesenFitHandleComponent* LeftHandle = Rig->GetSocketHandle(TEXT("LeftGrip"));
	const FVector LeftBefore = Mesh->FindSocket(TEXT("LeftGrip"))->RelativeLocation;
	LeftHandle->SetWorldLocation(LeftHandle->GetComponentLocation() - Grip->GetForwardVector() * 4.0f);
	LeftHandle->AddLocalRotation(FRotator(0.0, 0.0, 15.0));
	Rig->OnHandleMoved(LeftHandle, true);
	const float LeftMoved = (Mesh->FindSocket(TEXT("LeftGrip"))->RelativeLocation - LeftBefore).Size();
	TestTrue(FString::Printf(TEXT("LeftGrip moved %.1f cm"), LeftMoved), FMath::IsNearlyEqual(LeftMoved, 4.0f, 0.3f));
	TestWorld.Run(0.3f);
	LeftPalmOnGrip(TEXT("After moving LeftGrip"));

	for (int32 Index = 0; Index < Mesh->Sockets.Num() && Index < Original.Num(); ++Index)
	{
		Mesh->Sockets[Index]->RelativeLocation = Original[Index].Key;
		Mesh->Sockets[Index]->RelativeRotation = Original[Index].Value;
	}
	return true;
}
#endif

// --- Versatile grip: a sword in its natural grip with the off hand free, one hand with a shield, switched by V ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVaesenVersatileGripTest, "Vaesen.Combat.Melee.VersatileGrip", CombatTestFlags)
bool FVaesenVersatileGripTest::RunTest(const FString& Parameters)
{
	FTestWorld TestWorld;
	AVaesenPlayerCharacter* Player = SpawnFighter(TestWorld);
	UVaesenEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
	UVaesenMeleeWeaponItem* Sword = Equipment ? Cast<UVaesenMeleeWeaponItem>(GiveAndEquip(Player, UVaesenTestVersatileSword::StaticClass())) : nullptr;
	if (!TestNotNull(TEXT("A sword that can be held in both hands, equipped"), Sword))
	{
		return false;
	}
	// Its natural grip: both hands only when its category has two-handed animations; otherwise V takes both.
	const bool bNatural = Sword->PrefersBothHands();
	TestEqual(TEXT("Off hand free: held in its natural grip"), Sword->IsHeldTwoHanded(), bNatural);
	if (!bNatural)
	{
		Equipment->RequestToggleGrip();
	}
	TestTrue(TEXT("Off hand free (and V if needed): held in both hands"), Sword->IsHeldTwoHanded());

	// In both hands it hits harder (the category's two-handed scale) and costs more stamina.
	const FVaesenTwoHandedGrip& TwoHanded = Sword->GetCategoryDefaults().TwoHanded;
	FVaesenMeleeAttack InBoth;
	TestTrue(TEXT("It has a light attack"), Sword->GetAttack(false, InBoth));
	const float OneHandedDamage = Sword->GetCategoryDefaults().LightAttack.Damage.GetTotal();
	TestTrue(FString::Printf(TEXT("In both hands it hits harder (%.1f, one hand %.1f)"), InBoth.Damage.GetTotal(), OneHandedDamage),
		FMath::IsNearlyEqual(InBoth.Damage.GetTotal(), OneHandedDamage * TwoHanded.DamageScale, 0.01f));
	TestTrue(TEXT("...and costs more stamina"),
		FMath::IsNearlyEqual(InBoth.StaminaCost, Sword->GetCategoryDefaults().LightAttack.StaminaCost * TwoHanded.StaminaScale, 0.01f));

	// A shield in the off hand: one hand, and the plain attack.
	UVaesenWeaponItem* Buckler = GiveAndEquip(Player, UVaesenTestBuckler::StaticClass());
	TestNotNull(TEXT("A buckler equipped in the off hand"), Buckler);
	TestFalse(TEXT("With a shield: held in one hand"), Sword->IsHeldTwoHanded());
	FVaesenMeleeAttack InOne;
	Sword->GetAttack(false, InOne);
	TestTrue(TEXT("In one hand: the one-handed damage"), FMath::IsNearlyEqual(InOne.Damage.GetTotal(), OneHandedDamage, 0.01f));
	TestTrue(TEXT("V does nothing to the shield"), Equipment->GetItemInSlot(EVaesenEquipmentSlot::OffHand) != nullptr);

	// The shield off: both hands again.
	Equipment->UnequipToInventory(EVaesenEquipmentSlot::OffHand, Player->GetInventory());
	TestTrue(TEXT("Shield off: both hands again"), Sword->IsHeldTwoHanded());

	// V: one hand, and back.
	Equipment->RequestToggleGrip();
	TestEqual(TEXT("V: the switch is back where it started (or now on)"), Equipment->HasSwitchedGrip(), bNatural);
	TestFalse(TEXT("V: held in one hand"), Sword->IsHeldTwoHanded());
	Equipment->RequestToggleGrip();
	TestTrue(TEXT("V again: both hands"), Sword->IsHeldTwoHanded());

	// Taken off: in one hand next time (the grip belongs to the body holding it).
	Equipment->UnequipToInventory(EVaesenEquipmentSlot::MainHand, Player->GetInventory());
	TestFalse(TEXT("Unequipped: not held in both hands"), Sword->IsHeldTwoHanded());
	return true;
}
