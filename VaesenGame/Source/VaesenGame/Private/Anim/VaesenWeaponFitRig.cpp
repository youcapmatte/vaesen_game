#include "Anim/VaesenWeaponFitRig.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Weapons/VaesenWeaponActor.h"
#include "Weapons/VaesenWeaponData.h"

#if WITH_EDITOR
void UVaesenFitHandleComponent::PostEditComponentMove(bool bFinished)
{
	Super::PostEditComponentMove(bFinished);
	if (AVaesenWeaponFitRig* Rig = Cast<AVaesenWeaponFitRig>(GetOwner()))
	{
		Rig->OnHandleMoved(this, bFinished);
	}
}
#endif

AVaesenWeaponFitRig::AVaesenWeaponFitRig()
{
	PrimaryActorTick.bCanEverTick = true;
	bIsEditorOnlyActor = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Like a character's mesh: feet at the origin, facing the actor's forward (X).
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Grip = CreateDefaultSubobject<USceneComponent>(TEXT("Grip"));
	// Into the hand socket once the body has its mesh (ApplySetup).
	Grip->SetupAttachment(Body);

	WeaponMesh = CreateDefaultSubobject<UVaesenFitHandleComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Grip);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LeftHandHandle = CreateDefaultSubobject<UVaesenFitHandleComponent>(TEXT("LeftHandHandle"));
	LeftHandHandle->SetupAttachment(Grip);
	LeftHandHandle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftHandHandle->SetRelativeScale3D(FVector(0.05));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		LeftHandHandle->SetStaticMesh(Sphere.Object);
	}

	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny")));
	AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Vaesen/Animations/ABP_VaesenCharacter.ABP_VaesenCharacter_C")));
}

UVaesenWeaponItem* AVaesenWeaponFitRig::GetWeaponDefaults() const
{
	return Weapon ? Weapon->GetDefaultObject<UVaesenWeaponItem>() : nullptr;
}

FVaesenHeldWeapon AVaesenWeaponFitRig::GetHeldWeapon() const
{
	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	if (!Defaults || !WeaponMesh->GetStaticMesh())
	{
		return FVaesenHeldWeapon();
	}
	return { Grip, WeaponMesh, &Defaults->Hold };
}

void AVaesenWeaponFitRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySetup();
	ApplyPose();
}

void AVaesenWeaponFitRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyPose();
}

void AVaesenWeaponFitRig::ApplySetup()
{
	if (USkeletalMesh* Mesh = BodyMesh.LoadSynchronous(); Mesh && Body->GetSkeletalMeshAsset() != Mesh)
	{
		Body->SetSkeletalMeshAsset(Mesh);
	}
	if (UClass* Anim = AnimClass.LoadSynchronous(); Anim && Body->GetAnimClass() != Anim)
	{
		Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		Body->SetAnimInstanceClass(Anim);
	}
#if WITH_EDITOR
	// The editor doesn't play: this pose is the point of the rig.
	Body->SetUpdateAnimationInEditor(true);
#endif
	LinkedLayer = nullptr;
	PlayingAnimation = nullptr;
	PlayingMontage = nullptr;

	const FName HandSocket = UVaesenWeaponCategoryDefaults::Get()->MainHandSocket;
	if (Body->GetSkeletalMeshAsset() && Grip->GetAttachSocketName() != HandSocket)
	{
		Grip->AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, HandSocket);
	}

	// The weapon's mesh, held by its Grip socket exactly as the weapon actor holds it.
	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	WeaponMesh->SetRelativeTransform(FTransform::Identity);
	WeaponMesh->SetStaticMesh(Defaults ? Defaults->GetHeldStaticMesh() : nullptr);
	MeshInGrip = AVaesenWeaponActor::GripAlignment(WeaponMesh, AVaesenWeaponActor::HeldMeshScale(Defaults));
	WeaponMesh->SetRelativeTransform(MeshInGrip);
	LeftHandHandle->SetVisibility(Defaults && Defaults->bTwoHanded);
}

void AVaesenWeaponFitRig::ApplyPose()
{
	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	UAnimInstance* Anim = Body->GetAnimInstance();

	// The weapon category's locomotion, as when it's drawn in the game.
	const EVaesenWeaponCategory Category = Defaults ? Defaults->Category : EVaesenWeaponCategory::Unarmed;
	UClass* Layer = UVaesenWeaponCategoryDefaults::Get()->GetCategory(Category).AnimLayer;
	const FName Tag = UVaesenEquipmentComponent::WeaponLocomotionTag;
	if (Anim && Layer && Layer != LinkedLayer && Anim->GetLinkedAnimGraphInstanceByTag(Tag))
	{
		Anim->LinkAnimGraphByTag(Tag, Layer);
		LinkedLayer = Layer;
	}

	// The weapon and the off-hand handle where its Hold puts them (edits in the Blueprint show at once).
	if (Defaults)
	{
		Grip->SetRelativeTransform(Defaults->Hold.InHand);
		FVector LeftHand;
		if (GetHeldWeapon().GetLocalSocket(TEXT("LeftHandIK"), LeftHand))
		{
			LeftHandHandle->SetRelativeLocation(LeftHand + Defaults->Hold.LeftHandOffset);
		}
	}
	WeaponMesh->SetRelativeTransform(MeshInGrip);

	// The preview animation on the upper body, held at its time (or playing).
	if (!Anim)
	{
		return;
	}
	if (PlayingMontage && !Anim->Montage_IsActive(PlayingMontage))
	{
		// It ended (played through) or the anim instance was remade: start it again.
		PlayingAnimation = nullptr;
	}
	if (PreviewAnimation != PlayingAnimation)
	{
		Anim->Montage_Stop(0.0f);
		PlayingMontage = nullptr;
		if (UAnimMontage* Montage = Cast<UAnimMontage>(PreviewAnimation))
		{
			Anim->Montage_Play(Montage);
			PlayingMontage = Montage;
		}
		else if (PreviewAnimation)
		{
			PlayingMontage = Anim->PlaySlotAnimationAsDynamicMontage(PreviewAnimation, TEXT("UpperBody"), 0.0f, 0.0f, 1.0f, 1000000);
		}
		PlayingAnimation = PreviewAnimation;
	}
	if (PlayingMontage)
	{
		if (bPlayPreview)
		{
			Anim->Montage_Resume(PlayingMontage);
		}
		else
		{
			Anim->Montage_SetPosition(PlayingMontage, FMath::Clamp(PreviewTime, 0.0f, PlayingMontage->GetPlayLength()));
			Anim->Montage_Pause(PlayingMontage);
		}
	}
}

void AVaesenWeaponFitRig::SaveWeapon()
{
#if WITH_EDITOR
	if (!Weapon)
	{
		return;
	}
	UPackage* Package = Weapon->GetOutermost();
	const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(Package, nullptr, *FileName, Args);
#endif
}

void AVaesenWeaponFitRig::ResetHold()
{
	if (UVaesenWeaponItem* Defaults = GetWeaponDefaults())
	{
		Defaults->Modify();
		Defaults->Hold = FVaesenWeaponHold();
		Defaults->MarkPackageDirty();
		ApplyPose();
	}
}

#if WITH_EDITOR
void AVaesenWeaponFitRig::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);
	ApplySetup();
	ApplyPose();
}

void AVaesenWeaponFitRig::OnHandleMoved(UVaesenFitHandleComponent* Handle, bool bFinished)
{
	UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	if (!Defaults || !WeaponMesh->GetStaticMesh())
	{
		return;
	}
	Defaults->Modify();
	FVaesenWeaponHold& Hold = Defaults->Hold;

	if (Handle == LeftHandHandle)
	{
		// The off hand: its offset from the LeftHandIK socket, in the weapon's space.
		FVector Socket;
		GetHeldWeapon().GetLocalSocket(TEXT("LeftHandIK"), Socket);
		Hold.LeftHandOffset = Grip->GetComponentTransform().InverseTransformPosition(Handle->GetComponentLocation()) - Socket;
	}
	else if (Handle == WeaponMesh)
	{
		// Where the weapon (its grip) was moved to.
		const FTransform Moved = MeshInGrip.Inverse() * WeaponMesh->GetComponentTransform();
		// Back under its grip, so its sockets read right relative to the grip again.
		WeaponMesh->SetRelativeTransform(MeshInGrip);
		const bool bShouldered = IsAimPose() && Defaults->bTwoHanded && Hold.bShoulderAim && Defaults->Category != EVaesenWeaponCategory::Bow;
		if (bShouldered)
		{
			// Aimed: the shoulder-aim values that lay it there; the hand follows it.
			UVaesenAnimInstance::ShoulderHoldFromPlacement(Body, GetHeldWeapon(), GetAim(), FTransform(Moved.GetRotation(), Moved.GetLocation()), Hold);
		}
		else
		{
			// From the hip: how it sits in the hand.
			FTransform InHand = Moved.GetRelativeTransform(Body->GetSocketTransform(UVaesenWeaponCategoryDefaults::Get()->MainHandSocket));
			InHand.SetScale3D(FVector::OneVector);
			Hold.InHand = InHand;
		}
	}
	Defaults->MarkPackageDirty();
	ApplyPose();
}
#endif
