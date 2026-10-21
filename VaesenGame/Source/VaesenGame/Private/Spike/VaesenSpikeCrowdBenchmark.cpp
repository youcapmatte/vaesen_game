// Phase 0 spike (P0-10): how many MetaHumans can be on screen at the gameplay detail levels.
//
// Console:  Vaesen.Spike.Crowd <Count> <CharacterClassPath> [Seconds=30] [ForcedLOD=-1] [Quit=0]
// Count 0 measures the characters already in the level (the class path is then ignored; pass any word, e.g. none).
// Example:  Vaesen.Spike.Crowd 24 /Game/Spike/BP_SpikeMetaHuman.BP_SpikeMetaHuman_C 30 2 1
// Or at startup:  -ExecCmds="Vaesen.Spike.Crowd 24 /Game/... 30 2 1"
//
// Spawns the characters in a grid in front of the first player, waits 5 s for streaming and shaders, then records
// frame, game thread, render thread and GPU times for the given seconds. ForcedLOD >= 0 forces every skeletal mesh
// on the spawned characters to that LOD (as the gameplay minimum-LOD cap would). The result goes to the log
// ("[P0-10]" lines) and to Saved/Spike/CrowdBenchmark.csv; Quit=1 exits afterwards, for unattended runs.

#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "RenderCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogVaesenSpike, Log, All);

namespace VaesenSpikeCrowd
{
	constexpr double WarmupSeconds = 5.0;

	struct FSample
	{
		float FrameMs, GameMs, RenderMs, GpuMs;
	};

	struct FRun
	{
		TWeakObjectPtr<UWorld> World;
		TArray<TWeakObjectPtr<AActor>> Spawned;
		double StartTime = 0.0;
		double Seconds = 30.0;
		bool bQuit = false;
		FString Label;
		TArray<FSample> Samples;
		FTSTicker::FDelegateHandle Ticker;
	};

	TUniquePtr<FRun> Current;

	float Percentile(TArray<float> Values, float P)
	{
		if (Values.IsEmpty())
		{
			return 0.f;
		}
		Values.Sort();
		return Values[FMath::Clamp(FMath::FloorToInt32(P * (Values.Num() - 1)), 0, Values.Num() - 1)];
	}

	void Finish()
	{
		FRun& Run = *Current;
		FTSTicker::GetCoreTicker().RemoveTicker(Run.Ticker);

		TArray<float> Frame, Game, Render, Gpu;
		for (const FSample& S : Run.Samples)
		{
			Frame.Add(S.FrameMs);
			Game.Add(S.GameMs);
			Render.Add(S.RenderMs);
			Gpu.Add(S.GpuMs);
		}
		auto Average = [](const TArray<float>& V) { float Sum = 0.f; for (float X : V) { Sum += X; } return V.IsEmpty() ? 0.f : Sum / V.Num(); };

		const FString Summary = FString::Printf(
			TEXT("%s: %d frames | frame avg %.2f ms (%.1f fps), p95 %.2f ms, max %.2f ms | game avg %.2f p95 %.2f | render avg %.2f p95 %.2f | GPU avg %.2f p95 %.2f"),
			*Run.Label, Run.Samples.Num(),
			Average(Frame), Average(Frame) > 0.f ? 1000.f / Average(Frame) : 0.f, Percentile(Frame, 0.95f), Percentile(Frame, 1.f),
			Average(Game), Percentile(Game, 0.95f), Average(Render), Percentile(Render, 0.95f), Average(Gpu), Percentile(Gpu, 0.95f));
		UE_LOG(LogVaesenSpike, Display, TEXT("[P0-10] %s"), *Summary);

		FString Csv = TEXT("frame_ms,game_ms,render_ms,gpu_ms\n");
		for (const FSample& S : Run.Samples)
		{
			Csv += FString::Printf(TEXT("%.3f,%.3f,%.3f,%.3f\n"), S.FrameMs, S.GameMs, S.RenderMs, S.GpuMs);
		}
		const FString Dir = FPaths::ProjectSavedDir() / TEXT("Spike");
		FFileHelper::SaveStringToFile(Csv, *(Dir / TEXT("CrowdBenchmark.csv")));
		FFileHelper::SaveStringToFile(Summary + LINE_TERMINATOR, *(Dir / TEXT("CrowdBenchmark.txt")), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

		const bool bQuit = Run.bQuit;
		UWorld* World = Run.World.Get();
		Current.Reset();
		if (bQuit && World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->ConsoleCommand(TEXT("quit"));
			}
		}
	}

	bool Tick(float DeltaTime)
	{
		FRun& Run = *Current;
		if (!Run.World.IsValid())
		{
			UE_LOG(LogVaesenSpike, Warning, TEXT("[P0-10] The world went away; benchmark stopped."));
			FTSTicker::GetCoreTicker().RemoveTicker(Run.Ticker);
			Current.Reset();
			return false;
		}

		const double Elapsed = FPlatformTime::Seconds() - Run.StartTime;
		if (Elapsed >= WarmupSeconds)
		{
			Run.Samples.Add({
				DeltaTime * 1000.f,
				(float)FPlatformTime::ToMilliseconds(GGameThreadTime),
				(float)FPlatformTime::ToMilliseconds(GRenderThreadTime),
				(float)FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)),
			});
		}
		if (Elapsed >= WarmupSeconds + Run.Seconds)
		{
			Finish();
			return false;
		}
		return true;
	}

	void Start(const TArray<FString>& Args, UWorld* World)
	{
		if (Current)
		{
			UE_LOG(LogVaesenSpike, Warning, TEXT("[P0-10] A benchmark is already running."));
			return;
		}
		if (Args.Num() < 2 || !World)
		{
			UE_LOG(LogVaesenSpike, Warning, TEXT("[P0-10] Usage: Vaesen.Spike.Crowd <Count> <CharacterClassPath> [Seconds=30] [ForcedLOD=-1] [Quit=0]"));
			return;
		}

		const int32 Count = FMath::Clamp(FCString::Atoi(*Args[0]), 0, 200);
		UClass* Class = Count > 0 ? LoadClass<AActor>(nullptr, *Args[1]) : AActor::StaticClass();
		if (!Class)
		{
			UE_LOG(LogVaesenSpike, Error, TEXT("[P0-10] Class not found: %s"), *Args[1]);
			return;
		}

		Current = MakeUnique<FRun>();
		FRun& Run = *Current;
		Run.World = World;
		Run.Seconds = Args.Num() > 2 ? FMath::Max(1.0, FCString::Atod(*Args[2])) : 30.0;
		const int32 ForcedLOD = Args.Num() > 3 ? FCString::Atoi(*Args[3]) : -1;
		Run.bQuit = Args.Num() > 4 && FCString::Atoi(*Args[4]) != 0;
		Run.Label = FString::Printf(TEXT("%s, LOD %s"), Count > 0 ? *FString::Printf(TEXT("%d x %s"), Count, *Class->GetName()) : TEXT("placed characters"), ForcedLOD >= 0 ? *FString::FromInt(ForcedLOD) : TEXT("auto"));

		// A grid in front of the first player, 1.5 m apart, so they're all in view.
		FVector Origin = FVector::ZeroVector;
		FRotator Facing = FRotator::ZeroRotator;
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			Facing = FRotator(0.f, Player->GetActorRotation().Yaw, 0.f);
			Origin = Player->GetActorLocation() + Facing.Vector() * 400.f;
		}
		const int32 Columns = FMath::CeilToInt32(FMath::Sqrt((float)Count));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		// The characters to measure: spawned ones, or (Count 0) every character already in the level.
		TArray<AActor*> Targets;
		if (Count == 0)
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->FindComponentByClass<ULODSyncComponent>() || Cast<ACharacter>(*It))
				{
					Targets.Add(*It);
				}
			}
		}
		for (int32 I = 0; I < Count; ++I)
		{
			const FVector Local((I / Columns) * 150.f, ((I % Columns) - (Columns - 1) * 0.5f) * 150.f, 0.f);
			// Face back toward the player.
			const FRotator Rotation = Facing + FRotator(0.f, 180.f, 0.f);
			if (AActor* Actor = World->SpawnActor<AActor>(Class, Origin + Facing.RotateVector(Local), Rotation, Params))
			{
				Targets.Add(Actor);
				Run.Spawned.Add(Actor);
			}
		}
		if (ForcedLOD >= 0)
		{
			for (AActor* Actor : Targets)
			{
				// A MetaHuman's LOD Sync component decides every part's LOD (face, body, hair); force it there.
				if (ULODSyncComponent* LODSync = Actor->FindComponentByClass<ULODSyncComponent>())
				{
					LODSync->ForcedLOD = ForcedLOD;
				}
				TArray<USkeletalMeshComponent*> Meshes;
				Actor->GetComponents(Meshes);
				for (USkeletalMeshComponent* Mesh : Meshes)
				{
					Mesh->SetForcedLOD(ForcedLOD + 1); // SetForcedLOD is 1-based; 0 means automatic.
				}
			}
		}

		UE_LOG(LogVaesenSpike, Display, TEXT("[P0-10] Started: %s (%d characters), %.0f s after %.0f s warm-up."), *Run.Label, Targets.Num(), Run.Seconds, WarmupSeconds);
		Run.StartTime = FPlatformTime::Seconds();
		Run.Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
	}

	FAutoConsoleCommandWithWorldAndArgs Command(
		TEXT("Vaesen.Spike.Crowd"),
		TEXT("Phase 0 spike (P0-10): Vaesen.Spike.Crowd <Count> <CharacterClassPath> [Seconds=30] [ForcedLOD=-1] [Quit=0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}
