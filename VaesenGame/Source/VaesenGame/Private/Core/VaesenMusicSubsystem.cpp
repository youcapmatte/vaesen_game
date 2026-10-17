#include "Core/VaesenMusicSubsystem.h"

#include "Components/AudioComponent.h"
#include "Core/VaesenSettings.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

bool UVaesenMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UVaesenMusicSubsystem::Deinitialize()
{
	if (Current)
	{
		Current->Stop();
	}
	for (UAudioComponent* Track : FadingOut)
	{
		if (Track)
		{
			Track->Stop();
		}
	}
	Current = nullptr;
	FadingOut.Reset();
	Super::Deinitialize();
}

void UVaesenMusicSubsystem::PlayMusic(USoundBase* Music, float FadeSeconds)
{
	if (!Music)
	{
		StopMusic(FadeSeconds);
		return;
	}

	// Already playing: let it carry on.
	if (Current && Current->Sound == Music && Current->IsPlaying())
	{
		return;
	}

	FadeOutCurrent(FadeSeconds);

	// Persistent: not owned by a world, and not stopped when the map changes. Not auto-destroyed, so it can be
	// faded out and reused; we hold the reference.
	UWorld* World = GetGameInstance()->GetWorld();
	Current = World ? UGameplayStatics::CreateSound2D(World, Music, 1.0f, 1.0f, 0.0f, nullptr,
		/*bPersistAcrossLevelTransition*/ true, /*bAutoDestroy*/ false) : nullptr;
	if (Current)
	{
		Current->FadeIn(FadeSeconds);
	}
}

void UVaesenMusicSubsystem::StopMusic(float FadeSeconds)
{
	FadeOutCurrent(FadeSeconds);
}

void UVaesenMusicSubsystem::BeginLoadingMusic()
{
	if (USoundBase* LoadingMusic = UVaesenSettings::Get()->LoadingMusic.LoadSynchronous())
	{
		PlayMusic(LoadingMusic);
	}
}

void UVaesenMusicSubsystem::EndLoadingMusic(float FadeSeconds)
{
	const USoundBase* LoadingMusic = UVaesenSettings::Get()->LoadingMusic.Get();
	if (LoadingMusic && Current && Current->Sound == LoadingMusic)
	{
		StopMusic(FadeSeconds);
	}
}

void UVaesenMusicSubsystem::FadeOutCurrent(float FadeSeconds)
{
	FadingOut.RemoveAll([](const UAudioComponent* Track) { return !Track || !Track->IsPlaying(); });
	if (Current)
	{
		Current->FadeOut(FadeSeconds, 0.0f);
		FadingOut.Add(Current);
		Current = nullptr;
	}
}
