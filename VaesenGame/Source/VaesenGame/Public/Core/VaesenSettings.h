#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VaesenSettings.generated.h"

class USoundBase;
class UUserWidget;
class UVaesenUIStyle;

/**
 * Project-wide default values. Edit in Project Settings > Game > Vaesen; saved to Config/DefaultGame.ini.
 * Read anywhere with UVaesenSettings::Get().
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Vaesen"))
class VAESENGAME_API UVaesenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVaesenSettings();

	static const UVaesenSettings* Get() { return GetDefault<UVaesenSettings>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Base URL of Vaesen.Api, e.g. "http://localhost:5230" in development. */
	UPROPERTY(Config, EditAnywhere, Category = "API")
	FString ApiBaseUrl;

	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> LoginMap;

	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> CharacterSelectMap;

	/** Widget shown on the login map, e.g. WBP_Login (parent class VaesenLoginWidget). */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> LoginScreenWidget;

	/** Widget shown on the character select map. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> CharacterSelectScreenWidget;

	/** Look of the Vaesen controls (buttons, text, text boxes, combo boxes), e.g. DA_VaesenUIStyle. Empty = built-in defaults. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftObjectPtr<UVaesenUIStyle> UIStyle;

	/** Background music played when the login map starts. Leave empty for silence. Looping is set on the sound asset. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundBase> LoginMusic;

	/** First names the "Randomize" button on character creation picks from. */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<FString> RandomFirstNames;

	/** Last names the "Randomize" button on character creation picks from (independently of the first name). */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<FString> RandomLastNames;

	/** Played when the login button is pressed (or Enter in the password box). Leave empty for none. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundBase> LoginButtonSound;
};
