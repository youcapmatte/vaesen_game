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
#include "Combat/VaesenGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenSoundSetup, Log, All);

namespace VaesenSoundSetup
{
	const TCHAR* Pack = TEXT("/Game/WeaponVerse1/Source");
	const TCHAR* Folder = TEXT("/Game/Vaesen/Audio/Weapons");
	const TCHAR* WeaponsFolder = TEXT("/Game/Vaesen/Blueprints/Items/Weapons");

	bool bRebuild = false;

	/**
	 * A pack's asset where the project keeps it: what the project uses of a pack lives in /Game/Vaesen/ThirdParty/<pack>
	 * (the pack itself isn't in git; -run=VaesenContentAudit moves it there). Else in the pack, if it's installed.
	 */
	FString Resolve(const FString& PackagePath)
	{
		if (!PackagePath.StartsWith(TEXT("/Game/")) || PackagePath.StartsWith(TEXT("/Game/Vaesen/")))
		{
			return PackagePath;
		}
		FString Rest = PackagePath.Mid(6);
		Rest.RemoveFromStart(TEXT("ThirdParty/"));
		const FString Kept = FString(TEXT("/Game/Vaesen/ThirdParty/")) + Rest;
		return FPackageName::DoesPackageExist(Kept) ? Kept : PackagePath;
	}

	template <typename T>
	T* LoadAt(const FString& PackagePath)
	{
		const FString Path = Resolve(PackagePath);
		return LoadObject<T>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)));
	}

	/** In WeaponVerse1 (relative), or anywhere (a /Game/ path). */
	template <typename T>
	T* PackAsset(const FString& Relative)
	{
		T* Asset = LoadAt<T>(Relative.StartsWith(TEXT("/Game/")) ? Relative : FString(Pack) / Relative);
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

	/**
	 * A slight random pitch and loudness on top, so the same sound twice doesn't sound copied; Centre shifts the whole
	 * pitch (below 1: deeper and slower).
	 */
	USoundNode* Vary(USoundCue* Cue, USoundNode* Child, float Pitch = 0.03f, float Centre = 1.0f)
	{
		USoundNodeModulator* Modulator = Cue->ConstructSoundNode<USoundNodeModulator>();
		Modulator->PitchMin = Centre - Pitch;
		Modulator->PitchMax = Centre + Pitch;
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
	 * the next (cm). Recordings missing from the pack are left out. Pitch below 1 makes it deeper.
	 */
	USoundNode* DistanceFade(USoundCue* Cue, const TCHAR* Group, const TCHAR* Gun);

	USoundCue* DistanceShot(const FString& Name, const TCHAR* Group, const TCHAR* Gun, float Pitch = 1.0f)
	{
		USoundCue* Existing;
		USoundCue* Cue = OpenCue(Name, Existing);
		if (!Cue)
		{
			return Existing;
		}
		USoundNode* Fade = DistanceFade(Cue, Group, Gun);
		return Fade ? Finish(Cue, Vary(Cue, Fade, 0.02f, Pitch)) : nullptr;
	}

	/** One of several guns' shots at random (two recordings of a revolver), each heard as it is at the listener's distance. */
	USoundCue* DistanceShots(const FString& Name, const TCHAR* Group, const TArray<const TCHAR*>& Guns)
	{
		USoundCue* Existing;
		USoundCue* Cue = OpenCue(Name, Existing);
		if (!Cue)
		{
			return Existing;
		}
		USoundNodeRandom* Random = Many<USoundNodeRandom>(Cue);
		Random->Weights.Reset();
		for (const TCHAR* Gun : Guns)
		{
			if (USoundNode* Fade = DistanceFade(Cue, Group, Gun))
			{
				Random->ChildNodes.Add(Fade);
				Random->Weights.Add(1.0f);
			}
		}
		return Random->ChildNodes.Num() > 0 ? Finish(Cue, Vary(Cue, Random, 0.02f)) : nullptr;
	}

	/** A gun's shot recorded at 5, 25, 100 and 300 m, each crossfading into the next by the listener's distance (null if none is in the pack). */
	USoundNode* DistanceFade(USoundCue* Cue, const TCHAR* Group, const TCHAR* Gun)
	{
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
		return Fade->ChildNodes.Num() > 0 ? Fade : nullptr;
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
		// (Without WeaponVerse1, each sound's own.)
		Set->GunshotAttenuation = LoadAt<USoundAttenuation>(FString(Pack) / TEXT("Attenuations/Field/300M_Field"));
		Set->ShortRangeAttenuation = LoadAt<USoundAttenuation>(FString(Pack) / TEXT("Attenuations/Field/25M_Field"));
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

		// The revolver: the pack's own revolver (two recordings of its shot, one at random), drawn from the holster, the
		// cylinder opened and closed, a round clicked in, the hammer cocked and falling on nothing.
		const auto Take = [](const TCHAR* Name, const TCHAR* Wave) { return RandomTakes(Name, { Wave }); };
		USoundCue* RevolverClick = Take(TEXT("SC_Revolver_Click"), TEXT("Bolts/BoltRelease/Dry/PS/Audio/PSREVOLVER_Bolt_Release"));
		USoundCue* RevolverDraw = Take(TEXT("SC_Revolver_Draw"), TEXT("Draws/Dry/PS/Audio/PSREVOLVER_Draw"));
		USoundCue* RevolverCock = Take(TEXT("SC_Revolver_Cock"), TEXT("Bolts/BoltBack/Dry/PS/Audio/PSREVOLVER_BoltBack"));
		Assign(TEXT("BP_Weapon_Revolver"), SoundSet(TEXT("DA_WeaponSounds_Revolver"), {
			{ EVaesenWeaponSound::Fire, DistanceShots(TEXT("SC_Revolver_Fire"), TEXT("PS"), { TEXT("PSREVOLVER1"), TEXT("PSREVOLVER2") }) },
			{ EVaesenWeaponSound::Draw, RevolverDraw },
			{ EVaesenWeaponSound::Sheath, RevolverDraw },
			{ EVaesenWeaponSound::ReloadOpen, Take(TEXT("SC_Revolver_CylinderOpen"), TEXT("Magazines/Magazine_Out/Dry/PS/Audio/PSREVOLVER_Manazine_Out")) },
			{ EVaesenWeaponSound::ReloadInsert, RevolverClick },
			{ EVaesenWeaponSound::ReloadClose, Take(TEXT("SC_Revolver_CylinderClose"), TEXT("Magazines/Magazine_In/Dry/PS/Audio/PSREVOLVER_Magazine_In")) },
			{ EVaesenWeaponSound::HammerCock, RevolverCock },
			{ EVaesenWeaponSound::DryFire, RevolverClick },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));

		// The rolling-block pistol: a heavy single shot; its breech opened, a cartridge in and the breech closed (the
		// pack's M1911 handling, the nearest to a pistol's breech), the hammer as the revolver's.
		Assign(TEXT("BP_Weapon_RollingBlockPistol"), SoundSet(TEXT("DA_WeaponSounds_RollingBlock"), {
			{ EVaesenWeaponSound::Fire, DistanceShot(TEXT("SC_RollingBlock_Fire"), TEXT("PS"), TEXT("PS44MAGNUM2")) },
			{ EVaesenWeaponSound::Draw, PistolDraw },
			{ EVaesenWeaponSound::Sheath, PistolDraw },
			{ EVaesenWeaponSound::ReloadOpen, Take(TEXT("SC_RollingBlock_BreechOpen"), TEXT("Bolts/BoltBack/Dry/PS/Audio/PSM1911_BoltBack")) },
			{ EVaesenWeaponSound::ReloadInsert, Take(TEXT("SC_RollingBlock_Insert"), TEXT("Magazines/Magazine_In/Dry/PS/Audio/PSM1911_Magazine_In")) },
			{ EVaesenWeaponSound::ReloadClose, Take(TEXT("SC_RollingBlock_BreechClose"), TEXT("Bolts/BoltRelease/Dry/PS/Audio/PSM1911_Bolt_Release")) },
			{ EVaesenWeaponSound::HammerCock, RevolverCock },
			{ EVaesenWeaponSound::DryFire, RevolverClick },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));

		// The muzzle-loader, until there's a black-powder pack: a pump shotgun's boom (the pack's nearest to a
		// smoothbore) pitched down, deeper and slower, like black powder.
		Assign(TEXT("BP_Weapon_MuzzleLoader"), SoundSet(TEXT("DA_WeaponSounds_MuzzleLoader"), {
			{ EVaesenWeaponSound::Fire, DistanceShot(TEXT("SC_MuzzleLoader_Fire"), TEXT("SG"), TEXT("SGR870"), 0.86f) },
			{ EVaesenWeaponSound::Draw, RifleDraw },
			{ EVaesenWeaponSound::Sheath, RifleDraw },
			{ EVaesenWeaponSound::DropLand, Drop },
			{ EVaesenWeaponSound::Flyby, Flyby },
		}));
	}

	// --- Combat Sounds - Bundle Collection (Audios With Love): blades and hits ---

	const TCHAR* CombatPack = TEXT("/Game/Combat_SFX_Bundle/WAV");

	/** Takes Name_01..Name_Count in a folder of the combat pack (Count 0: just Name). */
	TArray<FString> Takes(const TCHAR* Group, const TCHAR* Name, int32 Count)
	{
		TArray<FString> Paths;
		if (Count == 0)
		{
			Paths.Add(FString::Printf(TEXT("%s/%s/%s"), CombatPack, Group, Name));
		}
		for (int32 Take = 1; Take <= Count; ++Take)
		{
			Paths.Add(FString::Printf(TEXT("%s/%s/%s_%02d"), CombatPack, Group, Name, Take));
		}
		return Paths;
	}

	/** Where a recording starts to be heard (above -30 dB of its peak) and where it peaks, in seconds. */
	bool MeasureOnset(const USoundWave* Wave, float& OutOnset, float& OutPeak)
	{
		TArray<uint8> Raw;
		uint32 SampleRate = 0;
		uint16 Channels = 0;
		if (!Wave || !Wave->GetImportedSoundWaveData(Raw, SampleRate, Channels) || SampleRate == 0 || Channels == 0)
		{
			return false;
		}
		const int16* Samples = reinterpret_cast<const int16*>(Raw.GetData());
		const int32 Frames = Raw.Num() / (2 * Channels);
		int32 PeakFrame = 0;
		int32 PeakValue = 0;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			const int32 Value = FMath::Abs(static_cast<int32>(Samples[Frame * Channels]));
			if (Value > PeakValue)
			{
				PeakValue = Value;
				PeakFrame = Frame;
			}
		}
		int32 Onset = 0;
		while (Onset < Frames && FMath::Abs(static_cast<int32>(Samples[Onset * Channels])) < PeakValue / 32)
		{
			++Onset;
		}
		OutOnset = static_cast<float>(Onset) / SampleRate;
		OutPeak = static_cast<float>(PeakFrame) / SampleRate;
		return true;
	}

	void AnalyzeCombatPack()
	{
		for (const TCHAR* Group : { TEXT("Sword_Hits_Blocks"), TEXT("Combat_Sounds_Weapons_And_Spells"), TEXT("Blood") })
		{
			IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
			TArray<FAssetData> Found;
			Registry.GetAssetsByPath(FName(FString(CombatPack) / Group), Found, false);
			Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
			for (const FAssetData& Data : Found)
			{
				const FString Name = Data.AssetName.ToString();
				if (!Name.Contains(TEXT("Whoosh")) && !Name.Contains(TEXT("Take")) && !Name.Contains(TEXT("Pack_Away")) && !Name.Contains(TEXT("VS_Sword"))
					&& !Name.Contains(TEXT("Parry")) && !Name.Contains(TEXT("Impact")) && !Name.Contains(TEXT("Hit_Then_Blood")) && !Name.Contains(TEXT("Pierce")) && !Name.StartsWith(TEXT("Stab_0")) && !Name.Contains(TEXT("Sword_Stab")))
				{
					continue;
				}
				float Onset = 0.0f, Peak = 0.0f;
				const USoundWave* Wave = Cast<USoundWave>(Data.GetAsset());
				if (MeasureOnset(Wave, Onset, Peak))
				{
					UE_LOG(LogVaesenSoundSetup, Display, TEXT("%-40s %.2f s long, heard from %.2f s, peaks at %.2f s"), *Name, Wave->Duration, Onset, Peak);
				}
			}
		}
	}

	/** Just the take that peaks soonest (some build up slowly; the character draws and sheathes quickly). */
	TArray<FString> Quickest(const TArray<FString>& Paths)
	{
		FString Best;
		float BestPeak = TNumericLimits<float>::Max();
		for (const FString& Path : Paths)
		{
			float Onset = 0.0f, Peak = 0.0f;
			if (MeasureOnset(LoadAt<USoundWave>(Path), Onset, Peak) && Peak < BestPeak)
			{
				Best = Path;
				BestPeak = Peak;
			}
		}
		UE_LOG(LogVaesenSoundSetup, Display, TEXT("Quickest take: %s (peaks at %.2f s)"), *FPackageName::GetShortName(Best), BestPeak);
		return Best.IsEmpty() ? TArray<FString>() : TArray<FString>{ Best };
	}

	/** Only the takes that hit at once (peak within MaxPeak): an impact must land with the blow, not after it. */
	TArray<FString> Snappy(const TArray<FString>& Paths, float MaxPeak = 0.12f)
	{
		TArray<FString> Kept;
		for (const FString& Path : Paths)
		{
			float Onset = 0.0f, Peak = 0.0f;
			if (MeasureOnset(LoadAt<USoundWave>(Path), Onset, Peak) && Peak <= MaxPeak)
			{
				Kept.Add(Path);
			}
			else
			{
				UE_LOG(LogVaesenSoundSetup, Display, TEXT("Left out (peaks late, at %.2f s): %s"), Peak, *FPackageName::GetShortName(Path));
			}
		}
		return Kept;
	}

	TArray<FString> operator+(TArray<FString> A, const TArray<FString>& B)
	{
		A.Append(B);
		return A;
	}

	void SetupCombatPack()
	{
		if (!LoadAt<USoundWave>(Takes(TEXT("Sword_Hits_Blocks"), TEXT("Take_Out_Sword"), 1)[0]))
		{
			UE_LOG(LogVaesenSoundSetup, Display, TEXT("Combat Sounds - Bundle Collection (/Game/Combat_SFX_Bundle) not in the project; skipping blade and hit sounds."));
			return;
		}
		UVaesenWeaponCategoryDefaults* Defaults = LoadObject<UVaesenWeaponCategoryDefaults>(nullptr, TEXT("/Game/Vaesen/Combat/DA_WeaponCategoryDefaults.DA_WeaponCategoryDefaults"));
		if (!Defaults)
		{
			return;
		}
		const TCHAR* Blades = TEXT("Sword_Hits_Blocks");
		const TCHAR* Weapons = TEXT("Combat_Sounds_Weapons_And_Spells");
		const TCHAR* Blood = TEXT("Blood");

		// The sword's own: drawn and put away (one quick take each; the montages time them), the swing (light: a quick
		// whoosh; heavy: a big one), steel meeting steel on the guard, and the ring of a parry. Every sword (the
		// category's set).
		UVaesenWeaponSoundSet* Sword = SoundSet(TEXT("DA_WeaponSounds_Sword"), {
			{ EVaesenWeaponSound::Draw, RandomTakes(TEXT("SC_Sword_Draw"), Quickest(Takes(Blades, TEXT("Take_Out_Sword"), 4))) },
			{ EVaesenWeaponSound::Sheath, RandomTakes(TEXT("SC_Sword_Sheath"), Quickest(Takes(Blades, TEXT("Pack_Away_Sword"), 3))) },
			{ EVaesenWeaponSound::SwingLight, RandomTakes(TEXT("SC_Sword_SwingLight"),
				Takes(Blades, TEXT("Whoosh_Standard_Sword"), 3) + Takes(Weapons, TEXT("Whoosh_Sword"), 2) + Takes(Weapons, TEXT("Whoosh"), 7)) },
			{ EVaesenWeaponSound::SwingHeavy, RandomTakes(TEXT("SC_Sword_SwingHeavy"),
				Takes(Blades, TEXT("Whoosh_Big_Sword"), 4) + Takes(Weapons, TEXT("BigSword_Whoosh"), 2)) },
			{ EVaesenWeaponSound::Block, RandomTakes(TEXT("SC_Sword_Block"), Snappy(Takes(Blades, TEXT("Sword_VS_Sword_Hit"), 10) + Takes(Blades, TEXT("Impact_On_Other_Sword"), 3))) },
			{ EVaesenWeaponSound::Parry, RandomTakes(TEXT("SC_Sword_Parry"), Snappy(Takes(Weapons, TEXT("Impact_Sword_Parry"), 5) + Takes(Blades, TEXT("Special_Crit_Sword_VS_Sword"), 3))) },
		});
		FVaesenCategoryDefaults& SwordCategory = Defaults->Categories.FindOrAdd(EVaesenWeaponCategory::OneHandedSword);
		if (Sword && SwordCategory.Sounds != Sword)
		{
			SwordCategory.Sounds = Sword;
			Save(Defaults);
		}

		// The impact table, by hand (blades, clubs, fists): what the blow lands on. Flesh is also each type's
		// any-surface row (a body with no material of its own). Shots keep their own rows.
		UVaesenImpactEffects* Table = Defaults->ImpactEffects;
		if (!Table)
		{
			return;
		}
		// Only takes that land with the blow (Snappy): the slow "hit, then blood" and drawn-out stabs peak too late.
		USoundBase* SlashFlesh = RandomTakes(TEXT("SC_Hit_Slash_Flesh"), Snappy(Takes(Weapons, TEXT("Impact_Sword_To_Body"), 5) + Takes(Blood, TEXT("Hit_Then_Blood"), 5)));
		USoundBase* StabFlesh = RandomTakes(TEXT("SC_Hit_Stab_Flesh"), Snappy(Takes(Blades, TEXT("Pierce_Enemy"), 10) + Takes(Blood, TEXT("Sword_Stab_With_Blood"), 2)
			+ Takes(Blood, TEXT("Stab"), 3)));
		USoundBase* BluntFlesh = RandomTakes(TEXT("SC_Hit_Blunt_Flesh"), Snappy(Takes(Weapons, TEXT("Impact_Punch"), 5) + Takes(Weapons, TEXT("Impact_Punch_Soft"), 4)
			+ Takes(Weapons, TEXT("Impact_Punch_With_Blood"), 7)));
		USoundBase* BladeWood = RandomTakes(TEXT("SC_Hit_Blade_Wood"), Snappy(Takes(Blades, TEXT("Impact_Sword_Wood"), 6) + Takes(Weapons, TEXT("Impact_Sword_To_Wood"), 4)));
		USoundBase* BladeStone = RandomTakes(TEXT("SC_Hit_Blade_Stone"), Snappy(Takes(Weapons, TEXT("Impact_Sword_To_Stone"), 4)));
		USoundBase* BladeMetal = RandomTakes(TEXT("SC_Hit_Blade_Metal"), Snappy(Takes(Weapons, TEXT("Impact_Sword_To_PlateArmour"), 5)
			+ Takes(Weapons, TEXT("Impact_Sword_To_Armour_Chain"), 5) + Takes(Blades, TEXT("Impact_Armour"), 2)));
		USoundBase* BluntHard = RandomTakes(TEXT("SC_Hit_Blunt_Hard"), Snappy(Takes(Weapons, TEXT("BigHammer_Impact"), 3)));
		USoundBase* BluntMetal = RandomTakes(TEXT("SC_Hit_Blunt_Metal"), Snappy(Takes(Blades, TEXT("Impact_Metal"), 6)));

		// Ours from an earlier run go; the rest of the table stays.
		const FString Ours = FString(Folder) / TEXT("SC_Hit_");
		Table->Entries.RemoveAll([&Ours](const FVaesenImpactEntry& Entry)
		{
			return Entry.Sound && Entry.Sound->GetPathName().StartsWith(Ours);
		});
		const auto Row = [Table](const FGameplayTag& Type, EPhysicalSurface Surface, USoundBase* Sound)
		{
			if (Sound)
			{
				FVaesenImpactEntry& Entry = Table->Entries.AddDefaulted_GetRef();
				Entry.DamageType = Type;
				Entry.Surface = Surface;
				Entry.Delivery = EVaesenImpactDelivery::ByHand;
				Entry.Sound = Sound;
			}
		};
		// Flesh, Wood, Stone, Metal, Cloth (Config/DefaultEngine.ini).
		const EPhysicalSurface Flesh = SurfaceType1, Wood = SurfaceType2, Stone = SurfaceType3, Metal = SurfaceType4, Cloth = SurfaceType7;
		const TPair<FGameplayTag, USoundBase*> OnBodies[] = {
			{ VaesenTags::Damage_Slashing, SlashFlesh }, { VaesenTags::Damage_Piercing, StabFlesh }, { VaesenTags::Damage_Blunt, BluntFlesh } };
		for (const TPair<FGameplayTag, USoundBase*>& Body : OnBodies)
		{
			for (const EPhysicalSurface Surface : { SurfaceType_Default, Flesh, Cloth })
			{
				Row(Body.Key, Surface, Body.Value);
			}
		}
		for (const FGameplayTag& Edge : TArray<FGameplayTag>{ VaesenTags::Damage_Slashing, VaesenTags::Damage_Piercing })
		{
			Row(Edge, Wood, BladeWood);
			Row(Edge, Stone, BladeStone);
			Row(Edge, Metal, BladeMetal);
		}
		Row(VaesenTags::Damage_Blunt, Wood, BluntHard);
		Row(VaesenTags::Damage_Blunt, Stone, BluntHard);
		Row(VaesenTags::Damage_Blunt, Metal, BluntMetal);
		Save(Table);
		UE_LOG(LogVaesenSoundSetup, Display, TEXT("Impact table: %d rows (blades, clubs and fists by hand from the combat pack)."), Table->Entries.Num());
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
	if (FParse::Param(*Params, TEXT("analyze")))
	{
		VaesenSoundSetup::AnalyzeCombatPack();
		return 0;
	}
	VaesenSoundSetup::SetupWeaponVerse();
	VaesenSoundSetup::SetupCombatPack();
	return 0;
}
