#include "Combat/VaesenCombatSelfTest.h"

#include "AIController.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenAbilitySystemComponent.h"
#include "Combat/VaesenAttributeSet.h"
#include "Combat/VaesenCombatLibrary.h"
#include "Combat/VaesenGameplayTags.h"
#include "Combat/VaesenTrainingDummy.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Inventory/VaesenInventoryComponent.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenEquippableItem.h"
#include "Items/VaesenPickup.h"
#include "Interaction/VaesenInteractionComponent.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "TimerManager.h"
#include "Abilities/VaesenGeneralAbilities.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"

TStrongObjectPtr<UVaesenCombatSelfTest> UVaesenCombatSelfTest::Current;

namespace VaesenSelfTest
{
	const TCHAR* Weapons = TEXT("/Game/Vaesen/Blueprints/Items/Weapons/");
	const TCHAR* Ammo = TEXT("/Game/Vaesen/Blueprints/Items/Ammo/");
	const TCHAR* Throwables = TEXT("/Game/Vaesen/Blueprints/Items/Throwables/");

	FString Path(const TCHAR* Folder, const TCHAR* Name)
	{
		return FString(Folder) + Name;
	}

	UClass* LoadBlueprintClass(const FString& PackagePath)
	{
		const FString ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath) + TEXT("_C");
		return LoadObject<UClass>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The server's world (in PIE the clients' worlds live in the same process). */
	UWorld* FindAuthorityWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
			{
				return World;
			}
		}
		return nullptr;
	}

	FAutoConsoleCommand Command(
		TEXT("vaesen.combat.selftest"),
		TEXT("Runs the in-map combat check on the server (best in L_CombatTest): a fighter, dummies and the test weapons; PASS/FAIL on screen and in the log."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UWorld* World = FindAuthorityWorld())
			{
				UVaesenCombatSelfTest::Run(World);
			}
			else
			{
				UE_LOG(LogVaesenCombat, Warning, TEXT("vaesen.combat.selftest: no server world (start Play first)."));
			}
		}));
}

void UVaesenCombatSelfTest::Run(UWorld* InWorld)
{
	if (Current.IsValid())
	{
		UE_LOG(LogVaesenCombat, Warning, TEXT("vaesen.combat.selftest is already running."));
		return;
	}
	Current.Reset(NewObject<UVaesenCombatSelfTest>());
	Current->World = InWorld;
	Current->Start();
}

void UVaesenCombatSelfTest::Say(const FString& Line, const FColor& Color) const
{
	UE_LOG(LogVaesenCombat, Display, TEXT("[selftest] %s"), *Line);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 30.0f, Color, FString::Printf(TEXT("[combat selftest] %s"), *Line));
	}
}

void UVaesenCombatSelfTest::Check(bool bPassed, const FString& What)
{
	(bPassed ? Passed : Failed)++;
	Say(FString::Printf(TEXT("%s  %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *What), bPassed ? FColor::Green : FColor::Red);
}

UClass* UVaesenCombatSelfTest::LoadTestClass(const TCHAR* InPath)
{
	UClass* Class = VaesenSelfTest::LoadBlueprintClass(InPath);
	Check(Class != nullptr, FString::Printf(TEXT("asset %s exists (run the commandlet if not)"), *FPackageName::GetShortName(InPath)));
	return Class;
}

void UVaesenCombatSelfTest::Start()
{
	UWorld* InWorld = World.Get();

	// 15 m to the side of the first player start, on whatever floor is there.
	FVector Start = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(InWorld); It; ++It)
	{
		Start = It->GetActorLocation();
		break;
	}
	Start += FVector(0.0, 1500.0, 0.0);
	FHitResult Ground;
	if (!InWorld->LineTraceSingleByChannel(Ground, Start + FVector(0.0, 0.0, 500.0), Start - FVector(0.0, 0.0, 2000.0), ECC_Visibility))
	{
		Say(TEXT("No floor 15 m beside the player start; run it in L_CombatTest."), FColor::Red);
		Current.Reset();
		return;
	}
	Origin = Ground.ImpactPoint + FVector(0.0, 0.0, 100.0);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UClass* FighterClass = VaesenSelfTest::LoadBlueprintClass(TEXT("/Game/Vaesen/Blueprints/Characters/BP_VaesenPlayerCharacter"));
	Fighter = InWorld->SpawnActor<AVaesenPlayerCharacter>(FighterClass ? FighterClass : AVaesenPlayerCharacter::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	Controller = InWorld->SpawnActor<AAIController>(AAIController::StaticClass(), Origin, FRotator::ZeroRotator, Params);
	UClass* DummyClass = VaesenSelfTest::LoadBlueprintClass(TEXT("/Game/Vaesen/Blueprints/Combat/BP_TrainingDummy"));
	UClass* ArmoredClass = VaesenSelfTest::LoadBlueprintClass(TEXT("/Game/Vaesen/Blueprints/Combat/BP_TrainingDummy_Armored"));
	Target = InWorld->SpawnActor<AVaesenTrainingDummy>(DummyClass ? DummyClass : AVaesenTrainingDummy::StaticClass(), Origin + FVector(400.0, 0.0, -10.0), FRotator(0.0, 180.0, 0.0), Params);
	Armored = InWorld->SpawnActor<AVaesenTrainingDummy>(ArmoredClass ? ArmoredClass : AVaesenTrainingDummy::StaticClass(), Origin + FVector(400.0, 250.0, -10.0), FRotator(0.0, 180.0, 0.0), Params);
	Attacker = InWorld->SpawnActor<AVaesenTrainingDummy>(DummyClass ? DummyClass : AVaesenTrainingDummy::StaticClass(), Origin + FVector(150.0, -300.0, -10.0), FRotator(0.0, 180.0, 0.0), Params);
	Spawned = { Fighter, Controller, Target, Armored, Attacker };
	if (!Fighter || !Controller || !Target || !Armored || !Attacker)
	{
		Say(TEXT("Couldn't spawn the fighter and dummies."), FColor::Red);
		Finish();
		return;
	}
	// Aim where the script says, not where the body faces.
	Controller->bSetControlRotationFromPawnOrientation = false;
	Controller->Possess(Fighter);
	Controller->SetControlRotation(FRotator::ZeroRotator);
	Say(FString::Printf(TEXT("Starting at %s"), *Origin.ToCompactString()), FColor::White);

	BuildSteps();
	NextStep = 0;
	InWorld->GetTimerManager().SetTimer(StepTimer, this, &UVaesenCombatSelfTest::RunNextStep, 1.0f, false);
}

void UVaesenCombatSelfTest::RunNextStep()
{
	if (!Fighter || !World.IsValid())
	{
		Finish();
		return;
	}
	if (!Steps.IsValidIndex(NextStep))
	{
		Finish();
		return;
	}
	FStep& Step = Steps[NextStep++];
	if (!Step.Name.IsEmpty())
	{
		Say(Step.Name, FColor::Cyan);
	}
	const float Delay = Step.Action ? Step.Action() : 0.0f;
	World->GetTimerManager().SetTimer(StepTimer, this, &UVaesenCombatSelfTest::RunNextStep, FMath::Max(Delay, 0.01f), false);
}

void UVaesenCombatSelfTest::Finish()
{
	if (UWorld* InWorld = World.Get())
	{
		InWorld->GetTimerManager().ClearTimer(StepTimer);
	}
	for (AActor* Actor : Spawned)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	Say(FString::Printf(TEXT("Done: %d passed, %d failed."), Passed, Failed), Failed ? FColor::Red : FColor::Green);
	Current.Reset();
}

bool UVaesenCombatSelfTest::GiveAndEquip(const TCHAR* InPath)
{
	UClass* Class = LoadTestClass(InPath);
	if (!Class)
	{
		return false;
	}
	UVaesenInventoryComponent* Inventory = Fighter->GetInventory();
	Inventory->TryAddItemOfClass(Class, 1);
	UVaesenItem* Item = Inventory->FindItemOfClass(Class);
	return Item && (!Item->IsA<UVaesenEquippableItem>() || Fighter->GetEquipment()->EquipFromInventory(Inventory, Item));
}

void UVaesenCombatSelfTest::Press(uint8 Input) const
{
	Fighter->GetVaesenAbilitySystem()->PressAbilityInput(static_cast<EVaesenAbilityInput>(Input));
}

void UVaesenCombatSelfTest::Release(uint8 Input) const
{
	Fighter->GetVaesenAbilitySystem()->ReleaseAbilityInput(static_cast<EVaesenAbilityInput>(Input));
}

float UVaesenCombatSelfTest::DummyDamage(const AVaesenTrainingDummy* Dummy) const
{
	const UAbilitySystemComponent* ASC = Dummy->GetAbilitySystemComponent();
	return ASC->GetNumericAttribute(UVaesenAttributeSet::GetMaxHealthAttribute()) - ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
}

void UVaesenCombatSelfTest::ResetAll()
{
	for (AVaesenTrainingDummy* Dummy : { Target.Get(), Armored.Get(), Attacker.Get() })
	{
		Dummy->GetVaesenAbilitySystem()->ResetVitals();
	}
	Fighter->GetVaesenAbilitySystem()->ResetVitals();
	Controller->SetControlRotation(FRotator::ZeroRotator);
}

void UVaesenCombatSelfTest::BuildSteps()
{
	using namespace VaesenSelfTest;
	const uint8 Attack = static_cast<uint8>(EVaesenAbilityInput::Attack);
	const uint8 Heavy = static_cast<uint8>(EVaesenAbilityInput::HeavyAttack);
	const uint8 AimOrBlock = static_cast<uint8>(EVaesenAbilityInput::AimOrBlock);
	const uint8 Reload = static_cast<uint8>(EVaesenAbilityInput::Reload);
	const uint8 Throw = static_cast<uint8>(EVaesenAbilityInput::Throw);
	const uint8 Holster = static_cast<uint8>(EVaesenAbilityInput::Holster);
	UVaesenEquipmentComponent* Equipment = Fighter->GetEquipment();
	UVaesenInventoryComponent* Inventory = Fighter->GetInventory();
	UVaesenAbilitySystemComponent* ASC = Fighter->GetVaesenAbilitySystem();
	const auto Tap = [this](uint8 Input) { Press(Input); Release(Input); };
	const auto AmmoDamage = [](const TCHAR* Name)
	{
		const UClass* Class = LoadBlueprintClass(Path(Ammo, Name));
		return Class ? Class->GetDefaultObject<UVaesenAmmoItem>()->GetDamage() : FVaesenDamagePacket();
	};

	// --- Pickups: the interaction trace finds them, and picking one up puts it in the inventory ---
	Steps.Add({ TEXT("Pickups"), [this]()
	{
		int32 Total = 0;
		TArray<FString> Missed;
		TArray<AActor*> Pickups;
		for (TActorIterator<AVaesenPickup> It(World.Get()); It; ++It)
		{
			Pickups.Add(*It);
		}
		for (TActorIterator<AVaesenPickup> It(World.Get()); It; ++It)
		{
			// The same sweep as the interaction trace (10 cm sphere, Visibility), from above onto it.
			FHitResult Hit;
			const FVector Location = It->GetActorLocation();
			// Only this pickup: neighbours lying close by don't count against it.
			FCollisionQueryParams Params(SCENE_QUERY_STAT(VaesenSelfTestPickup), false);
			for (AActor* Other : Pickups)
			{
				if (Other != *It)
				{
					Params.AddIgnoredActor(Other);
				}
			}
			const bool bFound = World->SweepSingleByChannel(Hit, Location + FVector(0.0, 0.0, 80.0), Location - FVector(0.0, 0.0, 20.0),
				FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(10.0f), Params) && Hit.GetActor() == *It;
			Total++;
			if (!bFound)
			{
				Missed.Add(It->GetItem() ? It->GetItem()->GetClass()->GetName() : It->GetName());
			}
		}
		Check(Missed.Num() == 0, FString::Printf(TEXT("the interaction trace finds all %d pickups in the map%s"), Total,
			Missed.Num() ? *FString::Printf(TEXT(" (missed: %s)"), *FString::Join(Missed, TEXT(", "))) : TEXT("")));

		// A rifle pickup of our own beside the fighter, picked up through its interaction.
		UClass* RifleClass = LoadTestClass(*Path(Weapons, TEXT("BP_Weapon_KragRifle")));
		UClass* PickupClass = LoadBlueprintClass(TEXT("/Game/Vaesen/Blueprints/Interaction/BP_Pickup"));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AVaesenPickup* Pickup = World->SpawnActor<AVaesenPickup>(PickupClass ? PickupClass : AVaesenPickup::StaticClass(), Origin + FVector(80.0, 0.0, -95.0), FRotator::ZeroRotator, Params);
		if (Pickup && RifleClass)
		{
			Spawned.Add(Pickup);
			Pickup->InitializePickup(RifleClass, 1);
			UVaesenInteractionComponent* Interaction = Pickup->FindComponentByClass<UVaesenInteractionComponent>();
			Check(Interaction && Interaction->CanInteract(Fighter), TEXT("the fighter may pick up the rifle"));
			if (Interaction)
			{
				// As the E key does: begin (instant interactions complete at once), complete, let go.
				Interaction->BeginInteract(Fighter);
				if (IsValid(Pickup))
				{
					Interaction->Interact(Fighter);
				}
				if (IsValid(Pickup))
				{
					Interaction->EndInteract(Fighter);
				}
			}
			Check(Fighter->GetInventory()->HasItem(RifleClass), TEXT("picked up: the rifle is in the inventory"));
			Fighter->GetInventory()->RemoveItemsOfClass(RifleClass, 1);
		}
		return 0.5f;
	} });

	// --- Revolver: equip, draw, reload from the inventory, fire, armored target ---
	Steps.Add({ TEXT("Revolver"), [this, Tap, Holster]()
	{
		ResetAll();
		Check(GiveAndEquip(*Path(Weapons, TEXT("BP_Weapon_Revolver"))), TEXT("revolver equipped (sidearm)"));
		Tap(Holster);
		return 1.0f;
	} });
	Steps.Add({ FString(), [this, Equipment, Inventory, Tap, Reload]()
	{
		const UVaesenRangedWeaponItem* Revolver = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Revolver != nullptr, TEXT("H drew it"));
		if (UClass* Cartridges = LoadTestClass(*Path(Ammo, TEXT("BP_Ammo_RevolverCartridge"))))
		{
			Inventory->TryAddItemOfClass(Cartridges, 12);
		}
		Tap(Reload);
		return Revolver ? Revolver->ReloadTime * Revolver->MagazineSize + 0.5f : 0.1f;
	} });
	Steps.Add({ FString(), [this, Equipment, Tap, Attack]()
	{
		const UVaesenRangedWeaponItem* Revolver = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Revolver && Revolver->GetLoadedRounds() == Revolver->MagazineSize, TEXT("reloaded round by round to full"));
		Tap(Attack);
		return 0.3f;
	} });
	const FVaesenDamagePacket Cartridge = AmmoDamage(TEXT("BP_Ammo_RevolverCartridge"));
	Steps.Add({ FString(), [this, Cartridge, Tap, Attack]()
	{
		Check(FMath::IsNearlyEqual(DummyDamage(Target), Cartridge.GetTotal()), FString::Printf(TEXT("shot hit the dummy for %.0f (got %.0f)"), Cartridge.GetTotal(), DummyDamage(Target)));
		// Turn to the armored dummy and fire again.
		const FVector ToArmored = Armored->GetActorLocation() - Fighter->GetPawnViewLocation();
		Controller->SetControlRotation(ToArmored.Rotation());
		return 0.5f;
	} });
	Steps.Add({ FString(), [Tap, Attack]()
	{
		// After the fire interval.
		Tap(Attack);
		return 0.5f;
	} });
	Steps.Add({ FString(), [this, Cartridge, Equipment]()
	{
		const float Expected = FMath::Max(0.0f, Cartridge.GetTotal() - Armored->ProtectionPiercing);
		Check(FMath::IsNearlyEqual(DummyDamage(Armored), Expected), FString::Printf(TEXT("armored dummy took %.0f after protection (got %.0f)"), Expected, DummyDamage(Armored)));
		Equipment->RequestSheathe();
		return 1.0f;
	} });

	// --- Krag: magazine reload, projectile, automatic bolt cycle ---
	Steps.Add({ TEXT("Krag-Jørgensen"), [this, Equipment, Inventory, Holster, Tap]()
	{
		ResetAll();
		Check(GiveAndEquip(*Path(Weapons, TEXT("BP_Weapon_KragRifle"))), TEXT("rifle equipped (main hand)"));
		if (UClass* Cartridges = LoadTestClass(*Path(Ammo, TEXT("BP_Ammo_KragCartridge"))))
		{
			Inventory->TryAddItemOfClass(Cartridges, 10);
		}
		Tap(Holster);
		return 1.2f;
	} });
	Steps.Add({ FString(), [this, Equipment, Tap, Reload]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && Rifle->Category == EVaesenWeaponCategory::Rifle, TEXT("H drew the rifle (main hand before sidearm)"));
		Tap(Reload);
		return Rifle ? Rifle->ReloadTime + 0.3f : 0.1f;
	} });
	Steps.Add({ FString(), [this, Equipment, ASC, Tap, Attack, AimOrBlock]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && Rifle->GetLoadedRounds() == Rifle->MagazineSize, TEXT("magazine loaded at once"));
		// Aimed (hip fire spreads 5 degrees with a rifle and can miss at 4 m).
		Press(AimOrBlock);
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming), TEXT("right mouse aims the rifle"));
		Tap(Attack);
		Release(AimOrBlock);
		return 0.2f;
	} });
	Steps.Add({ FString(), [this, ASC]()
	{
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Cycling), TEXT("bolt worked by itself after the shot"));
		return 1.2f;
	} });
	const FVaesenDamagePacket Krag = AmmoDamage(TEXT("BP_Ammo_KragCartridge"));
	Steps.Add({ FString(), [this, Equipment, Krag]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && !Rifle->NeedsCycle(), TEXT("chamber ready again"));
		Check(FMath::IsNearlyEqual(DummyDamage(Target), Krag.GetTotal()), FString::Printf(TEXT("projectile hit for %.0f (got %.0f)"), Krag.GetTotal(), DummyDamage(Target)));
		Equipment->RequestSheathe();
		return 1.0f;
	} });

	// --- Bare hands, then the sword: stamina, block and parry ---
	Steps.Add({ TEXT("Punch"), [this, Tap, Attack]()
	{
		ResetAll();
		Target->SetActorLocation(Origin + FVector(110.0, 0.0, -10.0));
		Tap(Attack);
		return 1.0f;
	} });
	Steps.Add({ FString(), [this]()
	{
		Check(DummyDamage(Target) > 0.0f, FString::Printf(TEXT("punch landed (%.0f)"), DummyDamage(Target)));
		return 0.1f;
	} });
	Steps.Add({ TEXT("Sword"), [this, Tap, Holster]()
	{
		ResetAll();
		Check(GiveAndEquip(*Path(Weapons, TEXT("BP_Weapon_OldSword"))), TEXT("sword equipped (replaces the rifle)"));
		Tap(Holster);
		return 1.2f;
	} });
	Steps.Add({ FString(), [this, Equipment, ASC, Tap, Attack]()
	{
		const UVaesenWeaponItem* Sword = Equipment->GetDrawnWeapon();
		Check(Sword && Sword->Category == EVaesenWeaponCategory::OneHandedSword, TEXT("sword drawn"));
		const float Before = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
		Tap(Attack);
		const float Cost = Sword ? Sword->GetCategoryDefaults().LightAttack.StaminaCost : 0.0f;
		Check(FMath::IsNearlyEqual(Before - ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute()), Cost, 0.5f), FString::Printf(TEXT("light attack cost %.0f stamina"), Cost));
		return 1.2f;
	} });
	Steps.Add({ FString(), [this, Equipment]()
	{
		// The blade only reaches the dummy when an attack animation swings it; without one it's where the hand hangs.
		const UVaesenWeaponItem* Sword = Equipment->GetDrawnWeapon();
		const bool bHasAnimation = Sword && Sword->GetMontage(&FVaesenWeaponMontages::Attack) != nullptr;
		if (bHasAnimation)
		{
			Check(DummyDamage(Target) > 0.0f, FString::Printf(TEXT("swing hit the dummy (%.0f)"), DummyDamage(Target)));
		}
		else
		{
			Say(FString::Printf(TEXT("INFO  swing dealt %.0f (no attack montage yet: the blade stays where the hand hangs)"), DummyDamage(Target)), FColor::Yellow);
		}
		return 0.1f;
	} });
	Steps.Add({ TEXT("Block and parry"), [this, ASC, Tap, AimOrBlock]()
	{
		ResetAll();
		Controller->SetControlRotation((Attacker->GetActorLocation() - Fighter->GetActorLocation()).Rotation());
		return 0.6f;
	} });
	Steps.Add({ FString(), [this, ASC, AimOrBlock]()
	{
		Press(AimOrBlock);
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Blocking), TEXT("right mouse blocks with a sword"));
		FVaesenDamagePacket Swing;
		Swing.Entries.Emplace(VaesenTags::Damage_Slashing, 10.0f);
		const float Before = ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
		UVaesenCombatLibrary::ApplyDamagePacket(Attacker, Fighter, Swing, FHitResult(), nullptr);
		Check(FMath::IsNearlyEqual(ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute()), Before), TEXT("a hit right after pressing block is parried"));
		Check(Attacker->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_Staggered), TEXT("the attacker is staggered"));
		return 0.5f;
	} });
	Steps.Add({ FString(), [this, ASC, Equipment, AimOrBlock]()
	{
		const UVaesenWeaponItem* Sword = Equipment->GetDrawnWeapon();
		const float DurabilityBefore = Sword ? Sword->GetDurability() : 0.0f;
		FVaesenDamagePacket Swing;
		Swing.Entries.Emplace(VaesenTags::Damage_Slashing, 10.0f);
		const float Before = ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
		UVaesenCombatLibrary::ApplyDamagePacket(Attacker, Fighter, Swing, FHitResult(), nullptr);
		const float Taken = Before - ASC->GetNumericAttribute(UVaesenAttributeSet::GetHealthAttribute());
		Check(Taken > 0.0f && Taken < 10.0f, FString::Printf(TEXT("later, a block reduces it (took %.1f of 10)"), Taken));
		Check(Sword && Sword->GetDurability() < DurabilityBefore, TEXT("blocking wore the sword"));
		Release(AimOrBlock);
		Equipment->RequestSheathe();
		return 1.0f;
	} });

	// --- Throwing a stone ---
	Steps.Add({ TEXT("Throw"), [this, Inventory]()
	{
		ResetAll();
		if (UClass* Stone = LoadTestClass(*Path(Throwables, TEXT("BP_Item_Stone"))))
		{
			Inventory->TryAddItemOfClass(Stone, 3);
			Target->SetActorLocation(Origin + FVector(300.0, 0.0, -10.0));
		}
		return 0.3f;
	} });
	Steps.Add({ FString(), [this, Inventory, ASC, Throw]()
	{
		UClass* Stone = LoadBlueprintClass(Path(Throwables, TEXT("BP_Item_Stone")));
		const int32 Before = Stone ? Inventory->GetItemCount(Stone) : 0;
		// Aim a little down, in the same frame as the release (AI controllers level their view every tick).
		Controller->SetControlRotation(FRotator(-10.0, 0.0, 0.0));
		Press(Throw);
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Throwing), TEXT("G aims a throw"));
		Release(Throw);
		Check(Stone && Inventory->GetItemCount(Stone) == Before - 1, TEXT("one stone off the stack"));
		return 1.0f;
	} });
	Steps.Add({ FString(), [this]()
	{
		Check(DummyDamage(Target) > 0.0f, FString::Printf(TEXT("stone hit the dummy (%.0f)"), DummyDamage(Target)));
		return 8.5f;
	} });
	Steps.Add({ FString(), [this]()
	{
		int32 Stones = 0;
		for (TActorIterator<AVaesenPickup> It(World.Get()); It; ++It)
		{
			if (It->GetItem() && FVector::Dist(It->GetActorLocation(), Origin) < 2500.0f && It->GetItem()->CanBeThrown())
			{
				Stones++;
				Spawned.Add(*It);
			}
		}
		Check(Stones > 0, TEXT("the stone became a pickup where it landed"));
		return 0.1f;
	} });

	// --- Bow ---
	Steps.Add({ TEXT("Bow"), [this, Inventory, Tap, Holster]()
	{
		ResetAll();
		Target->SetActorLocation(Origin + FVector(500.0, 0.0, -10.0));
		Check(GiveAndEquip(*Path(Weapons, TEXT("BP_Weapon_HuntingBow"))), TEXT("bow equipped"));
		if (UClass* Arrows = LoadTestClass(*Path(Ammo, TEXT("BP_Ammo_Arrow"))))
		{
			Inventory->TryAddItemOfClass(Arrows, 5);
		}
		Tap(Holster);
		return 1.2f;
	} });
	Steps.Add({ FString(), [this, ASC, Attack]()
	{
		Press(Attack);
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_DrawingBow), TEXT("left mouse nocks and draws"));
		return 1.6f;
	} });
	Steps.Add({ FString(), [this, Attack]()
	{
		Release(Attack);
		return 1.0f;
	} });
	const FVaesenDamagePacket Arrow = AmmoDamage(TEXT("BP_Ammo_Arrow"));
	Steps.Add({ FString(), [this, Arrow, Equipment]()
	{
		Check(DummyDamage(Target) > 0.0f, FString::Printf(TEXT("arrow at full draw hit (%.0f, ammo says %.0f)"), DummyDamage(Target), Arrow.GetTotal()));
		Equipment->RequestSheathe();
		return 1.0f;
	} });
}
