#include "VaesenCombatAssets.h"

#include "Abilities/VaesenGeneralAbilities.h"
#include "Abilities/VaesenMeleeAbilities.h"
#include "Abilities/VaesenRangedAbilities.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/VaesenBaseCharacter.h"
#include "Characters/VaesenPlayerCharacter.h"
#include "Combat/VaesenGameplayEffects.h"
#include "Combat/VaesenGameplayTags.h"
#include "Combat/VaesenTrainingDummy.h"
#include "Combat/VaesenTrainingDummyAttacker.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/VaesenSettings.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DirectionalLight.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Anim/VaesenWeaponFitRig.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Items/VaesenAmmoItem.h"
#include "Items/VaesenItem.h"
#include "Items/VaesenMeleeWeaponItem.h"
#include "Items/VaesenPickup.h"
#include "Items/VaesenPouchItem.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UDynamicMesh.h"
#include "UObject/SavePackage.h"
#include "Weapons/VaesenWeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenCombatAssets, Log, All);

#define LOCTEXT_NAMESPACE "VaesenCombatAssets"

namespace VaesenCombatAssets
{
	const TCHAR* CombatFolder = TEXT("/Game/Vaesen/Combat");
	const TCHAR* GreyBoxFolder = TEXT("/Game/Vaesen/Combat/GreyBox");
	const TCHAR* WeaponsFolder = TEXT("/Game/Vaesen/Blueprints/Items/Weapons");
	const TCHAR* AmmoFolder = TEXT("/Game/Vaesen/Blueprints/Items/Ammo");
	const TCHAR* ThrowablesFolder = TEXT("/Game/Vaesen/Blueprints/Items/Throwables");
	const TCHAR* PlayerCharacterPath = TEXT("/Game/Vaesen/Blueprints/Characters/BP_VaesenPlayerCharacter");
	const TCHAR* PickupPath = TEXT("/Game/Vaesen/Blueprints/Interaction/BP_Pickup");
	const TCHAR* MapPath = TEXT("/Game/Vaesen/Maps/L_CombatTest");

	// --- Helpers ---

	FString Join(const TCHAR* Folder, const TCHAR* Name)
	{
		return FString(Folder) / Name;
	}

	FString ObjectPathOf(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	}

	bool Exists(const FString& PackagePath)
	{
		return FPackageName::DoesPackageExist(PackagePath);
	}

	bool Save(UObject* Asset, bool bMap = false)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(),
			bMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, Args);
		UE_LOG(LogVaesenCombatAssets, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *FileName);
		return bSaved;
	}

	/** A new asset object in its own package, or null when it already exists. */
	template <typename T>
	T* CreateAsset(const FString& PackagePath, UClass* Class = T::StaticClass())
	{
		if (Exists(PackagePath))
		{
			return nullptr;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		T* Asset = NewObject<T>(Package, Class, FName(FPackageName::GetShortName(PackagePath)), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Asset);
		return Asset;
	}

	/** A data-only Blueprint of Parent, or null when it already exists. */
	UBlueprint* CreateBlueprint(const FString& PackagePath, UClass* Parent)
	{
		if (Exists(PackagePath))
		{
			UE_LOG(LogVaesenCombatAssets, Display, TEXT("%s already exists; leaving it alone."), *PackagePath);
			return nullptr;
		}
		UPackage* Package = CreatePackage(*PackagePath);
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, FName(FPackageName::GetShortName(PackagePath)),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}

	template <typename T>
	T* DefaultsOf(UBlueprint* Blueprint)
	{
		return CastChecked<T>(Blueprint->GeneratedClass->GetDefaultObject());
	}

	void Finish(UBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Save(Blueprint);
	}

	UClass* LoadBlueprintClass(const FString& PackagePath)
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPathOf(PackagePath), nullptr, LOAD_NoWarn | LOAD_Quiet);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	template <typename T>
	T* LoadAsset(const FString& PackagePath)
	{
		return LoadObject<T>(nullptr, *ObjectPathOf(PackagePath), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Sets a property by name (protected ones too): objects and classes, ints. */
	void SetObjectProperty(UObject* Target, const TCHAR* Name, UObject* Value)
	{
		if (FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Target->GetClass(), Name))
		{
			Property->SetObjectPropertyValue_InContainer(Target, Value);
		}
		else
		{
			UE_LOG(LogVaesenCombatAssets, Warning, TEXT("%s has no property %s."), *Target->GetClass()->GetName(), Name);
		}
	}

	void SetIntProperty(UObject* Target, const TCHAR* Name, int32 Value)
	{
		if (FIntProperty* Property = FindFProperty<FIntProperty>(Target->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Target, Value);
		}
	}

	FVaesenDamagePacket Damage(const FGameplayTag& Type, float Amount)
	{
		FVaesenDamagePacket Packet;
		Packet.Entries.Emplace(Type, Amount);
		return Packet;
	}

	// --- Grey-box meshes ---

	/**
	 * A box around the mesh as its simple collision, if it has none: the interaction trace, pickups lying on the floor and
	 * dropped items falling with physics all need one. Returns whether it was added.
	 */
	bool AddBoxCollision(UStaticMesh* Mesh)
	{
		if (!Mesh->GetBodySetup())
		{
			Mesh->CreateBodySetup();
		}
		UBodySetup* Body = Mesh->GetBodySetup();
		if (!Body || Body->AggGeom.GetElementCount() > 0)
		{
			return false;
		}
		const FBox Bounds = Mesh->GetBoundingBox();
		// At least 2 cm thick, so thin things (a blade, an arrow) can still be found and come to rest.
		const FVector Size = Bounds.GetSize().ComponentMax(FVector(2.0));
		FKBoxElem Box(Size.X, Size.Y, Size.Z);
		Box.Center = Bounds.GetCenter();
		Body->AggGeom.BoxElems.Add(Box);
		Body->CollisionTraceFlag = CTF_UseDefault;
		Body->InvalidatePhysicsData();
		Body->CreatePhysicsMeshes();
		Mesh->MarkPackageDirty();
		return true;
	}

	/** Builds a static mesh from boxes and cylinders, with sockets (all in cm, the weapon pointing along +X). */
	struct FGreyBox
	{
		UDynamicMesh* Mesh = NewObject<UDynamicMesh>();
		TArray<TPair<FName, FTransform>> Sockets;
		FGeometryScriptPrimitiveOptions Options;

		FGreyBox& Box(const FVector& Centre, const FVector& Size, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(Mesh, Options, FTransform(Rotation, Centre), Size.X, Size.Y, Size.Z, 0, 0, 0, EGeometryScriptPrimitiveOriginMode::Center);
			return *this;
		}

		/** A cylinder from Start along +X (or the given rotation) for Length. */
		FGreyBox& Rod(const FVector& Start, float Radius, float Length, const FRotator& Rotation = FRotator(-90.0, 0.0, 0.0))
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(Mesh, Options, FTransform(Rotation, Start), Radius, Length, 12, 0, true, EGeometryScriptPrimitiveOriginMode::Base);
			return *this;
		}

		FGreyBox& Ball(const FVector& Centre, float Radius)
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(Mesh, Options, FTransform(Centre), Radius, 4, 4, 4, EGeometryScriptPrimitiveOriginMode::Center);
			return *this;
		}

		FGreyBox& Cone(const FVector& Start, float BaseRadius, float TopRadius, float Length, const FRotator& Rotation = FRotator(-90.0, 0.0, 0.0))
		{
			UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCone(Mesh, Options, FTransform(Rotation, Start), BaseRadius, TopRadius, Length, 12, 1, true, EGeometryScriptPrimitiveOriginMode::Base);
			return *this;
		}

		FGreyBox& Socket(const TCHAR* Name, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			Sockets.Emplace(Name, FTransform(Rotation, Location));
			return *this;
		}

		UStaticMesh* Make(const TCHAR* Name)
		{
			const FString PackagePath = Join(GreyBoxFolder, Name);
			if (UStaticMesh* Existing = LoadAsset<UStaticMesh>(PackagePath))
			{
				// Meshes made before they got collision: add it now.
				if (AddBoxCollision(Existing))
				{
					Save(Existing);
				}
				return Existing;
			}
			FGeometryScriptCreateNewStaticMeshAssetOptions AssetOptions;
			AssetOptions.bEnableRecomputeNormals = true;
			AssetOptions.bEnableCollision = true;
			EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
			UStaticMesh* Asset = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewStaticMeshAssetFromMesh(Mesh, PackagePath, AssetOptions, Outcome);
			if (!Asset || Outcome != EGeometryScriptOutcomePins::Success)
			{
				UE_LOG(LogVaesenCombatAssets, Warning, TEXT("Couldn't build the grey-box mesh %s."), Name);
				return nullptr;
			}
			for (const TPair<FName, FTransform>& Entry : Sockets)
			{
				UStaticMeshSocket* MeshSocket = NewObject<UStaticMeshSocket>(Asset);
				MeshSocket->SocketName = Entry.Key;
				MeshSocket->RelativeLocation = Entry.Value.GetLocation();
				MeshSocket->RelativeRotation = Entry.Value.Rotator();
				Asset->AddSocket(MeshSocket);
			}
			AddBoxCollision(Asset);
			Save(Asset);
			return Asset;
		}
	};

	struct FMeshes
	{
		UStaticMesh* Revolver = nullptr;
		UStaticMesh* Pistol = nullptr;
		UStaticMesh* Rifle = nullptr;
		UStaticMesh* MuzzleLoader = nullptr;
		UStaticMesh* Bow = nullptr;
		UStaticMesh* Crossbow = nullptr;
		UStaticMesh* Sword = nullptr;
		UStaticMesh* Dagger = nullptr;
		UStaticMesh* Axe = nullptr;
		UStaticMesh* Club = nullptr;
		UStaticMesh* Warhammer = nullptr;
		UStaticMesh* ChairLeg = nullptr;
		UStaticMesh* Buckler = nullptr;
		UStaticMesh* Stone = nullptr;
		UStaticMesh* Bottle = nullptr;
		UStaticMesh* Cartridge = nullptr;
		UStaticMesh* Arrow = nullptr;
		UStaticMesh* Box = nullptr;
	};

	/**
	 * Grey-box meshes: the weapon points along +X, up is +Z, and Grip (where the hand closes) is near the origin. Long guns have Sight (the line of the sights) and Stock (the butt plate) for the shoulder aim.
	 * Replace them with the asset pack's meshes later (same socket names).
	 */
	FMeshes BuildGreyBoxMeshes()
	{
		FMeshes Meshes;
		// Guns: Grip in the grip, Muzzle at the end of the barrel, Eject at the breech's right side.
		Meshes.Revolver = FGreyBox()
			.Box(FVector(-1.0, 0.0, -5.0), FVector(3.0, 2.5, 10.0), FRotator(-15.0, 0.0, 0.0))
			.Box(FVector(3.0, 0.0, 2.0), FVector(8.0, 3.0, 4.0))
			.Rod(FVector(4.0, 0.0, 2.0), 2.0f, 4.0f)
			.Rod(FVector(7.0, 0.0, 3.0), 0.8f, 14.0f)
			.Socket(TEXT("Grip"), FVector(-1.0, 0.0, -4.0))
			.Socket(TEXT("Muzzle"), FVector(21.0, 0.0, 3.0))
			.Socket(TEXT("Eject"), FVector(6.0, 2.0, 3.0))
			.Make(TEXT("SM_GB_Revolver"));
		Meshes.Pistol = FGreyBox()
			.Box(FVector(-1.0, 0.0, -5.0), FVector(3.0, 2.5, 10.0), FRotator(-20.0, 0.0, 0.0))
			.Box(FVector(4.0, 0.0, 2.0), FVector(8.0, 2.5, 3.5))
			.Rod(FVector(7.0, 0.0, 2.5), 0.9f, 18.0f)
			.Socket(TEXT("Grip"), FVector(-1.0, 0.0, -4.0))
			.Socket(TEXT("Muzzle"), FVector(25.0, 0.0, 2.5))
			.Socket(TEXT("Eject"), FVector(4.0, 2.0, 3.0))
			.Make(TEXT("SM_GB_Pistol"));
		Meshes.Rifle = FGreyBox()
			.Box(FVector(-22.0, 0.0, -4.0), FVector(36.0, 4.0, 10.0), FRotator(-6.0, 0.0, 0.0))
			.Box(FVector(8.0, 0.0, 0.0), FVector(24.0, 4.0, 6.0))
			.Box(FVector(36.0, 0.0, -1.0), FVector(32.0, 3.5, 4.0))
			.Rod(FVector(18.0, 0.0, 2.0), 1.0f, 70.0f)
			.Box(FVector(10.0, 3.0, 4.0), FVector(2.0, 4.0, 1.0))
			.Socket(TEXT("Grip"), FVector(0.0, 0.0, -2.0))
			.Socket(TEXT("LeftGrip"), FVector(28.0, -1.0, -5.0))
			.Socket(TEXT("Muzzle"), FVector(88.0, 0.0, 2.0))
			.Socket(TEXT("Sight"), FVector(14.0, 0.0, 4.5))
			.Socket(TEXT("Stock"), FVector(-40.0, 0.0, -2.0))
			.Socket(TEXT("Eject"), FVector(10.0, 2.5, 3.0))
			.Socket(TEXT("Bolt"), FVector(10.0, 5.0, 4.0))
			.Socket(TEXT("TraceStart"), FVector(-20.0, 0.0, -4.0))
			.Socket(TEXT("TraceEnd"), FVector(-40.0, 0.0, -6.0))
			.Make(TEXT("SM_GB_KragRifle"));
		Meshes.MuzzleLoader = FGreyBox()
			.Box(FVector(-22.0, 0.0, -4.0), FVector(36.0, 4.0, 10.0), FRotator(-6.0, 0.0, 0.0))
			.Box(FVector(40.0, 0.0, -1.0), FVector(70.0, 3.5, 4.0))
			.Rod(FVector(5.0, 0.0, 2.0), 1.3f, 95.0f)
			.Rod(FVector(10.0, 0.0, -1.0), 0.3f, 80.0f)
			.Socket(TEXT("Grip"), FVector(0.0, 0.0, -2.0))
			.Socket(TEXT("LeftGrip"), FVector(30.0, -1.0, -5.0))
			.Socket(TEXT("Muzzle"), FVector(100.0, 0.0, 2.0))
			.Socket(TEXT("Sight"), FVector(8.0, 0.0, 4.5))
			.Socket(TEXT("Stock"), FVector(-40.0, 0.0, -2.0))
			.Socket(TEXT("TraceStart"), FVector(-20.0, 0.0, -4.0))
			.Socket(TEXT("TraceEnd"), FVector(-40.0, 0.0, -6.0))
			.Make(TEXT("SM_GB_MuzzleLoader"));
		// A bow stands up and down; the arrow sits on the rest at the grip.
		Meshes.Bow = FGreyBox()
			.Rod(FVector(0.0, 0.0, -65.0), 1.2f, 130.0f, FRotator::ZeroRotator)
			.Box(FVector(0.0, 0.0, 0.0), FVector(3.0, 3.0, 10.0))
			.Rod(FVector(-12.0, 0.0, -62.0), 0.15f, 124.0f, FRotator::ZeroRotator)
			.Socket(TEXT("Grip"), FVector(0.0, 0.0, 0.0))
			.Socket(TEXT("Muzzle"), FVector(3.0, 0.0, 3.0))
			.Socket(TEXT("LeftHandIK"), FVector(-12.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_Bow"));
		Meshes.Crossbow = FGreyBox()
			.Box(FVector(0.0, 0.0, -2.0), FVector(60.0, 4.0, 5.0))
			.Box(FVector(28.0, 0.0, 0.0), FVector(3.0, 60.0, 2.0))
			.Box(FVector(-8.0, 0.0, -6.0), FVector(3.0, 3.0, 8.0))
			.Socket(TEXT("Grip"), FVector(-8.0, 0.0, -5.0))
			.Socket(TEXT("LeftGrip"), FVector(12.0, -1.0, -6.5))
			.Socket(TEXT("Muzzle"), FVector(32.0, 0.0, 2.0))
			.Socket(TEXT("Sight"), FVector(-4.0, 0.0, 1.5))
			.Socket(TEXT("Stock"), FVector(-30.0, 0.0, -2.0))
			.Make(TEXT("SM_GB_Crossbow"));

		// Melee: TraceStart and TraceEnd run along the striking part (blade, head).
		Meshes.Sword = FGreyBox()
			.Rod(FVector(-14.0, 0.0, 0.0), 1.4f, 14.0f)
			.Box(FVector(0.5, 0.0, 0.0), FVector(2.0, 18.0, 2.0))
			.Box(FVector(41.0, 0.0, 0.0), FVector(80.0, 1.0, 4.5))
			.Socket(TEXT("Grip"), FVector(-7.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(4.0, 0.0, 0.0))
			.Socket(TEXT("TraceEnd"), FVector(80.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_Sword"));
		Meshes.Dagger = FGreyBox()
			.Rod(FVector(-11.0, 0.0, 0.0), 1.3f, 11.0f)
			.Box(FVector(0.5, 0.0, 0.0), FVector(1.5, 8.0, 1.5))
			.Box(FVector(13.0, 0.0, 0.0), FVector(24.0, 0.8, 3.0))
			.Socket(TEXT("Grip"), FVector(-5.5, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(2.0, 0.0, 0.0))
			.Socket(TEXT("TraceEnd"), FVector(25.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_Dagger"));
		Meshes.Axe = FGreyBox()
			.Rod(FVector(-15.0, 0.0, 0.0), 1.6f, 60.0f)
			.Box(FVector(42.0, 0.0, 6.0), FVector(8.0, 2.0, 14.0))
			.Socket(TEXT("Grip"), FVector(-8.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(38.0, 0.0, 6.0))
			.Socket(TEXT("TraceEnd"), FVector(46.0, 0.0, 13.0))
			.Make(TEXT("SM_GB_HandAxe"));
		Meshes.Club = FGreyBox()
			.Cone(FVector(-12.0, 0.0, 0.0), 1.8f, 4.0f, 70.0f)
			.Socket(TEXT("Grip"), FVector(-5.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(25.0, 0.0, 0.0))
			.Socket(TEXT("TraceEnd"), FVector(58.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_Club"));
		Meshes.Warhammer = FGreyBox()
			.Rod(FVector(-15.0, 0.0, 0.0), 1.6f, 70.0f)
			.Box(FVector(52.0, 0.0, 0.0), FVector(6.0, 6.0, 16.0))
			.Cone(FVector(52.0, 0.0, -8.0), 2.5f, 0.3f, 8.0f, FRotator(180.0, 0.0, 0.0))
			.Socket(TEXT("Grip"), FVector(-8.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(48.0, 0.0, 0.0))
			.Socket(TEXT("TraceEnd"), FVector(56.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_Warhammer"));
		Meshes.ChairLeg = FGreyBox()
			.Box(FVector(18.0, 0.0, 0.0), FVector(50.0, 4.0, 4.0))
			.Socket(TEXT("Grip"), FVector(0.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(20.0, 0.0, 0.0))
			.Socket(TEXT("TraceEnd"), FVector(43.0, 0.0, 0.0))
			.Make(TEXT("SM_GB_ChairLeg"));
		// A round shield faces +X; the hand holds the strap behind its centre.
		Meshes.Buckler = FGreyBox()
			.Rod(FVector(0.0, 0.0, 0.0), 22.0f, 2.5f)
			.Ball(FVector(2.5, 0.0, 0.0), 4.0f)
			.Socket(TEXT("Grip"), FVector(-3.0, 0.0, 0.0))
			.Socket(TEXT("TraceStart"), FVector(3.0, 0.0, -15.0))
			.Socket(TEXT("TraceEnd"), FVector(3.0, 0.0, 15.0))
			.Make(TEXT("SM_GB_Buckler"));

		Meshes.Stone = FGreyBox().Ball(FVector::ZeroVector, 6.0f).Socket(TEXT("Grip"), FVector::ZeroVector).Make(TEXT("SM_GB_Stone"));
		Meshes.Bottle = FGreyBox()
			.Rod(FVector(0.0, 0.0, 0.0), 4.0f, 18.0f, FRotator::ZeroRotator)
			.Cone(FVector(0.0, 0.0, 18.0), 4.0f, 1.2f, 6.0f, FRotator::ZeroRotator)
			.Rod(FVector(0.0, 0.0, 24.0), 1.2f, 5.0f, FRotator::ZeroRotator)
			.Socket(TEXT("Grip"), FVector(0.0, 0.0, 12.0))
			.Make(TEXT("SM_GB_Bottle"));
		Meshes.Cartridge = FGreyBox().Rod(FVector::ZeroVector, 0.6f, 4.5f).Make(TEXT("SM_GB_Cartridge"));
		Meshes.Arrow = FGreyBox()
			.Rod(FVector(-70.0, 0.0, 0.0), 0.4f, 68.0f)
			.Cone(FVector(-2.0, 0.0, 0.0), 0.8f, 0.05f, 3.0f)
			.Box(FVector(-66.0, 0.0, 0.0), FVector(8.0, 0.2, 2.5))
			.Make(TEXT("SM_GB_Arrow"));
		Meshes.Box = FGreyBox().Box(FVector(0.0, 0.0, 5.0), FVector(16.0, 10.0, 10.0)).Make(TEXT("SM_GB_CartridgeBox"));
		return Meshes;
	}

	// --- The mannequin's sockets ---

	/** hand_r_weapon / hand_l_weapon and the sheath sockets on SK_Mannequin (only the missing ones). Adjust them in the Skeleton editor. */
	void AddSkeletonSockets()
	{
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin"));
		if (!Skeleton)
		{
			UE_LOG(LogVaesenCombatAssets, Warning, TEXT("SK_Mannequin not found; weapons will use the fallback bones."));
			return;
		}

		struct FSocketDef { FName Name; FName Bone; FTransform Offset; };
		TArray<FSocketDef> Defs;
		// The hand sockets: the palm, with the weapon pointing out past the thumb (UE5 mannequin's hand axes).
		Defs.Add({ TEXT("hand_r_weapon"), TEXT("hand_r"), FTransform(FRotator(0.0, 0.0, 90.0), FVector(-2.0, 4.0, 0.0)) });
		Defs.Add({ TEXT("hand_l_weapon"), TEXT("hand_l"), FTransform(FRotator(0.0, 0.0, -90.0), FVector(2.0, -4.0, 0.0)) });
		for (const TPair<FName, FVaesenSheathLocation>& Pair : GetDefault<UVaesenWeaponCategoryDefaults>()->SheathLocations)
		{
			Defs.Add({ Pair.Key, Pair.Value.FallbackBone, Pair.Value.FallbackOffset });
		}

		bool bChanged = false;
		for (const FSocketDef& Def : Defs)
		{
			if (Skeleton->FindSocket(Def.Name))
			{
				continue;
			}
			USkeletalMeshSocket* Socket = NewObject<USkeletalMeshSocket>(Skeleton);
			Socket->SocketName = Def.Name;
			Socket->BoneName = Def.Bone;
			Socket->RelativeLocation = Def.Offset.GetLocation();
			Socket->RelativeRotation = Def.Offset.Rotator();
			Skeleton->Sockets.Add(Socket);
			bChanged = true;
		}
		if (bChanged)
		{
			Save(Skeleton);
		}
	}

	// --- Physical materials and data assets ---

	TMap<FName, UPhysicalMaterial*> CreatePhysicalMaterials()
	{
		TMap<FName, UPhysicalMaterial*> Materials;
		const TPair<const TCHAR*, EPhysicalSurface> Surfaces[] = {
			{ TEXT("Flesh"), SurfaceType1 }, { TEXT("Wood"), SurfaceType2 }, { TEXT("Stone"), SurfaceType3 }, { TEXT("Metal"), SurfaceType4 },
			{ TEXT("Earth"), SurfaceType5 }, { TEXT("Water"), SurfaceType6 }, { TEXT("Cloth"), SurfaceType7 },
		};
		for (const TPair<const TCHAR*, EPhysicalSurface>& Surface : Surfaces)
		{
			const FString Path = Join(TEXT("/Game/Vaesen/Combat/PhysicalMaterials"), *FString::Printf(TEXT("PM_%s"), Surface.Key));
			UPhysicalMaterial* Material = LoadAsset<UPhysicalMaterial>(Path);
			if (!Material)
			{
				Material = CreateAsset<UPhysicalMaterial>(Path);
				Material->SurfaceType = Surface.Value;
				Save(Material);
			}
			Materials.Add(Surface.Key, Material);
		}
		return Materials;
	}

	void CreateDataAssets()
	{
		const FString ImpactPath = Join(CombatFolder, TEXT("DA_ImpactEffects"));
		UVaesenImpactEffects* Impacts = LoadAsset<UVaesenImpactEffects>(ImpactPath);
		if (!Impacts)
		{
			// One row per damage type and per surface, empty (silent) until sounds and effects are chosen.
			Impacts = CreateAsset<UVaesenImpactEffects>(ImpactPath);
			Impacts->Entries.AddDefaulted();
			for (const FGameplayTag& Type : TArray<FGameplayTag>{ VaesenTags::Damage_Slashing, VaesenTags::Damage_Piercing, VaesenTags::Damage_Blunt })
			{
				for (const EPhysicalSurface Surface : { SurfaceType1, SurfaceType2, SurfaceType3, SurfaceType4, SurfaceType5 })
				{
					FVaesenImpactEntry& Entry = Impacts->Entries.AddDefaulted_GetRef();
					Entry.DamageType = Type;
					Entry.Surface = Surface;
					Entry.RicochetChance = Type == VaesenTags::Damage_Piercing && (Surface == SurfaceType3 || Surface == SurfaceType4) ? 0.3f : 0.0f;
				}
			}
			Save(Impacts);
		}

		const FString SoundsPath = Join(CombatFolder, TEXT("DA_WeaponSounds_Default"));
		if (UVaesenWeaponSoundSet* Sounds = CreateAsset<UVaesenWeaponSoundSet>(SoundsPath))
		{
			// Every slot listed, empty: fill in placeholder sounds to hear them.
			for (const EVaesenWeaponSound Slot : TEnumRange<EVaesenWeaponSound>())
			{
				Sounds->Sounds.Add(Slot, nullptr);
			}
			Save(Sounds);
		}

		const FString DefaultsPath = Join(CombatFolder, TEXT("DA_WeaponCategoryDefaults"));
		UVaesenWeaponCategoryDefaults* Defaults = LoadAsset<UVaesenWeaponCategoryDefaults>(DefaultsPath);
		if (!Defaults)
		{
			// Starts with the built-in grey-box values (the class defaults).
			Defaults = CreateAsset<UVaesenWeaponCategoryDefaults>(DefaultsPath);
			Defaults->ImpactEffects = Impacts;
			Save(Defaults);
		}

		// Project Settings > Vaesen > Combat points at it in Config/DefaultGame.ini (and the code finds it at this path anyway).
	}

	// --- Effects and abilities ---

	void CreateEffectsAndAbilities()
	{
		const TPair<const TCHAR*, UClass*> Effects[] = {
			{ TEXT("GE_Damage"), UVaesenGE_Damage::StaticClass() },
			{ TEXT("GE_StaminaCost"), UVaesenGE_StaminaCost::StaticClass() },
			{ TEXT("GE_Stagger"), UVaesenGE_Stagger::StaticClass() },
			{ TEXT("GE_StaminaRegen"), UVaesenGE_StaminaRegen::StaticClass() },
		};
		for (const TPair<const TCHAR*, UClass*>& Effect : Effects)
		{
			if (UBlueprint* Blueprint = CreateBlueprint(Join(TEXT("/Game/Vaesen/Combat/Effects"), Effect.Key), Effect.Value))
			{
				Finish(Blueprint);
			}
		}
		// GE_Damage is set as the damage effect in Config/DefaultGame.ini (empty there = the C++ class).

		const TPair<const TCHAR*, UClass*> Abilities[] = {
			{ TEXT("GA_FireWeapon"), UVaesenGA_FireWeapon::StaticClass() },
			{ TEXT("GA_Reload"), UVaesenGA_Reload::StaticClass() },
			{ TEXT("GA_CycleAction"), UVaesenGA_CycleAction::StaticClass() },
			{ TEXT("GA_Aim"), UVaesenGA_Aim::StaticClass() },
			{ TEXT("GA_DrawBow"), UVaesenGA_DrawBow::StaticClass() },
			{ TEXT("GA_MeleeLight"), UVaesenGA_MeleeLight::StaticClass() },
			{ TEXT("GA_MeleeHeavy"), UVaesenGA_MeleeHeavy::StaticClass() },
			{ TEXT("GA_Block"), UVaesenGA_Block::StaticClass() },
			{ TEXT("GA_Throw"), UVaesenGA_Throw::StaticClass() },
			{ TEXT("GA_Holster"), UVaesenGA_Holster::StaticClass() },
			{ TEXT("GA_SwapWeapon"), UVaesenGA_SwapWeapon::StaticClass() },
		};
		UClass* StaminaCost = LoadBlueprintClass(TEXT("/Game/Vaesen/Combat/Effects/GE_StaminaCost"));
		UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *ObjectPathOf(PlayerCharacterPath));
		AVaesenBaseCharacter* CharacterDefaults = Character ? Cast<AVaesenBaseCharacter>(Character->GeneratedClass->GetDefaultObject()) : nullptr;
		bool bCharacterChanged = false;
		for (const TPair<const TCHAR*, UClass*>& Ability : Abilities)
		{
			const FString Path = Join(TEXT("/Game/Vaesen/Combat/Abilities"), Ability.Key);
			if (UBlueprint* Blueprint = CreateBlueprint(Path, Ability.Value))
			{
				if (StaminaCost)
				{
					DefaultsOf<UVaesenGameplayAbility>(Blueprint)->StaminaCostEffect = StaminaCost;
				}
				Finish(Blueprint);
			}
			// The player grants the Blueprints, so tuning them takes effect.
			if (UClass* AbilityBlueprint = LoadBlueprintClass(Path); AbilityBlueprint && CharacterDefaults)
			{
				bCharacterChanged |= CharacterDefaults->ReplaceDefaultAbility(Ability.Value, AbilityBlueprint);
			}
		}
		if (Character && bCharacterChanged)
		{
			Finish(Character);
		}
	}

	// --- Input ---

	UInputAction* GetOrCreateAction(const TCHAR* Name)
	{
		const FString Path = Join(TEXT("/Game/Vaesen/Input"), Name);
		if (UInputAction* Existing = LoadAsset<UInputAction>(Path))
		{
			return Existing;
		}
		UInputAction* Action = CreateAsset<UInputAction>(Path);
		Action->ValueType = EInputActionValueType::Boolean;
		Save(Action);
		return Action;
	}

	/** Maps Action to Key in IMC_Default (once), with optional triggers made by MakeTriggers. */
	bool MapOnce(UInputMappingContext* Mappings, UInputAction* Action, const FKey& Key, TFunction<void(FEnhancedActionKeyMapping&)> Configure = nullptr)
	{
		const bool bMapped = Mappings->GetMappings().ContainsByPredicate([Action, &Key](const FEnhancedActionKeyMapping& Mapping)
		{
			return Mapping.Action == Action && Mapping.Key == Key;
		});
		if (bMapped)
		{
			return false;
		}
		FEnhancedActionKeyMapping& Mapping = Mappings->MapKey(Action, Key);
		if (Configure)
		{
			Configure(Mapping);
		}
		return true;
	}

	void SetupCombatInput()
	{
		UInputMappingContext* Mappings = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default"));
		UBlueprint* Character = LoadObject<UBlueprint>(nullptr, *ObjectPathOf(PlayerCharacterPath));
		if (!Mappings || !Character)
		{
			UE_LOG(LogVaesenCombatAssets, Warning, TEXT("IMC_Default or BP_VaesenPlayerCharacter missing; combat input not set up."));
			return;
		}

		UInputAction* HeavyModifier = GetOrCreateAction(TEXT("IA_HeavyModifier"));
		UInputAction* Attack = GetOrCreateAction(TEXT("IA_Attack"));
		UInputAction* Heavy = GetOrCreateAction(TEXT("IA_HeavyAttack"));
		UInputAction* Trigger = GetOrCreateAction(TEXT("IA_TriggerAttack"));
		UInputAction* AimBlock = GetOrCreateAction(TEXT("IA_AimBlock"));
		UInputAction* Reload = GetOrCreateAction(TEXT("IA_Reload"));
		UInputAction* Throw = GetOrCreateAction(TEXT("IA_Throw"));
		UInputAction* Holster = GetOrCreateAction(TEXT("IA_Holster"));
		UInputAction* Swap = GetOrCreateAction(TEXT("IA_SwapWeapon"));
		UInputAction* Crouch = GetOrCreateAction(TEXT("IA_Crouch"));
		UInputAction* SwitchGrip = GetOrCreateAction(TEXT("IA_SwitchGrip"));

		bool bChanged = false;
		bChanged |= MapOnce(Mappings, HeavyModifier, EKeys::LeftShift);
		// Left mouse alone attacks; with Shift held it's the heavy attack instead (chords, so both stay rebindable).
		bChanged |= MapOnce(Mappings, Attack, EKeys::LeftMouseButton, [Mappings, HeavyModifier](FEnhancedActionKeyMapping& Mapping)
		{
			UInputTriggerChordBlocker* Blocker = NewObject<UInputTriggerChordBlocker>(Mappings);
			Blocker->ChordAction = HeavyModifier;
			Mapping.Triggers.Add(Blocker);
		});
		bChanged |= MapOnce(Mappings, Heavy, EKeys::LeftMouseButton, [Mappings, HeavyModifier](FEnhancedActionKeyMapping& Mapping)
		{
			UInputTriggerChordAction* Chord = NewObject<UInputTriggerChordAction>(Mappings);
			Chord->ChordAction = HeavyModifier;
			Mapping.Triggers.Add(Chord);
		});
		bChanged |= MapOnce(Mappings, Attack, EKeys::Gamepad_RightShoulder);
		bChanged |= MapOnce(Mappings, Trigger, EKeys::Gamepad_RightTriggerAxis);
		bChanged |= MapOnce(Mappings, AimBlock, EKeys::RightMouseButton);
		bChanged |= MapOnce(Mappings, AimBlock, EKeys::Gamepad_LeftTriggerAxis);
		bChanged |= MapOnce(Mappings, Reload, EKeys::R);
		bChanged |= MapOnce(Mappings, Reload, EKeys::Gamepad_FaceButton_Left);
		bChanged |= MapOnce(Mappings, Throw, EKeys::G);
		bChanged |= MapOnce(Mappings, Throw, EKeys::Gamepad_LeftShoulder);
		bChanged |= MapOnce(Mappings, Holster, EKeys::H);
		bChanged |= MapOnce(Mappings, Holster, EKeys::Gamepad_FaceButton_Top, [Mappings](FEnhancedActionKeyMapping& Mapping)
		{
			UInputTriggerHold* Hold = NewObject<UInputTriggerHold>(Mappings);
			Hold->HoldTimeThreshold = 0.35f;
			Hold->bIsOneShot = true;
			Mapping.Triggers.Add(Hold);
		});
		bChanged |= MapOnce(Mappings, Swap, EKeys::Q);
		bChanged |= MapOnce(Mappings, Crouch, EKeys::C);
		bChanged |= MapOnce(Mappings, Crouch, EKeys::Gamepad_FaceButton_Right);
		bChanged |= MapOnce(Mappings, SwitchGrip, EKeys::V);
		bChanged |= MapOnce(Mappings, SwitchGrip, EKeys::Gamepad_DPad_Down);
		if (bChanged)
		{
			Save(Mappings);
		}

		UObject* Defaults = Character->GeneratedClass->GetDefaultObject();
		bool bCharacterChanged = false;
		const TPair<const TCHAR*, UInputAction*> Properties[] = {
			{ TEXT("AttackAction"), Attack }, { TEXT("HeavyAttackAction"), Heavy }, { TEXT("TriggerAttackAction"), Trigger },
			{ TEXT("AimOrBlockAction"), AimBlock }, { TEXT("ReloadAction"), Reload }, { TEXT("ThrowAction"), Throw },
			{ TEXT("HolsterAction"), Holster }, { TEXT("SwapWeaponAction"), Swap },
			{ TEXT("CrouchAction"), Crouch },
			{ TEXT("SwitchGripAction"), SwitchGrip },
		};
		for (const TPair<const TCHAR*, UInputAction*>& Property : Properties)
		{
			const FObjectPropertyBase* Found = FindFProperty<FObjectPropertyBase>(Defaults->GetClass(), Property.Key);
			if (Found && !Found->GetObjectPropertyValue_InContainer(Defaults))
			{
				SetObjectProperty(Defaults, Property.Key, Property.Value);
				bCharacterChanged = true;
			}
		}
		if (bCharacterChanged)
		{
			Finish(Character);
		}
	}

	// --- Items ---

	void SetItemBasics(UVaesenItem* Item, const FText& Name, const FText& Description, UStaticMesh* Mesh, float Weight, int32 SlotSize = 1)
	{
		Item->ItemName = Name;
		Item->Description = Description;
		Item->PickupMesh = Mesh;
		Item->Weight = Weight;
		Item->SlotSize = SlotSize;
	}

	void SetWeaponBasics(UVaesenWeaponItem* Weapon, EVaesenWeaponCategory Category, EVaesenEquipmentSlot Slot, const TCHAR* Sheath, bool bTwoHanded, UStaticMesh* Mesh)
	{
		Weapon->Category = Category;
		Weapon->Slot = Slot;
		Weapon->SheathSocket = Sheath;
		Weapon->bTwoHanded = bTwoHanded;
		Weapon->WeaponMesh = Mesh;
	}

	void MakeThrowable(UVaesenItem* Item, EVaesenThrowStyle Style, const FVaesenDamagePacket& ThrowDamage, float Speed = 1400.0f)
	{
		Item->Throwing.bCanBeThrown = true;
		Item->Throwing.Style = Style;
		Item->Throwing.Damage = ThrowDamage;
		Item->Throwing.Speed = Speed;
		Item->Throwing.SpinRate = Style == EVaesenThrowStyle::Spinning ? 3.0f : Style == EVaesenThrowStyle::PointFirst ? 0.0f : 1.5f;
	}

	void CreateTestItems(const FMeshes& Meshes)
	{
		using namespace VaesenTags;

		// Ammo.
		struct FAmmo { const TCHAR* Name; FText Title; FText Description; FVaesenDamagePacket Damage; bool bSticks; UStaticMesh* Projectile; float Gravity; };
		FVaesenDamagePacket Ball = Damage(Damage_Piercing, 34.0f);
		Ball.Add(Damage_Blunt, 6.0f);
		const FAmmo Ammo[] = {
			{ TEXT("BP_Ammo_RevolverCartridge"), LOCTEXT("RevolverCartridge", "Revolver cartridges"), LOCTEXT("RevolverCartridgeDesc", "Center-fire cartridges for a revolver."), Damage(Damage_Piercing, 18.0f), false, nullptr, 1.0f },
			{ TEXT("BP_Ammo_KragCartridge"), LOCTEXT("KragCartridge", "Krag cartridges"), LOCTEXT("KragCartridgeDesc", "8x58R cartridges for the Krag-Jørgensen."), Damage(Damage_Piercing, 30.0f), false, nullptr, 1.0f },
			{ TEXT("BP_Ammo_PowderAndBall"), LOCTEXT("PowderBall", "Powder and ball"), LOCTEXT("PowderBallDesc", "A measure of black powder and a lead ball."), Ball, false, nullptr, 1.0f },
			{ TEXT("BP_Ammo_Arrow"), LOCTEXT("Arrow", "Arrows"), LOCTEXT("ArrowDesc", "Goose-fletched hunting arrows."), Damage(Damage_Piercing, 22.0f), true, Meshes.Arrow, 1.0f },
			{ TEXT("BP_Ammo_CrossbowBolt"), LOCTEXT("Bolt", "Crossbow bolts"), LOCTEXT("BoltDesc", "Short, heavy bolts."), Damage(Damage_Piercing, 28.0f), true, Meshes.Arrow, 0.8f },
		};
		for (const FAmmo& Entry : Ammo)
		{
			if (UBlueprint* Blueprint = CreateBlueprint(Join(AmmoFolder, Entry.Name), UVaesenAmmoItem::StaticClass()))
			{
				UVaesenAmmoItem* Defaults = DefaultsOf<UVaesenAmmoItem>(Blueprint);
				SetItemBasics(Defaults, Entry.Title, Entry.Description, Entry.Projectile ? Entry.Projectile : Meshes.Cartridge, 0.02f, 1);
				Defaults->Damage = Entry.Damage;
				Defaults->bSticksInSurfaces = Entry.bSticks;
				Defaults->bRecoverable = Entry.bSticks;
				Defaults->ProjectileMesh = Entry.Projectile;
				Defaults->ProjectileScale = Entry.Projectile ? FVector::OneVector : FVector(0.04);
				Defaults->GravityScale = Entry.Gravity;
				Defaults->MaxStackSize = 20;
				Finish(Blueprint);
			}
		}

		if (UBlueprint* Blueprint = CreateBlueprint(Join(AmmoFolder, TEXT("BP_Item_CartridgeBox")), UVaesenPouchItem::StaticClass()))
		{
			UVaesenPouchItem* Defaults = DefaultsOf<UVaesenPouchItem>(Blueprint);
			SetItemBasics(Defaults, LOCTEXT("CartridgeBox", "Cartridge box"), LOCTEXT("CartridgeBoxDesc", "A tin box for cartridges, powder and shot. Ammunition you pick up goes in here."), Meshes.Box, 0.3f);
			Defaults->PouchSlots = 4;
			Defaults->AcceptedItemTags.AddTag(Item_Ammo);
			Finish(Blueprint);
		}

		const auto AmmoClass = [](const TCHAR* Name) { return LoadBlueprintClass(Join(AmmoFolder, Name)); };

		// Ranged weapons.
		struct FRanged
		{
			const TCHAR* Name; FText Title; FText Description; UStaticMesh* Mesh;
			EVaesenWeaponCategory Category; EVaesenEquipmentSlot Slot; const TCHAR* Sheath; bool bTwoHanded;
			EVaesenShotType Shot; EVaesenWeaponAction Action; EVaesenReloadStyle Reload; EVaesenCasingEjection Casings;
			int32 Magazine; float FireInterval; float ReloadTime; float LaunchSpeed; const TCHAR* Ammo; float SmokeScale; int32 SlotSize;
		};
		const FRanged Ranged[] = {
			{ TEXT("BP_Weapon_Revolver"), LOCTEXT("Revolver", "Revolver"), LOCTEXT("RevolverDesc", "A six-shot service revolver."), Meshes.Revolver,
				EVaesenWeaponCategory::Revolver, EVaesenEquipmentSlot::Sidearm, TEXT("Sheath_HipRight"), false,
				EVaesenShotType::InstantHit, EVaesenWeaponAction::Revolver, EVaesenReloadStyle::Cylinder, EVaesenCasingEjection::OnReload,
				6, 0.4f, 0.6f, 0.0f, TEXT("BP_Ammo_RevolverCartridge"), 0.6f, 1 },
			{ TEXT("BP_Weapon_KragRifle"), LOCTEXT("Krag", "Krag-Jørgensen rifle"), LOCTEXT("KragDesc", "A bolt-action army rifle with a five-round magazine."), Meshes.Rifle,
				EVaesenWeaponCategory::Rifle, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackSling"), true,
				EVaesenShotType::Projectile, EVaesenWeaponAction::BoltAction, EVaesenReloadStyle::Magazine, EVaesenCasingEjection::OnCycle,
				5, 0.3f, 2.4f, 30000.0f, TEXT("BP_Ammo_KragCartridge"), 0.8f, 3 },
			{ TEXT("BP_Weapon_MuzzleLoader"), LOCTEXT("MuzzleLoader", "Muzzle-loading musket"), LOCTEXT("MuzzleLoaderDesc", "An old flintlock musket. Loud, smoky and slow to load."), Meshes.MuzzleLoader,
				EVaesenWeaponCategory::Rifle, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackSling"), true,
				EVaesenShotType::InstantHit, EVaesenWeaponAction::SingleShot, EVaesenReloadStyle::MuzzleLoading, EVaesenCasingEjection::None,
				1, 1.0f, 7.0f, 0.0f, TEXT("BP_Ammo_PowderAndBall"), 3.0f, 3 },
			{ TEXT("BP_Weapon_RollingBlockPistol"), LOCTEXT("Pistol", "Rolling-block pistol"), LOCTEXT("PistolDesc", "A single-shot pistol; open the breech to reload."), Meshes.Pistol,
				EVaesenWeaponCategory::Pistol, EVaesenEquipmentSlot::Sidearm, TEXT("Sheath_HipRight"), false,
				EVaesenShotType::InstantHit, EVaesenWeaponAction::SingleShot, EVaesenReloadStyle::SingleRound, EVaesenCasingEjection::OnReload,
				1, 0.5f, 1.4f, 0.0f, TEXT("BP_Ammo_RevolverCartridge"), 0.6f, 1 },
			{ TEXT("BP_Weapon_HuntingBow"), LOCTEXT("Bow", "Hunting bow"), LOCTEXT("BowDesc", "A yew longbow. Draw fully for the strongest shot."), Meshes.Bow,
				EVaesenWeaponCategory::Bow, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackUpper"), true,
				EVaesenShotType::Projectile, EVaesenWeaponAction::Draw, EVaesenReloadStyle::SingleRound, EVaesenCasingEjection::None,
				1, 0.4f, 0.5f, 6000.0f, TEXT("BP_Ammo_Arrow"), 0.0f, 3 },
			{ TEXT("BP_Weapon_Crossbow"), LOCTEXT("Crossbow", "Crossbow"), LOCTEXT("CrossbowDesc", "Long to load, then it fires at once."), Meshes.Crossbow,
				EVaesenWeaponCategory::Crossbow, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackSling"), true,
				EVaesenShotType::Projectile, EVaesenWeaponAction::SingleShot, EVaesenReloadStyle::SingleRound, EVaesenCasingEjection::None,
				1, 0.5f, 3.5f, 7000.0f, TEXT("BP_Ammo_CrossbowBolt"), 0.0f, 3 },
		};
		for (const FRanged& Entry : Ranged)
		{
			if (UBlueprint* Blueprint = CreateBlueprint(Join(WeaponsFolder, Entry.Name), UVaesenRangedWeaponItem::StaticClass()))
			{
				UVaesenRangedWeaponItem* Defaults = DefaultsOf<UVaesenRangedWeaponItem>(Blueprint);
				SetItemBasics(Defaults, Entry.Title, Entry.Description, Entry.Mesh, Entry.bTwoHanded ? 4.0f : 1.2f, Entry.SlotSize);
				SetWeaponBasics(Defaults, Entry.Category, Entry.Slot, Entry.Sheath, Entry.bTwoHanded, Entry.Mesh);
				Defaults->ShotType = Entry.Shot;
				Defaults->Action = Entry.Action;
				Defaults->ReloadStyle = Entry.Reload;
				Defaults->CasingEjection = Entry.Casings;
				Defaults->MagazineSize = Entry.Magazine;
				Defaults->FireInterval = Entry.FireInterval;
				Defaults->ReloadTime = Entry.ReloadTime;
				Defaults->LaunchSpeed = Entry.LaunchSpeed > 0.0f ? Entry.LaunchSpeed : Defaults->LaunchSpeed;
				Defaults->Effects.SmokeScale = Entry.SmokeScale;
				Defaults->AcceptedAmmo = AmmoClass(Entry.Ammo);
				Defaults->CasingMesh = Entry.Casings != EVaesenCasingEjection::None ? Meshes.Cartridge : nullptr;
				if (Entry.Category == EVaesenWeaponCategory::Rifle)
				{
					Defaults->SpreadHip = 5.0f;
					Defaults->SpreadAimed = 0.3f;
					Defaults->EffectiveRange = 8000.0f;
					Defaults->MaxRange = 30000.0f;
					Defaults->AimFieldOfView = 50.0f;
				}
				Finish(Blueprint);
			}
		}

		// Melee weapons (and a shield), all throwable.
		struct FMelee
		{
			const TCHAR* Name; FText Title; FText Description; UStaticMesh* Mesh;
			EVaesenWeaponCategory Category; EVaesenEquipmentSlot Slot; const TCHAR* Sheath;
			EVaesenThrowStyle ThrowStyle; FVaesenDamagePacket ThrowDamage; float MaxDurability;
		};
		const FMelee Melee[] = {
			{ TEXT("BP_Weapon_OldSword"), LOCTEXT("OldSword", "Old sword"), LOCTEXT("OldSwordDesc", "A notched infantry sabre from some forgotten war."), Meshes.Sword,
				EVaesenWeaponCategory::OneHandedSword, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_HipLeft"), EVaesenThrowStyle::Overhand, Damage(Damage_Slashing, 10.0f), 60.0f },
			{ TEXT("BP_Weapon_Dagger"), LOCTEXT("Dagger", "Dagger"), LOCTEXT("DaggerDesc", "A short, sharp knife."), Meshes.Dagger,
				EVaesenWeaponCategory::Dagger, EVaesenEquipmentSlot::Sidearm, TEXT("Sheath_Belt"), EVaesenThrowStyle::PointFirst, Damage(Damage_Piercing, 12.0f), 50.0f },
			{ TEXT("BP_Weapon_HandAxe"), LOCTEXT("HandAxe", "Hand axe"), LOCTEXT("HandAxeDesc", "A woodsman's hatchet."), Meshes.Axe,
				EVaesenWeaponCategory::OneHandedAxe, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackLower"), EVaesenThrowStyle::Spinning, Damage(Damage_Slashing, 14.0f), 80.0f },
			{ TEXT("BP_Weapon_Club"), LOCTEXT("Club", "Club"), LOCTEXT("ClubDesc", "A knotted length of birch."), Meshes.Club,
				EVaesenWeaponCategory::Club, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackLower"), EVaesenThrowStyle::Overhand, Damage(Damage_Blunt, 8.0f), 70.0f },
			{ TEXT("BP_Weapon_Warhammer"), LOCTEXT("Warhammer", "Warhammer"), LOCTEXT("WarhammerDesc", "An old horseman's hammer with a spike."), Meshes.Warhammer,
				EVaesenWeaponCategory::Warhammer, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_BackUpper"), EVaesenThrowStyle::Overhand, Damage(Damage_Blunt, 12.0f), 120.0f },
			{ TEXT("BP_Weapon_ChairLeg"), LOCTEXT("ChairLeg", "Chair leg"), LOCTEXT("ChairLegDesc", "Better than nothing."), Meshes.ChairLeg,
				EVaesenWeaponCategory::Improvised, EVaesenEquipmentSlot::MainHand, TEXT("Sheath_Belt"), EVaesenThrowStyle::Overhand, Damage(Damage_Blunt, 5.0f), 25.0f },
			{ TEXT("BP_Weapon_Buckler"), LOCTEXT("Buckler", "Rusty buckler"), LOCTEXT("BucklerDesc", "A small round shield from a manor's armory."), Meshes.Buckler,
				EVaesenWeaponCategory::Shield, EVaesenEquipmentSlot::OffHand, TEXT("Sheath_BackUpper"), EVaesenThrowStyle::Spinning, Damage(Damage_Blunt, 6.0f), 200.0f },
		};
		for (const FMelee& Entry : Melee)
		{
			if (UBlueprint* Blueprint = CreateBlueprint(Join(WeaponsFolder, Entry.Name), UVaesenMeleeWeaponItem::StaticClass()))
			{
				UVaesenMeleeWeaponItem* Defaults = DefaultsOf<UVaesenMeleeWeaponItem>(Blueprint);
				SetItemBasics(Defaults, Entry.Title, Entry.Description, Entry.Mesh, 1.5f, Entry.Category == EVaesenWeaponCategory::Dagger ? 1 : 2);
				SetWeaponBasics(Defaults, Entry.Category, Entry.Slot, Entry.Sheath, false, Entry.Mesh);
				Defaults->MaxDurability = Entry.MaxDurability;
				MakeThrowable(Defaults, Entry.ThrowStyle, Entry.ThrowDamage);
				Finish(Blueprint);
			}
		}

		// Plain throwables.
		if (UBlueprint* Blueprint = CreateBlueprint(Join(ThrowablesFolder, TEXT("BP_Item_Stone")), UVaesenItem::StaticClass()))
		{
			UVaesenItem* Defaults = DefaultsOf<UVaesenItem>(Blueprint);
			SetItemBasics(Defaults, LOCTEXT("Stone", "Stone"), LOCTEXT("StoneDesc", "A fist-sized stone. Throw it (G)."), Meshes.Stone, 0.5f);
			Defaults->bStackable = true;
			Defaults->MaxStackSize = 10;
			MakeThrowable(Defaults, EVaesenThrowStyle::Lob, Damage(Damage_Blunt, 6.0f), 1300.0f);
			Finish(Blueprint);
		}
		if (UBlueprint* Blueprint = CreateBlueprint(Join(ThrowablesFolder, TEXT("BP_Item_Bottle")), UVaesenItem::StaticClass()))
		{
			UVaesenItem* Defaults = DefaultsOf<UVaesenItem>(Blueprint);
			SetItemBasics(Defaults, LOCTEXT("Bottle", "Empty bottle"), LOCTEXT("BottleDesc", "A green glass bottle. It will break."), Meshes.Bottle, 0.4f);
			Defaults->bStackable = true;
			Defaults->MaxStackSize = 5;
			MakeThrowable(Defaults, EVaesenThrowStyle::Lob, Damage(Damage_Blunt, 3.0f), 1200.0f);
			Defaults->Throwing.bBreaksOnImpact = true;
			Finish(Blueprint);
		}
	}

	// --- Dummies ---

	void CreateDummies(const TMap<FName, UPhysicalMaterial*>& Materials)
	{
		UPhysicalMaterial* Flesh = Materials.FindRef(TEXT("Flesh"));
		const FString Folder = TEXT("/Game/Vaesen/Blueprints/Combat");
		if (UBlueprint* Blueprint = CreateBlueprint(Folder / TEXT("BP_TrainingDummy"), AVaesenTrainingDummy::StaticClass()))
		{
			DefaultsOf<AVaesenTrainingDummy>(Blueprint)->SurfaceMaterial = Flesh;
			Finish(Blueprint);
		}
		if (UBlueprint* Blueprint = CreateBlueprint(Folder / TEXT("BP_TrainingDummy_Armored"), AVaesenTrainingDummy::StaticClass()))
		{
			// Mail over padding: good against blades, some against points, weak against blunt.
			AVaesenTrainingDummy* Defaults = DefaultsOf<AVaesenTrainingDummy>(Blueprint);
			Defaults->SurfaceMaterial = Materials.FindRef(TEXT("Metal"));
			Defaults->ProtectionSlashing = 8.0f;
			Defaults->ProtectionPiercing = 4.0f;
			Defaults->ProtectionBlunt = -2.0f;
			Finish(Blueprint);
		}
		if (UBlueprint* Blueprint = CreateBlueprint(Folder / TEXT("BP_TrainingDummy_Attacker"), AVaesenTrainingDummyAttacker::StaticClass()))
		{
			DefaultsOf<AVaesenTrainingDummy>(Blueprint)->SurfaceMaterial = Flesh;
			Finish(Blueprint);
		}
	}

	// --- The test map ---

	AStaticMeshActor* PlaceBlock(UWorld* World, const FVector& Location, const FVector& Scale, UPhysicalMaterial* Surface, const TCHAR* Label)
	{
		AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Block->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Block->SetActorScale3D(Scale);
		if (Surface)
		{
			Block->GetStaticMeshComponent()->SetPhysMaterialOverride(Surface);
		}
		Block->SetActorLabel(Label);
		return Block;
	}

	void PlacePickup(UWorld* World, UClass* PickupClass, const FString& ItemPath, int32 Quantity, const FVector& Location)
	{
		UClass* ItemClass = LoadBlueprintClass(ItemPath);
		if (!PickupClass || !ItemClass)
		{
			UE_LOG(LogVaesenCombatAssets, Warning, TEXT("Can't place a pickup of %s."), *ItemPath);
			return;
		}
		AActor* Pickup = World->SpawnActor<AActor>(PickupClass, Location, FRotator::ZeroRotator);
		SetObjectProperty(Pickup, TEXT("ItemTemplate"), ItemClass);
		SetIntProperty(Pickup, TEXT("Quantity"), Quantity);
		Pickup->RerunConstructionScripts();
		Pickup->SetActorLabel(FPackageName::GetShortName(ItemPath));
	}

	/** Sun, sky light, atmosphere, clouds and a little fog. */
	void AddDaylight(UWorld* World)
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.0, 0.0, 1000.0), FRotator(-50.0, 30.0, 0.0));
		Sun->GetComponent()->SetMobility(EComponentMobility::Movable);
		Sun->SetActorLabel(TEXT("Sun"));
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.0, 0.0, 800.0), FRotator::ZeroRotator);
		Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		Sky->GetLightComponent()->bRealTimeCapture = true;
		Sky->SetActorLabel(TEXT("SkyLight"));
		// A real sky for the sun and the sky light to work with: atmosphere, clouds, a little fog.
		if (UDirectionalLightComponent* SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			SunLight->SetAtmosphereSunLight(true);
		}
		World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator)->SetActorLabel(TEXT("SkyAtmosphere"));
		World->SpawnActor<AVolumetricCloud>(FVector::ZeroVector, FRotator::ZeroRotator)->SetActorLabel(TEXT("VolumetricCloud"));
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector(0.0, 0.0, -100.0), FRotator::ZeroRotator);
		Fog->GetComponent()->SetFogDensity(0.01f);
		Fog->SetActorLabel(TEXT("HeightFog"));
	}

	/**
	 * L_WeaponFit: a Weapon Fit rig holding the Krag, on a floor in daylight. Select the rig, pick a weapon and a pose,
	 * and move the weapon with the gizmo (AVaesenWeaponFitRig).
	 */
	void CreateWeaponFitMap()
	{
		const TCHAR* FitMapPath = TEXT("/Game/Vaesen/Maps/L_WeaponFit");
		if (Exists(FitMapPath))
		{
			UE_LOG(LogVaesenCombatAssets, Display, TEXT("%s already exists; leaving it alone."), FitMapPath);
			return;
		}
		UPackage* Package = CreatePackage(FitMapPath);
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(FPackageName::GetShortName(FitMapPath)), Package);
		World->SetFlags(RF_Public | RF_Standalone);

		PlaceBlock(World, FVector(0.0, 0.0, -10.0), FVector(10.0, 10.0, 0.2), nullptr, TEXT("Floor"));
		AddDaylight(World);
		AVaesenWeaponFitRig* Rig = World->SpawnActor<AVaesenWeaponFitRig>(FVector::ZeroVector, FRotator::ZeroRotator);
		Rig->SetActorLabel(TEXT("WeaponFitRig"));
		Rig->Weapon = LoadBlueprintClass(Join(WeaponsFolder, TEXT("BP_Weapon_KragRifle")));
		Rig->RerunConstructionScripts();

		FAssetRegistryModule::AssetCreated(World);
		Save(World, true);
		World->DestroyWorld(false);
	}

	/**
	 * L_CombatTest: a floor, light, a row of pickups (every test weapon, ammo, the cartridge box, stones, bottles), the
	 * three training dummies close by, a stone and a wooden wall, and a far dummy and wall for long shots.
	 */
	void CreateTestMap(const TMap<FName, UPhysicalMaterial*>& Materials)
	{
		if (Exists(MapPath))
		{
			UE_LOG(LogVaesenCombatAssets, Display, TEXT("%s already exists; leaving it alone."), MapPath);
			return;
		}

		UPackage* Package = CreatePackage(MapPath);
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false, FName(FPackageName::GetShortName(MapPath)), Package);
		World->SetFlags(RF_Public | RF_Standalone);

		PlaceBlock(World, FVector(2000.0, 0.0, -10.0), FVector(100.0, 40.0, 0.2), Materials.FindRef(TEXT("Earth")), TEXT("Floor"));
		PlaceBlock(World, FVector(900.0, -500.0, 150.0), FVector(0.4, 4.0, 3.0), Materials.FindRef(TEXT("Stone")), TEXT("StoneWall"));
		PlaceBlock(World, FVector(900.0, 500.0, 150.0), FVector(0.4, 4.0, 3.0), Materials.FindRef(TEXT("Wood")), TEXT("WoodWall"));
		PlaceBlock(World, FVector(5200.0, 0.0, 250.0), FVector(0.5, 10.0, 5.0), Materials.FindRef(TEXT("Wood")), TEXT("FarWoodWall"));

		AddDaylight(World);

		APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(0.0, -100.0, 100.0), FRotator::ZeroRotator);
		Start->SetActorLabel(TEXT("PlayerStart_A"));
		APlayerStart* Start2 = World->SpawnActor<APlayerStart>(FVector(0.0, 100.0, 100.0), FRotator::ZeroRotator);
		Start2->SetActorLabel(TEXT("PlayerStart_B"));

		const FString DummyFolder = TEXT("/Game/Vaesen/Blueprints/Combat");
		const TPair<const TCHAR*, FVector> Dummies[] = {
			{ TEXT("BP_TrainingDummy"), FVector(500.0, -150.0, 90.0) },
			{ TEXT("BP_TrainingDummy"), FVector(500.0, -60.0, 90.0) },
			{ TEXT("BP_TrainingDummy_Armored"), FVector(500.0, 150.0, 90.0) },
			{ TEXT("BP_TrainingDummy_Attacker"), FVector(300.0, 400.0, 90.0) },
			{ TEXT("BP_TrainingDummy"), FVector(5000.0, 0.0, 90.0) },
		};
		for (const TPair<const TCHAR*, FVector>& Dummy : Dummies)
		{
			if (UClass* DummyClass = LoadBlueprintClass(DummyFolder / Dummy.Key))
			{
				AActor* Placed = World->SpawnActor<AActor>(DummyClass, Dummy.Value, FRotator(0.0, 180.0, 0.0));
				Placed->SetActorLabel(Dummy.Key);
			}
		}

		UClass* PickupClass = LoadBlueprintClass(PickupPath);
		if (!PickupClass)
		{
			PickupClass = AVaesenPickup::StaticClass();
		}
		const TPair<FString, int32> Pickups[] = {
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Revolver")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_KragRifle")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_MuzzleLoader")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_RollingBlockPistol")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_HuntingBow")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Crossbow")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_OldSword")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Dagger")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_HandAxe")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Club")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Warhammer")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_ChairLeg")), 1 },
			{ Join(WeaponsFolder, TEXT("BP_Weapon_Buckler")), 1 },
			{ Join(AmmoFolder, TEXT("BP_Item_CartridgeBox")), 1 },
			{ Join(AmmoFolder, TEXT("BP_Ammo_RevolverCartridge")), 12 },
			{ Join(AmmoFolder, TEXT("BP_Ammo_KragCartridge")), 10 },
			{ Join(AmmoFolder, TEXT("BP_Ammo_PowderAndBall")), 5 },
			{ Join(AmmoFolder, TEXT("BP_Ammo_Arrow")), 10 },
			{ Join(AmmoFolder, TEXT("BP_Ammo_CrossbowBolt")), 5 },
			{ Join(ThrowablesFolder, TEXT("BP_Item_Stone")), 5 },
			{ Join(ThrowablesFolder, TEXT("BP_Item_Bottle")), 3 },
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Pickups); ++Index)
		{
			// Rows of seven, left of the start, 1.5 m apart so long guns don't lie over their neighbours.
			const FVector Location(-300.0 + (Index % 7) * 150.0, -400.0 - (Index / 7) * 150.0, 20.0);
			PlacePickup(World, PickupClass, Pickups[Index].Key, Pickups[Index].Value, Location);
		}

		FAssetRegistryModule::AssetCreated(World);
		Save(World, true);
		World->DestroyWorld(false);
	}
	/**
	 * Melee weapons that can also be held in both hands (V switches; a shield means one): the sword, the hand axe, the
	 * club, the warhammer. Each gets a LeftGrip socket on its handle for the second hand (grey-box places: tune them in
	 * the Weapon Fit rig once there are two-handed animations).
	 */
	void MakeVersatile()
	{
		struct FVersatile { const TCHAR* Weapon; const TCHAR* Mesh; FVector LeftGrip; };
		const FVersatile Weapons[] = {
			{ TEXT("BP_Weapon_OldSword"), TEXT("SM_GB_Sword"), FVector(-13.0, 0.0, 0.0) },
			{ TEXT("BP_Weapon_HandAxe"), TEXT("SM_GB_HandAxe"), FVector(6.0, 0.0, 0.0) },
			{ TEXT("BP_Weapon_Club"), TEXT("SM_GB_Club"), FVector(9.0, 0.0, 0.0) },
			{ TEXT("BP_Weapon_Warhammer"), TEXT("SM_GB_Warhammer"), FVector(6.0, 0.0, 0.0) },
		};
		for (const FVersatile& Each : Weapons)
		{
			if (UBlueprint* Blueprint = LoadAsset<UBlueprint>(Join(WeaponsFolder, Each.Weapon)))
			{
				UVaesenWeaponItem* Defaults = DefaultsOf<UVaesenWeaponItem>(Blueprint);
				if (!Defaults->bCanBeTwoHanded && !Defaults->bTwoHanded)
				{
					Defaults->bCanBeTwoHanded = true;
					Finish(Blueprint);
				}
			}
			UStaticMesh* Mesh = LoadAsset<UStaticMesh>(Join(GreyBoxFolder, Each.Mesh));
			if (Mesh && !Mesh->FindSocket(TEXT("LeftGrip")))
			{
				UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(Mesh);
				Socket->SocketName = TEXT("LeftGrip");
				Socket->RelativeLocation = Each.LeftGrip;
				Mesh->AddSocket(Socket);
				Save(Mesh);
			}
		}
	}

}

void VaesenCombatAssets::SetupCombat()
{
	const FMeshes Meshes = BuildGreyBoxMeshes();
	AddSkeletonSockets();
	const TMap<FName, UPhysicalMaterial*> Materials = CreatePhysicalMaterials();
	CreateDataAssets();
	CreateEffectsAndAbilities();
	SetupCombatInput();
	CreateTestItems(Meshes);
	MakeVersatile();
	CreateDummies(Materials);
	CreateTestMap(Materials);
	CreateWeaponFitMap();
}

#undef LOCTEXT_NAMESPACE
