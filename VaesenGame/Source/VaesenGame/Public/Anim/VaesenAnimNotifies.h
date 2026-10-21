#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Weapons/VaesenWeaponTypes.h"
#include "VaesenAnimNotifies.generated.h"

/** Base for AN_WeaponGrab and AN_WeaponRelease: the frame the hand is on the weapon, where it changes from sheath to hand. */
UCLASS(Abstract)
class VAESENGAME_API UAnimNotify_VaesenWeaponHandoff : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

/** AN_WeaponGrab: in a draw montage, the frame the hand closes on the sheathed weapon; it moves to the hand. */
UCLASS(meta = (DisplayName = "Weapon Grab (draw)"))
class VAESENGAME_API UAnimNotify_VaesenWeaponGrab : public UAnimNotify_VaesenWeaponHandoff
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Weapon Grab"); }
};

/** AN_WeaponRelease: in a sheath montage, the frame the weapon is in its sheath; it moves to the sheath socket. */
UCLASS(meta = (DisplayName = "Weapon Release (sheath)"))
class VAESENGAME_API UAnimNotify_VaesenWeaponRelease : public UAnimNotify_VaesenWeaponHandoff
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Weapon Release"); }
};

/**
 * AN_EjectCasing: in a fire, cycle or reload montage, throws out a spent casing from the weapon's Eject socket, but
 * only if the weapon's Casing Ejection matches Moment (a revolver's fire montage with this notify set to Fire ejects
 * nothing, since revolvers eject on reload). Without the notify, the code ejects at the right moment itself.
 */
UCLASS(meta = (DisplayName = "Eject Casing"))
class VAESENGAME_API UAnimNotify_VaesenEjectCasing : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Eject Casing"); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casing")
	EVaesenCasingMoment Moment = EVaesenCasingMoment::Fire;

	/** How many (a revolver emptied: 6). 0 = the spent rounds the weapon has. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Casing", meta = (ClampMin = "0"))
	int32 Count = 1;
};

/** Plays one of the drawn weapon's sounds (a swing whoosh, an insert-round click, a bolt). Silent if the slot is empty. */
UCLASS(meta = (DisplayName = "Weapon Sound"))
class VAESENGAME_API UAnimNotify_VaesenWeaponSound : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EVaesenWeaponSound Sound = EVaesenWeaponSound::SwingLight;
};

/** Plays the character's effort grunt (heavy attacks, throws), from its voice set. */
UCLASS(meta = (DisplayName = "Effort Vocal"))
class VAESENGAME_API UAnimNotify_VaesenEffortVocal : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Effort Vocal"); }
};

/**
 * ANS_MeleeHitWindow: while it runs, the weapon's striking part is live (the server traces it). Without it in the
 * attack montage, the attack's HitWindowStart / HitWindowEnd time the window instead.
 */
UCLASS(meta = (DisplayName = "Melee Hit Window"))
class VAESENGAME_API UAnimNotifyState_VaesenMeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Melee Hit Window"); }
};

/** AN_Footstep: a foot touches down (in locomotion animations). Sheathed weapons rattle. */
UCLASS(meta = (DisplayName = "Footstep"))
class VAESENGAME_API UAnimNotify_VaesenFootstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Footstep"); }
};
