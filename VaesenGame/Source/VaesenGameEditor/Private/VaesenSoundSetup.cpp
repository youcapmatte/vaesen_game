#include "VaesenSoundSetupCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "Items/VaesenRangedWeaponItem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeDelay.h"
#include "Sound/SoundNodeDistanceCrossFade.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Weapons/VaesenWeaponData.h"
#include "Weapons/VaesenWeaponTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenSoundSetup, Log, All);

namespace VaesenSoundSetup
{
	const TCHAR* Pack = TEXT("/Game/WeaponVerse1/Source");
	const TCHAR* Folder = TEXT("/Game/Vaesen/Audio/Weapons");
	const TCHAR* WeaponsFolder = TEXT("/Game/Vaesen/Blueprints/Items/Weapons");

	bool bRebuild = false;

	template <typename T>
	T* LoadAt(const FString& PackagePath)
	{
		return LoadObject<T>(nullptr, *(PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath)));
	}

	template <typename T>
	T* PackAsset(const FString& Relative)
	{
		T* Asset = LoadAt<T>(FString(Pack) / Relative);
		if (!Asset)
		{
			UE_LOG(LogVaesenSoundSetup, Warning, TEXT("Not in the pack: %s"), *Relative);
		}
		return Asset;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const bool bSaved = UPackage::SavePackage(Package, Asset, *FileName, Args);
		UE_LOG(LogVaesenSoundSetup, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *Package->GetName());
		return bSaved;
	}

	/** A new (or, rebuilding, emptied) Sound Cue at Name; null if it exists and isn't to be rebuilt. */
	USoundCue* OpenCue(const FString& Name, USoundCue*& OutExisting)
	{
		const FString Path = FString(Folder) / Name;
		OutExisting = LoadAt<USoundCue>(Path);
		if (OutExisting && !bRebuild)
		{
			return nullptr;
		}
		USoundCue* Cue = OutExisting;
		if (!Cue)
		{
			UPackage* Package = CreatePackage(*Path);
			Cue = NewObject<USoundCue>(Package, FName(Name), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Cue);
		}
		else
		{
			Cue->ResetGraph();
		}
		return Cue;
	}

	USoundNodeWavePlayer* Player(USoundCue* Cue, USoundWave* Wave)
	{
		USoundNodeWavePlayer* Node = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
		Node->SetSoundWave(Wave);
		return Node;
	}

	/** A slight random pitch and loudness on top, so the same sound twice doesn't sound copied. */
	USoundNode* Vary(USoundCue* Cue, USoundNode* Child, float Pitch = 0.03f)
	{
		USoundNodeModulator* Modulator = Cue->ConstructSoundNode<USoundNodeModulator>();
		Modulator->PitchMin = 1.0f - Pitch;
		Modulator->PitchMax = 1.0f + Pitch;
		Modulator->VolumeMin = 0.9f;
		Modulator->VolumeMax = 1.0f;
		Modulator->ChildNodes.Add(Child);
		return Modulator;
	}

	/** A node that takes several inputs, with none yet (new ones come with empty ones). */
	template <typename T>
	T* Many(USoundCue* Cue)
	{
		T* Node = Cue->ConstructSoundNode<T>();
		Node->ChildNodes.Reset();
		return Node;
	}

	USoundCue* Finish(USoundCue* Cue, USoundNode* First)
	{
		Cue->FirstNode = First;
	// The editor graph's pins, one per input as the nodes now are.
		for (USoundNode* Node : Cue->AllNodes)
		{
			if (Node && Node->GetGraphNode())
			{
				Node->GetGraphNode()->ReconstructNode();
			}
		}
		Cue->LinkGraphNodesFromSoundNodes();
		Cue->CacheAggregateValues();
		Cue->PostEditChange();
		Save(Cue);
		return Cue;
	}

	/**
	 * A gunshot heard as it is at the listener's distance: the 5, 25, 100 and 300 m recordings, each crossfading into
	 * the next (cm). Recordings missing from the pack are left out.
	 */
	USoundCue* DistanceShot(const FString& Name, const TCHAR* Group, const TCHAR* Gun)
	{
		USoundCue* Existing;
		USoundCue* Cue = OpenCue(Name, Existing);
		if (!Cue)
		{
			return Existing;
		}
		struct FBand { const TCHAR* Folder; const TCHAR* Suffix; float InStart, InEnd, OutStart, OutEnd; };
		const FBand Bands[] = {
			{ TEXT("5M"), TEXT("5M"), 0.0f, 0.0f, 1500.0f, 3000.0f },
			{ TEXT("25M"), TEXT("25M"), 1500.0f, 3000.0f, 5000.0f, 9000.0f },
			{ TEXT("100M"), TEXT("100M"), 5000.0f, 9000.0f, 15000.0f, 22000.0f },
			{ TEXT("200M"), TEXT("300M"), 15000.0f, 22000.0f, 1000000.0f, 1000000.0f },
		};
		USoundNodeDistanceCrossFade* Fade = Many<USoundNodeDistanceCrossFade>(Cue);
		Fade->CrossFadeInput.Reset();
		for (const FBand& Band : Bands)
		{
			USoundWave* Wave = PackAsset<USoundWave>(FString::Printf(TEXT("Fires/Field/%s/%s/Audio/%s_Fire_%s"), Band.Folder, Group, Gun, Band.Suffix));
			if (!Wave)
			{
				continue;
			}
			FDistanceDatum& Datum = Fade->CrossFadeInput.AddDefaulted_GetRef();
			Datum.FadeInDistanceStart = Band.InStart;
			Datum.FadeInDistanceEnd = Band.InEnd;
			Datum.FadeOutDistanceStart = Band.OutStart;
			Datum.FadeOutDistanceEnd = Band.OutEnd;
			Datum.Volume = 1.0f;
			Fade->ChildNodes.Add(Player(Cue, Wave));
		}
		if (Fade->ChildNodes.Num() == 0)
		{
			return nullptr;
		}
		return Finish(Cue, Vary(Cue, Fade, 0.02f));
	}

	/** One of several takes at random. */
	USoundCue* RandomTakes(const FString& Name, const TArray<FString>& Waves)
	{
		USoundCue* Existing;
		USoundCue* Cue = OpenCue(Name, Existing);
		if (!Cue)
		{
			return Existing;
		}
		USoundNodeRandom* Random = Many<USoundNodeRandom>(Cue);
		Random->Weights.Reset();
		for (const FString& Relative : Waves)
		{
			if (USoundWave* Wave = PackAsset<USoundWave>(Relative))
			{
				Random->ChildNodes.Add(Player(Cue, Wave));
				Random->Weights.Add(1.0f);
			}
		}
		if (Random->ChildNodes.Num() == 0)
		{
			return nullptr;
		}
		return Finish(Cue, Vary(Cue, Random));
	}

	/**
	 * The bolt worked, timed to the hand (VaesenBolt shares of the cycle): pulled back as the hand lifts it, pushed
	 * home as the hand comes forward.
	 */
	USoundCue* BoltCycle(const FString& Name, const TCHAR* Gun, float CycleTime)
	{
		USoundCue* Existing;
		USoundCue* Cue = OpenCue(Name, Existing);
		if (!Cue)
		{
			return Existing;
		}
		USoundWave* Back = PackAsset<USoundWave>(FString::Printf(TEXT("Bolts/BoltBack/Dry/SR/Audio/%s_BoltBack"), Gun));
		USoundWave* Home = PackAsset<USoundWave>(FString::Printf(TEXT("Bolts/BoltRelease/Dry/SR/Audio/%s_Bolt_Release"), Gun));
		if (!Back || !Home)
		{
			return nullptr;
		}
		USoundNodeMixer* Mixer = Many<USoundNodeMixer>(Cue);
		Mixer->InputVolume.Reset();
		const TPair<USoundWave*, float> Parts[] = { { Back, CycleTime * VaesenBolt::Reached }, { Home, CycleTime * VaesenBolt::Back } };
		for (const TPair<USoundWave*, float>& Part : Parts)
		{
			USoundNodeDelay* Delay = Cue->ConstructSoundNode<USoundNodeDelay>();
			Delay->DelayMin = Delay->DelayMax = Part.Value;
			Delay->ChildNodes.Add(Player(Cue, Part.Key));
			Mixer->ChildNodes.Add(Delay);
			Mixer->InputVolume.Add(1.0f);
		}
		return Finish(Cue, Vary(Cue, Mixer, 0.02f));
	}

	/** A weapon's sound set (made once, or refilled when rebuilding). */
	UVaesenWeaponSoundSet* SoundSet(const FString& Name, const TMap<EVaesenWeaponSound, USoundBase*>& Sounds)
	{
		const FString Path = FString(Folder) / Name;
		UVaesenWeaponSoundSet* Set = LoadAt<UVaesenWeaponSoundSet>(Path);
		if (Set && !bRebuild)
		{
			return Set;
		}
		if (!Set)
		{
			UPackage* Package = CreatePackage(*Path);
			Set = NewObject<UVaesenWeaponSoundSet>(Package, FName(Name), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Set);
		}
		Set->Sounds.Reset();
		for (const TPair<EVaesenWeaponSound, USoundBase*>& Entry : Sounds)
		{
			if (Entry.Value)
			{
				Set->Sounds.Add(Entry.Key, Entry.Value);
			}
		}
		// The pack's own falloffs: a shot is heard out to 300 m, handling out to 25 m.
		Set->GunshotAttenuation = PackAsset<USoundAttenuation>(TEXT("Attenuations/Field/300M_Field"));
		Set->ShortRangeAttenuation = PackAsset<USoundAttenuation>(TEXT("Attenuations/Field/25M_Field"));
		Save(Set);
		return Set;
	}

	/** Gives a weapon Blueprint its sound set. */
	void Assign(const TCHAR* Weapon, UVaesenWeaponSoundSet* Set)
	{
		UBlueprint* Blueprint = LoadAt<UBlueprint>(FString(WeaponsFolder) / Weapon);
		UVaesenWeaponItem* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<UVaesenWeaponItem>() : nullptr;
		if (!Defaults || !Set)
		{
			UE_LOG(LogVaesenSoundSetup, Warning, TEXT("Couldn't give %s its sounds."), Weapon);
			return;
		}
		if (Defaults->SoundSet == Set)
		{
			return;
		}
		Defaults->SoundSet = Set;
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Save(Blueprint);
	}

	float CycleTimeOf(const TCHAR* Weapon)
	{
		const UBlueprint* Blueprint = LoadAt<UBlueprint>(FString(WeaponsFolder) / Weapon);
		const UVaesenRangedWeaponItem* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject<UVaesenRangedWeaponItem>() : nullptr;
		return Defaults ? Defaults->CycleTime : 0.8f;
	}

	void SetupWeaponVerse()
	{
		if (!LoadAt<USoundAttenuation>(FString(Pack) / TEXT("Attenuations/Field/300M_Field")))
		{
			UE_LOG(LogVaesenSoundSetup, Display, TEXT("Distance Gun Sounds: WeaponVerse1 (/Game/WeaponVerse1) not in the project; skipping weapon sounds."));
			return;
		}

		// Loaded before any weapon Blueprint is compiled (their data check loads it, and with it the anim Blueprints,
		// which mustn't be loaded mid-compile).
		UVaesenWeaponCategoryDefaults::Get();

		// Shared: the crack of a bullet passing, a weapon dropped.
		TArray<FString> Cracks, Drops;
		for (int32 Take = 1; Take <= 6; ++Take)
		{
			Cracks.Add(FString::Printf(TEXT("Service/Bullet_SonicBoom/Audio/Bullet_SonicBoom_%02d"), Take));
		}
		for (int32 Take = 1; Take <= 5; ++Take)
		{
			Drops.Add(FString::Printf(TEXT("Drop/Dry/Audio/WeaponDrop_%02d"), Take));
		}
		USoundCue* Flyby = RandomTakes(TEXT("SC_BulletCrack"), Cracks);
		USoundCue* Drop = RandomTakes(TEXT("SC_WeaponDrop"), Drops);
		USoundBase* RifleDraw = PackAsset<USoundBase>(TEXT("Draws/Dry/SR/Cue/SRM98_Draw_Cue"));
		USoundBase* PistolDraw = PackAsset<USoundBase>(TEXT("Draws/Dry/PS/Cue/PSMP412_Draw_Cue"));

		// Krag-JÃ¸rgensen: the Mauser 1898 (the same era, the same kind of bolt-action rifle).
		const float KragCycle = CycleTimeOf(TEXT("BP_Weapon_KragRifle"));
		UE_LOG(LogVaesenSoundSetup, Display, TEXT("Krag bolt cycle %.2f s: bolt back at %.2f s, home at %.2f s"), KragCycle,
			KragCycle * VaesenBolt::Reached, KragCycle * VaesenBolt::Back);
		Assign(TEXT("BP_Weapon_KragRifle"), SoundSet(TEXT("DA_WeaponSounds_Krag"), {
			{ EVaesenWeaponSound::Fire, DistanceShot(TEXT("SC_Krag_Fire"), TEXT("SR"), TEXT("SRM98")) },
			{ EVaesenWeaponSound::Cycle, BoltCycle(TEXT("SC_Krag_Bolt"), TEXT("SRM98"), KragCycle) },
			{ EVaesenWeaponSound::Draw, RifleDraw },
			{ EVaesenWeaponSound::Sheath, RifleDraw },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));

		// The revolver: the MP412 (a revolver's report).
		Assign(TEXT("BP_Weapon_Revolver"), SoundSet(TEXT("DA_WeaponSounds_Revolver"), {
			{ EVaesenWeaponSound::Fire, DistanceShot(TEXT("SC_Revolver_Fire"), TEXT("PS"), TEXT("PSMP412")) },
			{ EVaesenWeaponSound::Draw, PistolDraw },
			{ EVaesenWeaponSound::Sheath, PistolDraw },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));

		// The rolling-block pistol: a heavy single shot.
		Assign(TEXT("BP_Weapon_RollingBlockPistol"), SoundSet(TEXT("DA_WeaponSounds_RollingBlock"), {
			{ EVaesenWeaponSound::Fire, DistanceShot(TEXT("SC_RollingBlock_Fire"), TEXT("PS"), TEXT("PS44MAGNUM2")) },
			{ EVaesenWeaponSound::Draw, PistolDraw },
			{ EVaesenWeaponSound::Sheath, PistolDraw },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));
	}
}

UVaesenSoundSetupCommandlet::UVaesenSoundSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UVaesenSoundSetupCommandlet::Main(const FString& Params)
{
	VaesenSoundSetup::bRebuild = FParse::Param(*Params, TEXT("rebuild"));
	VaesenSoundSetup::SetupWeaponVerse();
	return 0;
}
