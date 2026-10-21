#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Appearance/VaesenAppearance.h"
#include "Appearance/VaesenAppearancePresets.h"
#include "Online/VaesenApiTypes.h"
#include "VaesenCharacterCreator.generated.h"

class UDataTable;
class UVaesenApiSubsystem;

/** The character creator's steps, in order. */
UENUM(BlueprintType)
enum class EVaesenCreatorStep : uint8
{
	/** Sex and class. */
	Basics,
	/** Body type and skin tone. */
	BodyAndSkin,
	/** Base face, sculpt sliders, age. */
	Face,
	/** Eye colour, hairstyle, hair colour, eyebrows, facial hair. */
	EyesAndHair,
	/** First and last name, review, Create. */
	NameAndReview,
};

/** Which camera framing a step uses. */
UENUM(BlueprintType)
enum class EVaesenCreatorFraming : uint8
{
	Body,
	Face,
};

/** What changing the sex needs. */
UENUM(BlueprintType)
enum class EVaesenSexChange : uint8
{
	/** Done. */
	Changed,
	/** Nothing to do: already that sex. */
	Unchanged,
	/** The body or face steps were edited and would be reset: ask the player, then call again with bConfirmed. */
	NeedsConfirmation,
	/** Not a sex the presets offer. */
	Invalid,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FVaesenCreatorChangedSignature);

/**
 * The character creator's state and rules, apart from its widgets: the step, the appearance being made, the name and
 * class, and what each step allows (the player-selectable presets of DT_AppearancePresets for the chosen sex). The
 * stepper widgets show it and call it; the preview MetaHuman's appearance component applies GetAppearance() on every
 * OnChanged. Create sends the result to the API.
 */
UCLASS(BlueprintType)
class VAESENGAME_API UVaesenCharacterCreator : public UObject
{
	GENERATED_BODY()

public:
	/** Starts over: the first step, the given sex with its first presets, no name. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void Begin(UDataTable* InPresets, FName InSex);

	/** Broadcast after any change (step, appearance, name, class). */
	UPROPERTY(BlueprintAssignable, Category = "Character Creator")
	FVaesenCreatorChangedSignature OnChanged;

	// Steps.

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	EVaesenCreatorStep GetStep() const { return Step; }

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	EVaesenCreatorFraming GetFraming() const;

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	bool CanGoBack() const { return Step != EVaesenCreatorStep::Basics; }

	/** Whether the current step is complete enough to move on (Basics: a class; the last step has no next). */
	UFUNCTION(BlueprintPure, Category = "Character Creator")
	bool CanGoNext() const;

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	bool Next();

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	bool Back();

	// Basics.

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	EVaesenSexChange SetSex(FName InSex, bool bConfirmed);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetClass(const FString& InClass);

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	const FString& GetClass() const { return Class; }

	// Appearance.

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	const FVaesenAppearance& GetAppearance() const { return Appearance; }

	/** The IDs the player may pick of one kind, for the current sex (sliders: the slider names). */
	UFUNCTION(BlueprintPure, Category = "Character Creator")
	TArray<FName> GetOptions(EVaesenAppearancePresetKind Kind) const;

	/** Picks a preset (body, face, hairstyle, eyebrows, facial hair, eye colour; none clears facial hair). False if not allowed. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	bool SetPreset(EVaesenAppearancePresetKind Kind, FName Id);

	/** Applies a skin tone or hair colour swatch's values. False if it isn't one. */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	bool ApplySwatch(FName SwatchId);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetSkinTone(FVector2f InTone);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	bool SetSlider(FName Slider, float Value);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetAge(float InAge);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetHairColor(float Melanin, float Redness, float Dye);

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetHairGrey(float Grey);

	/**
	 * The Face step's Randomise: a random base face, small slider offsets and a random skin, eye and hair combination
	 * from the allowed palettes. Keeps the sex, the body and the name. The same seed gives the same result.
	 */
	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void Randomise(int32 Seed);

	// Name and Create.

	UFUNCTION(BlueprintCallable, Category = "Character Creator")
	void SetName(const FString& InFirstName, const FString& InLastName);

	UFUNCTION(BlueprintPure, Category = "Character Creator")
	FString GetCharacterName() const;

	/** The same rule as the API: 1-32 letters, with single spaces, apostrophes or hyphens between words. */
	UFUNCTION(BlueprintPure, Category = "Character Creator")
	static bool IsValidPersonName(const FString& Name);

	/** Whether Create may be pressed: on the last step, with a valid name and a complete appearance. */
	UFUNCTION(BlueprintPure, Category = "Character Creator")
	bool CanCreate() const;

	/** Sends the character to the API (which checks the appearance again). */
	void Create(UVaesenApiSubsystem* Api, const FString& GuildId, TFunction<void(bool bSuccess, const FVaesenCharacter& Character, const FString& Error)> OnComplete);

private:
	UPROPERTY()
	TObjectPtr<UDataTable> Presets;

	EVaesenCreatorStep Step = EVaesenCreatorStep::Basics;
	FVaesenAppearance Appearance;
	FString Class;
	FString FirstName;
	FString LastName;
	/** Set once the body or face steps were changed by the player (changing sex then needs a confirmation). */
	bool bBodyOrFaceEdited = false;

	bool IsAllowed(EVaesenAppearancePresetKind Kind, FName Id) const;
	/** The sex's first allowed presets for body, face, hair and eyebrows; no facial hair. */
	void ResetForSex();
	void Changed() { OnChanged.Broadcast(); }
};
