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
#include "Abilities/VaesenMeleeAbilities.h"
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

		FTestWorld()
		{
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

#endif
