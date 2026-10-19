#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "VaesenSettings.generated.h"

class USoundBase;
class UTexture2D;
class UUserWidget;
class UVaesenInteractionCardWidget;
class UVaesenInventoryScreenWidget;
class UVaesenHUDWidget;
class UVaesenLoadingScreenWidget;
class UVaesenUIStyle;

/** How a map looks on the loading screen. */
USTRUCT()
struct FVaesenMapLoadingInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Loading", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> Map;

	/** Shown on the loading screen, e.g. "Gyllencreutz". Empty = the map's asset name without "L_". */
	UPROPERTY(EditAnywhere, Category = "Loading")
	FText DisplayName;

	/** Empty = a random one of the default loading backgrounds. */
	UPROPERTY(EditAnywhere, Category = "Loading")
	TSoftObjectPtr<UTexture2D> Background;
};

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

	/** Each guild's base (its own instance of this map); where a character enters the world unless it can return to where it just was. */
	UPROPERTY(Config, EditAnywhere, Category = "Maps", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> GuildHubMap;

	/** Widget shown on the login map, e.g. WBP_Login (parent class VaesenLoginWidget). */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> LoginScreenWidget;

	/** Widget shown on the character select map. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UUserWidget> CharacterSelectScreenWidget;

	/** Look of the Vaesen controls (buttons, text, text boxes, combo boxes), e.g. DA_VaesenUIStyle. Empty = built-in defaults. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftObjectPtr<UVaesenUIStyle> UIStyle;

	/** Card shown at an interactable the player looks at, e.g. WBP_InteractionCard (parent class VaesenInteractionCardWidget). An interactable can pick its own instead. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UVaesenInteractionCardWidget> InteractionCardWidget;

	/** The inventory screen (player inventory, plus a container being looted), e.g. WBP_InventoryScreen. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UVaesenInventoryScreenWidget> InventoryScreenWidget;

	/** The in-game HUD all gameplay UI lives on (minimap, quick bar...), e.g. WBP_HUD. Empty = none. */
	UPROPERTY(Config, EditAnywhere, Category = "UI")
	TSoftClassPtr<UVaesenHUDWidget> HUDWidget;

	/** Icons for the status badges under the health bar (Status.* tags); one without shows its name's first letters. */
	UPROPERTY(Config, EditAnywhere, Category = "UI", meta = (Categories = "Status"))
	TMap<FGameplayTag, TSoftObjectPtr<UTexture2D>> StatusIcons;

	/** Front-end music: starts on the login screen and carries on through character select. Leave empty for silence. Looping is set on the sound asset. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundBase> LoginMusic;

	/** Played while the loading screen is up; fades out with it. Leave empty to keep the current music playing. Looping is set on the sound asset. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundBase> LoadingMusic;

	/** First names the "Randomize" button on character creation picks from. */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<FString> RandomFirstNames;

	/** Last names the "Randomize" button on character creation picks from (independently of the first name). */
	UPROPERTY(Config, EditAnywhere, Category = "Characters")
	TArray<FString> RandomLastNames;

	/** Shown while traveling between maps, e.g. WBP_LoadingScreen (parent class VaesenLoadingScreenWidget). Empty = no loading screen. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TSoftClassPtr<UVaesenLoadingScreenWidget> LoadingScreenWidget;

	/** One is picked at random for each load. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (MultiLine = "true"))
	TArray<FText> LoadingTips;

	/** Used for maps without their own background; one is picked at random. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TArray<TSoftObjectPtr<UTexture2D>> LoadingBackgrounds;

	/** Per-map name and background. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen")
	TArray<FVaesenMapLoadingInfo> MapLoadingInfo;

	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenFadeInSeconds = 0.3f;

	/** The loading screen fades out to reveal the new map. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenFadeOutSeconds = 0.8f;

	/** Never shown shorter than this, so a fast load doesn't flash. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenMinimumSeconds = 1.0f;

	/** Kept up this long after the map is ready, so textures can stream in before the reveal. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "0", Units = "s"))
	float LoadingScreenSettleSeconds = 0.5f;

	/** Give up waiting for the map to be ready (streaming, your character, shaders) after this long. */
	UPROPERTY(Config, EditAnywhere, Category = "Loading Screen", meta = (ClampMin = "1", Units = "s"))
	float LoadingScreenTimeoutSeconds = 30.0f;

	/** Played to a player when they put something on, unless the item has its own Equip Sound. Leave empty for none. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Items")
	TSoftObjectPtr<USoundBase> DefaultEquipSound;

	/** Played to a player when something can't be put on (wrong slot, no room), unless the item has its own. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Items")
	TSoftObjectPtr<USoundBase> DefaultEquipRejectedSound;

	/** Played to a player when they drop something on the ground. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio|Items")
	TSoftObjectPtr<USoundBase> DropSound;

	/** Played when the login button is pressed (or Enter in the password box). Leave empty for none. */
	UPROPERTY(Config, EditAnywhere, Category = "Audio")
	TSoftObjectPtr<USoundBase> LoginButtonSound;
};
