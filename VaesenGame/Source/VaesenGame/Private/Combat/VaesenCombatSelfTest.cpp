#include "Combat/VaesenCombatSelfTest.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraActor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UI/Combat/VaesenCombatHUDWidget.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
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
#include "Items/VaesenMeleeWeaponItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "TimerManager.h"
#include "Abilities/VaesenGeneralAbilities.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/StrongObjectPtr.h"
#include "Weapons/VaesenCasing.h"
#include "Weapons/VaesenRangedWeaponActor.h"
#include "Weapons/VaesenWeaponActor.h"
#include "Weapons/VaesenWeaponData.h"
#include "Anim/VaesenAnimInstance.h"

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

	TAutoConsoleVariable<int32> CVarShots(TEXT("vaesen.combat.selftest.shots"), 0,
		TEXT("1: the self-test also takes side-on screenshots of the sword's poses (Saved/Screenshots/SelfTest)."));

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

bool UVaesenCombatSelfTest::RiflePose(FVector& OutStock, FVector& OutBarrel) const
{
	const UVaesenWeaponItem* Rifle = Fighter && Fighter->GetEquipment() ? Fighter->GetEquipment()->GetDrawnWeapon() : nullptr;
	const AVaesenWeaponActor* Actor = Rifle ? Rifle->GetWeaponActor() : nullptr;
	FTransform Stock, Muzzle;
	if (!Actor || !Actor->GetSocketTransform(TEXT("Stock"), Stock) || !Actor->GetSocketTransform(TEXT("Muzzle"), Muzzle))
	{
		return false;
	}
	OutStock = Stock.GetLocation();
	OutBarrel = (Muzzle.GetLocation() - Stock.GetLocation()).GetSafeNormal();
	return true;
}

int32 UVaesenCombatSelfTest::CountCasings() const
{
	int32 Count = 0;
	for (TActorIterator<AVaesenCasing> It(GetWorld()); It; ++It)
	{
		++Count;
	}
	return Count;
}

void UVaesenCombatSelfTest::CheckLeftPalm(const TCHAR* When)
{
	const UVaesenWeaponItem* Weapon = Fighter && Fighter->GetEquipment() ? Fighter->GetEquipment()->GetDrawnWeapon() : nullptr;
	const AVaesenWeaponActor* Actor = Weapon ? Weapon->GetWeaponActor() : nullptr;
	FTransform LeftGrip;
	if (!Actor || !Actor->GetSocketTransform(TEXT("LeftGrip"), LeftGrip))
	{
		Say(FString::Printf(TEXT("INFO  left hand %s: the weapon has no LeftGrip socket"), When), FColor::Yellow);
		return;
	}
	const FTransform Palm = Fighter->GetMesh()->GetSocketTransform(TEXT("hand_l_weapon"));
	const float Gap = FVector::Dist(Palm.GetLocation(), LeftGrip.GetLocation());
	const float Turn = FMath::RadiansToDegrees(Palm.GetRotation().AngularDistance(LeftGrip.GetRotation()));
	// Which hand bones the animating mesh (the torso) evaluates: the hands mesh copies only those.
	const USkeletalMeshComponent* Torso = Fighter->GetMesh();
	FString Evaluated;
	for (const TCHAR* Bone : { TEXT("hand_l"), TEXT("index_01_l"), TEXT("hand_r"), TEXT("index_01_r") })
	{
		const int32 Index = Torso->GetBoneIndex(Bone);
		const bool bRequired = Index != INDEX_NONE && Torso->RequiredBones.Contains(static_cast<FBoneIndexType>(Index));
		Evaluated += FString::Printf(TEXT(" %s:%s"), Bone, bRequired ? TEXT("yes") : TEXT("NO"));
	}
	Say(FString::Printf(TEXT("INFO  bones the torso animates:%s"), *Evaluated), FColor::Yellow);
	const FString Measured = FString::Printf(TEXT("(%.1f cm, %.0f degrees off)"), Gap, Turn);
	if (GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		Say(FString::Printf(TEXT("INFO  left hand %s not measured on a dedicated server %s"), When, *Measured), FColor::Yellow);
		return;
	}
	Check(Gap < 2.0f && Turn < 10.0f, FString::Printf(TEXT("the left hand holds LeftGrip %s %s"), When, *Measured));
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

	// The ammo rules are part of the test: free fire off while it runs.
	if (IConsoleVariable* FreeFire = IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire")))
	{
		FreeFireBefore = FreeFire->GetInt();
		FreeFire->Set(0, ECVF_SetByCode);
	}

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
	if (IConsoleVariable* FreeFire = IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire")))
	{
		FreeFire->Set(FreeFireBefore, ECVF_SetByCode);
	}
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

void UVaesenCombatSelfTest::Shot(const TCHAR* Name, const TCHAR* PackAnim, float PackTime, bool bFromFront)
{
	UWorld* Here = Fighter ? Fighter->GetWorld() : nullptr;
	APlayerController* Viewer = Here ? Here->GetFirstPlayerController() : nullptr;
	if (VaesenSelfTest::CVarShots.GetValueOnGameThread() <= 0 || !Viewer)
	{
		return;
	}
	if (!ShotCamera)
	{
		ShotCamera = Here->SpawnActor<ACameraActor>();
		Spawned.Add(ShotCamera);
		// The HUD (crosshair, notices) would sit over the fighter.
		for (TObjectIterator<UVaesenCombatHUDWidget> Widget; Widget; ++Widget)
		{
			if (Widget->GetWorld() == Here)
			{
				Widget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	// From the fighter's left, at chest height: him in front, the pack's swordsman behind him. Or close, from ahead
	// and a little to his left, at the hips.
	const FVector Forward = Fighter->GetActorForwardVector();
	const FVector Body = bFromFront ? Fighter->GetActorLocation() - FVector(0.0, 0.0, 15.0)
		: Fighter->GetActorLocation() - Forward * 90.0 + FVector(0.0, 0.0, 20.0);
	const FVector Eye = bFromFront ? Body + Forward * 200.0 - Fighter->GetActorRightVector() * 70.0 + FVector(0.0, 0.0, 10.0)
		: Body - Fighter->GetActorRightVector() * 420.0 + FVector(0.0, 0.0, 20.0);
	ShotCamera->SetActorLocationAndRotation(Eye, (Body - Eye).Rotation());
	Viewer->SetViewTarget(ShotCamera);

	if (!ShotReference)
	{
		if (UClass* BladeMan = LoadObject<UClass>(nullptr, TEXT("/Game/1HM_Sword_Attacks_Finishers/Mannequin/Character/Mesh/1HMBladeMan.1HMBladeMan_C")))
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ShotReference = Here->SpawnActor<AActor>(BladeMan, FTransform::Identity, Parameters);
			Spawned.Add(ShotReference);
		}
	}
	USkeletalMeshComponent* ReferenceMesh = ShotReference ? ShotReference->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimSequence* Pose = PackAnim ? LoadObject<UAnimSequence>(nullptr, PackAnim) : nullptr;
	if (ReferenceMesh)
	{
		ShotReference->SetActorHiddenInGame(!Pose);
		if (Pose)
		{
			// Where the fighter's mesh is, 180 cm behind him (the pack's mannequin faces +Y, as Manny does).
			const FTransform At = Fighter->GetMesh()->GetComponentTransform();
			ShotReference->SetActorLocationAndRotation(At.GetLocation() - Forward * 180.0, At.GetRotation());
			ReferenceMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			ReferenceMesh->PlayAnimation(Pose, false);
			ReferenceMesh->SetPosition(PackTime, false);
			ReferenceMesh->SetPlayRate(0.0f);
			ReferenceMesh->TickAnimation(0.0f, false);
			ReferenceMesh->RefreshBoneTransforms();
			ReferenceMesh->UpdateChildTransforms();

			// Both blades, in each body's own frame (the pack's sword is its mesh's Z, ours the weapon's X).
			const UStaticMeshComponent* PackSword = ShotReference->FindComponentByClass<UStaticMeshComponent>();
			const UVaesenWeaponItem* Weapon = Fighter->GetEquipment()->GetDrawnWeapon();
			const AVaesenWeaponActor* Held = Weapon ? Weapon->GetWeaponActor() : nullptr;
			if (PackSword && Held)
			{
				const FVector Theirs = ReferenceMesh->GetComponentQuat().UnrotateVector(PackSword->GetComponentQuat().GetAxisZ());
				const FVector Ours = Fighter->GetMesh()->GetComponentQuat().UnrotateVector(Held->GetActorQuat().GetAxisX());
				const FVector TheirHand = ReferenceMesh->GetComponentTransform().InverseTransformPosition(ReferenceMesh->GetBoneLocation(TEXT("hand_r")));
				const FVector OurHand = Fighter->GetMesh()->GetComponentTransform().InverseTransformPosition(Fighter->GetMesh()->GetBoneLocation(TEXT("hand_r")));
				Say(FString::Printf(TEXT("INFO  %s: pack blade %s hand %s | ours blade %s hand %s (mesh frame; +Y ahead, -X his right)"), Name,
					*Theirs.ToCompactString(), *TheirHand.ToCompactString(), *Ours.ToCompactString(), *OurHand.ToCompactString()), FColor::Yellow);
			}
		}
	}
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/SelfTest") / (FString(Name) + TEXT(".png")), false, false);
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
	Steps.Add({ FString(), [this, Equipment, Tap, Attack, AimOrBlock]()
	{
		const UVaesenRangedWeaponItem* Revolver = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Revolver && Revolver->GetLoadedRounds() == Revolver->MagazineSize, TEXT("reloaded round by round to full"));
		// Aimed: the hip spread (4 degrees) can miss a dummy 4 m off.
		Press(AimOrBlock);
		Tap(Attack);
		Release(AimOrBlock);
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
	Steps.Add({ FString(), [this, Tap, Attack, AimOrBlock]()
	{
		// After the fire interval.
		Press(AimOrBlock);
		Tap(Attack);
		Release(AimOrBlock);
		return 0.5f;
	} });
	// Aimed, the revolver is held out at arm's length: along the aim, its sight line through the eye.
	Steps.Add({ FString(), [this, AimOrBlock]() { Press(AimOrBlock); return 0.7f; } });
	Steps.Add({ FString(), [this, Equipment, AimOrBlock]()
	{
		Shot(TEXT("12_RevolverAim"), nullptr, 0.0f, true);
		const UVaesenWeaponItem* Revolver = Equipment->GetDrawnWeapon();
		const AVaesenWeaponActor* Actor = Revolver ? Revolver->GetWeaponActor() : nullptr;
		FTransform Muzzle, Sight;
		if (Actor && Actor->GetSocketTransform(TEXT("Muzzle"), Muzzle) && Actor->GetSocketTransform(TEXT("Sight"), Sight))
		{
			FRotator Aim = Fighter->GetBaseAimRotation();
			Aim.Roll = 0.0f;
			// Along the bore (sight to muzzle; the grip is below it).
			const float Alignment = FVector::DotProduct((Muzzle.GetLocation() - Sight.GetLocation()).GetSafeNormal(), Aim.Vector());
			const FVector Eye = UVaesenAnimInstance::EyePoint(Fighter->GetMesh());
			const FVector ToEye = Eye + Aim.RotateVector(Actor->Hold.AimSightOffset) - Sight.GetLocation();
			const FVector Line = Sight.GetRotation().GetForwardVector();
			const float EyeGap = (ToEye - Line * FVector::DotProduct(ToEye, Line)).Size();
			const float Reach = FVector::DotProduct(Sight.GetLocation() - Eye, Aim.Vector());
			const FString Measured = FString::Printf(TEXT("(barrel along the aim %.2f, sight %.0f cm out from the eye, sight line %.1f cm from the eye)"), Alignment, Reach, EyeGap);
			if (GetWorld()->GetNetMode() == NM_DedicatedServer)
			{
				Say(TEXT("INFO  pistol aim not measured on a dedicated server ") + Measured, FColor::Yellow);
			}
			else
			{
				Check(Alignment > 0.97f && EyeGap < 4.0f && Reach > 35.0f, TEXT("aimed, the revolver is held out at arm's length, sights at the eye ") + Measured);
			}
		}
		Release(AimOrBlock);
		return 0.3f;
	} });
	Steps.Add({ FString(), [this, Cartridge, Equipment]()
	{
		const float Expected = FMath::Max(0.0f, Cartridge.GetTotal() - Armored->ProtectionPiercing);
		Check(FMath::IsNearlyEqual(DummyDamage(Armored), Expected), FString::Printf(TEXT("armored dummy took %.0f after protection (got %.0f)"), Expected, DummyDamage(Armored)));
		Equipment->RequestSheathe();
		return 1.0f;
	} });

	// --- Krag: magazine reload, projectile, automatic bolt cycle ---
	Steps.Add({ TEXT("Krag-JÃƒÂ¸rgensen"), [this, Equipment, Inventory, Holster, Tap]()
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
	Steps.Add({ FString(), [this, Equipment, Tap, Attack, Reload]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && Rifle->Category == EVaesenWeaponCategory::Rifle, TEXT("H drew the rifle (main hand before sidearm)"));
		CheckLeftPalm(TEXT("from the hip"));
		// Fired empty first: nothing comes out, and the HUD is told why.
		Tap(Attack);
		Tap(Reload);
		return Rifle ? Rifle->ReloadTime + 0.3f : 0.1f;
	} });
	Steps.Add({ FString(), [this, Equipment, ASC, AimOrBlock]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && Rifle->GetLoadedRounds() == Rifle->MagazineSize, TEXT("magazine loaded at once"));
		double DryTime;
		EVaesenFireBlock Why;
		const AVaesenRangedWeaponActor* RifleActor = Rifle ? Cast<AVaesenRangedWeaponActor>(Rifle->GetWeaponActor()) : nullptr;
		Check(RifleActor && RifleActor->GetLastDryFire(DryTime, Why) && Why == EVaesenFireBlock::Empty, TEXT("fired empty: a dry click, the HUD told it was empty"));
		// Its sounds (from the WeaponVerse1 pack, when it's installed: -run=VaesenSoundSetup).
		if (Rifle && Rifle->SoundSet)
		{
			Check(Rifle->GetSound(EVaesenWeaponSound::Fire) && Rifle->GetSound(EVaesenWeaponSound::Cycle) && Rifle->GetSound(EVaesenWeaponSound::Flyby),
				FString::Printf(TEXT("the Krag has its shot, bolt and bullet-crack sounds (%s)"), *GetNameSafe(Rifle->SoundSet)));
		}
		else
		{
			Say(TEXT("INFO  the Krag has no sound set yet (install WeaponVerse1, run -run=VaesenSoundSetup)"), FColor::Yellow);
		}
		// Aimed (hip fire spreads 5 degrees with a rifle and can miss at 4 m).
		Press(AimOrBlock);
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Aiming), TEXT("right mouse aims the rifle"));
		return 0.8f;
	} });
	Steps.Add({ FString(), [this, Equipment, Tap, Attack, AimOrBlock]()
	{
		// Shouldered: the barrel along the aim, the stock at the shoulder (the anim graph's shoulder aim).
		const UVaesenWeaponItem* Rifle = Equipment->GetDrawnWeapon();
		const AVaesenWeaponActor* Actor = Rifle ? Rifle->GetWeaponActor() : nullptr;
		FTransform Muzzle, Stock;
		if (Actor && Actor->GetSocketTransform(TEXT("Muzzle"), Muzzle) && Actor->GetSocketTransform(TEXT("Stock"), Stock))
		{
			const FVector Along = Fighter->GetBaseAimRotation().Vector();
			const float Alignment = FVector::DotProduct((Muzzle.GetLocation() - Stock.GetLocation()).GetSafeNormal(), Along);
			// The butt in the shoulder pocket (not floating over the shoulder, not down at the chest)...
			const USkeletalMeshComponent* Body = Fighter->GetMesh();
			const float ToShoulder = FVector::Dist(Stock.GetLocation(), UVaesenAnimInstance::ShoulderPocket(Body, Fighter->GetBaseAimRotation()));
			const float BelowHead = Body->GetSocketLocation(TEXT("head")).Z - Stock.GetLocation().Z;
			// ...and the eye looking down the sights: the sight line passes through the eye.
			FTransform Sight;
			Actor->GetSocketTransform(TEXT("Sight"), Sight);
			FRotator Aim = Fighter->GetBaseAimRotation();
			Aim.Roll = 0.0f;
			const FVector ToEye = UVaesenAnimInstance::EyePoint(Body) + Aim.RotateVector(Actor->Hold.AimSightOffset) - Sight.GetLocation();
			const FVector Line = Sight.GetRotation().GetForwardVector();
			const float EyeGap = (ToEye - Line * FVector::DotProduct(ToEye, Line)).Size();
			const FString Measured = FString::Printf(TEXT("(barrel along the aim %.2f, stock %.1f cm from the shoulder pocket, %.0f cm below the head, sight line %.1f cm from the eye)"),
				Alignment, ToShoulder, BelowHead, EyeGap);
			if (GetWorld()->GetNetMode() == NM_DedicatedServer)
			{
				Say(TEXT("INFO  shoulder aim not measured on a dedicated server (the pose is the clients') ") + Measured, FColor::Yellow);
			}
			else
			{
				Check(Alignment > 0.98f && ToShoulder < 2.0f && EyeGap < 4.0f, TEXT("the aimed rifle is shouldered ") + Measured);
			}
		}
		CheckLeftPalm(TEXT("aimed"));
		// Fire and keep aiming: the bolt is worked at the shoulder.
		CasingsBefore = CountCasings();
		RiflePose(RestStock, RestBarrel);
		Tap(Attack);
		return 0.05f;
	} });
	Steps.Add({ FString(), [this]()
	{
		// The kick, just after the shot: the muzzle up, the butt and shoulder driven back.
		FVector Stock, Barrel;
		if (RiflePose(Stock, Barrel) && GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			const float Climb = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Barrel.Z, -1.0, 1.0)) - FMath::Asin(FMath::Clamp(RestBarrel.Z, -1.0, 1.0)));
			const float Back = -FVector::DotProduct(Stock - RestStock, RestBarrel);
			Check(Climb > 3.0f && Back > 2.0f, FString::Printf(TEXT("the shot kicks (muzzle up %.1f degrees, butt and shoulder back %.1f cm)"), Climb, Back));
		}
		return 0.2f;
	} });
	Steps.Add({ FString(), [this, ASC, Equipment]()
	{
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Cycling), TEXT("bolt worked by itself after the shot"));
		// Mid-cycle: still shouldered and the left hand on the rifle; the right hand off the grip, on the bolt.
		const UVaesenWeaponItem* Rifle = Equipment->GetDrawnWeapon();
		const AVaesenWeaponActor* Actor = Rifle ? Rifle->GetWeaponActor() : nullptr;
		FTransform Stock, Bolt;
		if (Actor && Actor->GetSocketTransform(TEXT("Stock"), Stock) && Actor->GetSocketTransform(TEXT("Bolt"), Bolt) && GetWorld()->GetNetMode() != NM_DedicatedServer)
		{
			const USkeletalMeshComponent* Body = Fighter->GetMesh();
			const float ToShoulder = FVector::Dist(Stock.GetLocation(), UVaesenAnimInstance::ShoulderPocket(Body, Fighter->GetBaseAimRotation()));
			const FVector Palm = Body->GetSocketLocation(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket);
			const float OffGrip = FVector::Dist(Palm, Actor->GetActorLocation());
			const float ToBolt = FVector::Dist(Palm, Bolt.GetLocation());
			Check(ToShoulder < 3.0f && OffGrip > 5.0f && ToBolt < 15.0f, FString::Printf(TEXT("working the bolt at the shoulder (stock %.1f cm from the pocket; right hand %.0f cm off the grip, %.0f cm from the bolt)"),
				ToShoulder, OffGrip, ToBolt));
		}
		CheckLeftPalm(TEXT("working the bolt"));
		return 0.45f;
	} });
	Steps.Add({ FString(), [this, AimOrBlock]()
	{
		Check(CountCasings() > CasingsBefore, TEXT("the spent case flew out as the bolt came back"));
		Release(AimOrBlock);
		return 0.8f;
	} });
	const FVaesenDamagePacket Krag = AmmoDamage(TEXT("BP_Ammo_KragCartridge"));
	Steps.Add({ FString(), [this, Equipment, Krag, Tap, Attack]()
	{
		const UVaesenRangedWeaponItem* Rifle = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon());
		Check(Rifle && !Rifle->NeedsCycle(), TEXT("chamber ready again"));
		Check(FMath::IsNearlyEqual(DummyDamage(Target), Krag.GetTotal()), FString::Printf(TEXT("projectile hit for %.0f (got %.0f)"), Krag.GetTotal(), DummyDamage(Target)));
		// vaesen.combat.freefire: an empty rifle still fires (and works its bolt).
		if (UVaesenRangedWeaponItem* Emptied = Cast<UVaesenRangedWeaponItem>(Equipment->GetDrawnWeapon()))
		{
			Emptied->SetLoadedRounds(0, nullptr);
		}
		IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire"))->Set(1, ECVF_SetByCode);
		CasingsBefore = CountCasings();
		Tap(Attack);
		return 0.3f;
	} });
	Steps.Add({ FString(), [this, Equipment, ASC]()
	{
		Check(ASC->HasMatchingGameplayTag(VaesenTags::State_Cycling), TEXT("with free fire on, the empty rifle fired (and works its bolt)"));
		IConsoleManager::Get().FindConsoleVariable(TEXT("vaesen.combat.freefire"))->Set(0, ECVF_SetByCode);
		return 1.0f;
	} });
	Steps.Add({ FString(), [Equipment]()
	{
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
		// The draw from the hip takes its montage's time.
		return 0.62f;
	} });
	Steps.Add({ FString(), [this]()
	{
		// The hand at the hilt in the sheath (the draw takes it about now).
		Shot(TEXT("0_DrawGrab"));
		return 1.2f;
	} });
	Steps.Add({ FString(), [this, Equipment, ASC, Tap, Attack]()
	{
		const UVaesenWeaponItem* Sword = Equipment->GetDrawnWeapon();
		Check(Sword && Sword->Category == EVaesenWeaponCategory::OneHandedSword, TEXT("sword drawn"));
		// The off hand is free, but the sword has only one-handed animations: one hand (V would take both). Its swing
		// costs what that grip's does.
		Check(Sword && Sword->IsHeldTwoHanded() == Sword->PrefersBothHands(), TEXT("the sword is held in its natural grip (one hand: no two-handed animations yet)"));
		const float Before = ASC->GetNumericAttribute(UVaesenAttributeSet::GetStaminaAttribute());
		Tap(Attack);
		FVaesenMeleeAttack Light;
		const UVaesenMeleeWeaponItem* Melee = Cast<UVaesenMeleeWeaponItem>(Sword);
		const float Cost = Melee && Melee->GetAttack(false, Light) ? Light.StaminaCost : 0.0f;
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
	// --- The sword's moves: a combo, a stab, the guard, a dodge (photographed with vaesen.combat.selftest.shots 1) ---
	const uint8 Dodge = static_cast<uint8>(EVaesenAbilityInput::Dodge);
	const TSharedRef<FVector> DodgeFrom = MakeShared<FVector>();
	const TSharedRef<FTransform> StandAt = MakeShared<FTransform>();
	const auto PlayingMontage = [this]()
	{
		const UAnimInstance* Anim = Fighter->GetMesh()->GetAnimInstance();
		const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
		return Montage ? Montage->GetName() : FString(TEXT("none"));
	};
	Steps.Add({ TEXT("Sword moves"), [this, StandAt]()
	{
		ResetAll();
		*StandAt = Fighter->GetActorTransform();
		return 0.6f;
	} });
	Steps.Add({ FString(), [this]()
	{
		Shot(TEXT("1_SwordIdle"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Idle/1HM_Idle_Standing.1HM_Idle_Standing"), 0.0f);
		const UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Fighter->GetMesh()->GetAnimInstance());
		Check(Anim && Anim->IsRelaxed(), TEXT("standing with a sword drawn: the plain idle (not the fighting stance)"));
		return 0.3f;
	} });
	Steps.Add({ FString(), [Tap, Attack]() { Tap(Attack); return 0.32f; } });
	Steps.Add({ FString(), [this, PlayingMontage]()
	{
		Shot(TEXT("2_Slash01"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Slash/1HM_Slash01.1HM_Slash01"), 0.32f);
		Check(PlayingMontage().Contains(TEXT("Slash01")), FString::Printf(TEXT("the first swing is the first slash (%s)"), *PlayingMontage()));
		Check(Fighter->HasAnyRootMotion(), TEXT("the swing moves the body by its animation (root motion), not by the input"));
		return 0.45f;
	} });
	Steps.Add({ FString(), [Tap, Attack]() { Tap(Attack); return 0.3f; } });
	Steps.Add({ FString(), [this, PlayingMontage]()
	{
		Shot(TEXT("3_Slash02"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Slash/1HM_Slash02.1HM_Slash02"), 0.3f);
		Check(PlayingMontage().Contains(TEXT("Slash02")), FString::Printf(TEXT("attacking again soon after swings the next slash of the combo (%s)"), *PlayingMontage()));
		return 1.4f;
	} });
	Steps.Add({ FString(), [Tap, Heavy]() { Tap(Heavy); return 0.3f; } });
	Steps.Add({ FString(), [this, PlayingMontage]()
	{
		Shot(TEXT("4_Stab01"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Stab/1HM_Stab01.1HM_Stab01"), 0.3f);
		Check(PlayingMontage().Contains(TEXT("Stab01")), FString::Printf(TEXT("the heavy attack stabs (%s)"), *PlayingMontage()));
		return 1.4f;
	} });
	Steps.Add({ FString(), [this, AimOrBlock]() { Press(AimOrBlock); return 0.6f; } });
	Steps.Add({ FString(), [this, AimOrBlock]()
	{
		Shot(TEXT("5_Guard"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Block/1HM_BlockRight.1HM_BlockRight"), 0.54f);
		const UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Fighter->GetMesh()->GetAnimInstance());
		Check(Anim && !Anim->IsRelaxed(), TEXT("guarding squares up: the fighting stance"));
		Release(AimOrBlock);
		return 0.5f;
	} });
	Steps.Add({ FString(), [this, Tap, Dodge, DodgeFrom]()
	{
		*DodgeFrom = Fighter->GetActorLocation();
		Tap(Dodge);
		Check(Fighter->GetVaesenAbilitySystem()->HasMatchingGameplayTag(VaesenTags::State_Dodging), TEXT("Left Alt dodges with a sword"));
		return 0.35f;
	} });
	Steps.Add({ FString(), [this]() { Shot(TEXT("6_DodgeBack"), TEXT("/Game/1HM_Sword_Attacks_Finishers/Animations/1HM_Evade/1HM_Evade_Back.1HM_Evade_Back"), 0.35f); return 1.0f; } });
	Steps.Add({ FString(), [this, DodgeFrom, StandAt]()
	{
		const float Moved = FVector::Dist2D(*DodgeFrom, Fighter->GetActorLocation());
		Check(Moved > 50.0f, FString::Printf(TEXT("the dodge carries the fighter (%.0f cm)"), Moved));
		Say(FString::Printf(TEXT("INFO  the swings carried the fighter %.0f cm (root motion)"), FVector::Dist2D(StandAt->GetLocation(), *DodgeFrom)), FColor::Yellow);
		// Back where he stood, facing as he did.
		Fighter->SetActorTransform(*StandAt);
		if (APlayerController* Viewer = ShotCamera ? Fighter->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Viewer->SetViewTarget(Viewer->GetPawn());
		}
		return 0.3f;
	} });

	// Running with the sword drawn (the plain run, the blade turned clear): steered away from the dummies for a moment.
	const TSharedRef<FTimerHandle> RunTimer = MakeShared<FTimerHandle>();
	Steps.Add({ FString(), [this, RunTimer, StandAt]()
	{
		*StandAt = Fighter->GetActorTransform();
		const FVector Away = -Fighter->GetActorForwardVector();
		Fighter->GetWorldTimerManager().SetTimer(*RunTimer, FTimerDelegate::CreateWeakLambda(Fighter.Get(), [Fighter = Fighter.Get(), Away]()
		{
			Fighter->AddMovementInput(Away);
		}), 0.01f, true);
		return 0.9f;
	} });
	Steps.Add({ FString(), [this]()
	{
		const UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Fighter->GetMesh()->GetAnimInstance());
		Check(Anim && Anim->IsRelaxed(), FString::Printf(TEXT("running with the sword uses the plain run (%.0f cm/s)"), Fighter->GetVelocity().Size2D()));
		Shot(TEXT("8_Running"));
		return 0.25f;
	} });
	Steps.Add({ FString(), [this]() { Shot(TEXT("9_RunningFront"), nullptr, 0.0f, true); return 0.3f; } });
	Steps.Add({ FString(), [this, RunTimer, StandAt]()
	{
		Fighter->GetWorldTimerManager().ClearTimer(*RunTimer);
		Fighter->GetCharacterMovement()->StopMovementImmediately();
		Fighter->SetActorTransform(*StandAt);
		if (APlayerController* Viewer = ShotCamera ? Fighter->GetWorld()->GetFirstPlayerController() : nullptr)
		{
			Viewer->SetViewTarget(Viewer->GetPawn());
		}
		return 0.5f;
	} });

	// --- Target lock: onto the dummy ahead, squared up in the sword's fighting stance, and let go ---
	Steps.Add({ TEXT("Target lock"), [this]()
	{
		ResetAll();
		Fighter->ToggleTargetLock();
		Check(Fighter->GetLockTarget() != nullptr, FString::Printf(TEXT("locks onto the fighter ahead (%s)"), *GetNameSafe(Fighter->GetLockTarget())));
		Check(Fighter->WantsToFaceAim(), TEXT("locked: faces the target (strafes)"));
		return 0.5f;
	} });
	Steps.Add({ FString(), [this]()
	{
		const UVaesenAnimInstance* Anim = Cast<UVaesenAnimInstance>(Fighter->GetMesh()->GetAnimInstance());
		Check(Anim && !Anim->IsRelaxed(), TEXT("locked: the sword's fighting stance"));
		Fighter->ToggleTargetLock();
		Check(!Fighter->IsTargetLocked() && !Fighter->WantsToFaceAim(), TEXT("pressed again: lets go"));
		return 0.3f;
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
		return 2.0f;
	} });

	Steps.Add({ FString(), [this]()
	{
		Shot(TEXT("7_Sheathed"));
		// At the hip it hangs from the hilt, the blade down and back (not level, not straight down).
		const UVaesenWeaponItem* Sword = Fighter->GetEquipment()->GetWeaponInSlot(EVaesenEquipmentSlot::MainHand);
		const AVaesenWeaponActor* Sheathed = Sword ? Sword->GetWeaponActor() : nullptr;
		const float Below = Sheathed ? FMath::RadiansToDegrees(FMath::Asin(-Sheathed->GetActorForwardVector().Z)) : 0.0f;
		Check(Sheathed && !Sheathed->IsInHand() && Below > 35.0f && Below < 85.0f, FString::Printf(TEXT("the sheathed sword hangs at the hip (%.0f degrees below level; %s hangs %.0f; ticking %d)"), Below,
			Sword ? *Sword->SheathSocket.ToString() : TEXT("-"), Sword && UVaesenWeaponCategoryDefaults::Get()->FindSheathLocation(Sword->SheathSocket) ? UVaesenWeaponCategoryDefaults::Get()->FindSheathLocation(Sword->SheathSocket)->HangAngle : -1.0f,
			Sheathed && Sheathed->IsActorTickEnabled() ? 1 : 0));
		return 0.3f;
	} });

	Steps.Add({ FString(), [this]() { Shot(TEXT("7b_SheathedFront"), nullptr, 0.0f, true); return 0.3f; } });

	// Running with the sword sheathed (it rests against the leg as the legs come in under the body).
	Steps.Add({ FString(), [this, RunTimer, StandAt]()
	{
		*StandAt = Fighter->GetActorTransform();
		const FVector Away = -Fighter->GetActorForwardVector();
		Fighter->GetWorldTimerManager().SetTimer(*RunTimer, FTimerDelegate::CreateWeakLambda(Fighter.Get(), [Fighter = Fighter.Get(), Away]()
		{
			Fighter->AddMovementInput(Away);
		}), 0.01f, true);
		return 0.9f;
	} });
	Steps.Add({ FString(), [this]() { Shot(TEXT("10_RunningSheathedFront"), nullptr, 0.0f, true); return 0.2f; } });
	Steps.Add({ FString(), [this]() { Shot(TEXT("11_RunningSheathedFront"), nullptr, 0.0f, true); return 0.2f; } });
	Steps.Add({ FString(), [this, RunTimer, StandAt]()
	{
		Fighter->GetWorldTimerManager().ClearTimer(*RunTimer);
		Fighter->GetCharacterMovement()->StopMovementImmediately();
		Fighter->SetActorTransform(*StandAt);
		return 0.3f;
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
