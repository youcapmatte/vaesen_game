#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VaesenMusicSubsystem.generated.h"

class UAudioComponent;
class USoundBase;

/**
 * Background music that survives map changes. One track plays at a time; starting another crossfades. Asking for the
 * track that's already playing does nothing, so login and character select both ask for the front-end music and it
 * simply carries on between them.
 * While a loading screen is up, UVaesenSettings::LoadingMusic (if set) takes over; it fades out with the loading
 * screen and the new map starts its own music. Clients only.
 */
UCLASS()
class VAESENGAME_API UVaesenMusicSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	/** Plays Music (looping is set on the sound asset). Null stops the music. */
	void PlayMusic(USoundBase* Music, float FadeSeconds = 1.0f);
	void StopMusic(float FadeSeconds = 1.0f);

	/** Called by the loading screen: switches to the loading music, if one is set. */
	void BeginLoadingMusic();

	/** Called by the loading screen when it fades out: fades the loading music out (other music keeps playing). */
	void EndLoadingMusic(float FadeSeconds = 1.0f);

private:
	void FadeOutCurrent(float FadeSeconds);

	/** The track playing now. */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Current;

	/** Tracks fading out; kept referenced until they've stopped. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> FadingOut;
};
