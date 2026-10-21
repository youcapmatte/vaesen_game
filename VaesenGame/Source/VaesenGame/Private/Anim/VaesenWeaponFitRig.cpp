#include "Anim/VaesenWeaponFitRig.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Inventory/VaesenEquipmentComponent.h"
#include "Items/VaesenWeaponItem.h"
#include "Misc/PackageName.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Weapons/VaesenWeaponActor.h"
#include "Weapons/VaesenWeaponData.h"

namespace VaesenWeaponFitRig
{
	const FName Grip(TEXT("Grip"));
	const FName LeftGrip(TEXT("LeftGrip"));
	const FName Sight(TEXT("Sight"));
	const FName Stock(TEXT("Stock"));
}

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

	// Into the hand socket once the body has its mesh (ApplySetup).
	Grip = CreateDefaultSubobject<USceneComponent>(TEXT("Grip"));
	Grip->SetupAttachment(Body);

	WeaponMesh = CreateDefaultSubobject<UVaesenFitHandleComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Grip);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GripHandle = MakeHandle(TEXT("GripHandle"), VaesenWeaponFitRig::Grip, TEXT("/Engine/BasicShapes/Cube.Cube"), 0.025f);
	LeftGripHandle = MakeHandle(TEXT("LeftGripHandle"), VaesenWeaponFitRig::LeftGrip, TEXT("/Engine/BasicShapes/Sphere.Sphere"), 0.04f);
	SightHandle = MakeHandle(TEXT("SightHandle"), VaesenWeaponFitRig::Sight, TEXT("/Engine/BasicShapes/Cone.Cone"), 0.03f);
	StockHandle = MakeHandle(TEXT("StockHandle"), VaesenWeaponFitRig::Stock, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"), 0.03f);

	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny.SKM_Manny")));
	AnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(TEXT("/Game/Vaesen/Animations/ABP_VaesenCharacter.ABP_VaesenCharacter_C")));
}

UVaesenFitHandleComponent* AVaesenWeaponFitRig::MakeHandle(FName Name, FName Socket, const TCHAR* Shape, float Size)
{
	UVaesenFitHandleComponent* Handle = CreateDefaultSubobject<UVaesenFitHandleComponent>(Name);
	Handle->SetupAttachment(WeaponMesh);
	Handle->Socket = Socket;
	Handle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Handle->SetUsingAbsoluteScale(true);
	Handle->SetWorldScale3D(FVector(Size));
	ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Shape);
	if (Mesh.Succeeded())
	{
		Handle->SetStaticMesh(Mesh.Object);
	}
	return Handle;
}

UVaesenWeaponItem* AVaesenWeaponFitRig::GetWeaponDefaults() const
{
	return Weapon ? Weapon->GetDefaultObject<UVaesenWeaponItem>() : nullptr;
}

UStaticMesh* AVaesenWeaponFitRig::GetStaticMesh() const
{
	return WeaponMesh->GetStaticMesh();
}

FVaesenHeldWeapon AVaesenWeaponFitRig::GetHeldWeapon() const
{
	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	if (!Defaults || !GetStaticMesh())
	{
		return FVaesenHeldWeapon();
	}
	return { Grip, WeaponMesh, &Defaults->Hold };
}

UVaesenFitHandleComponent* AVaesenWeaponFitRig::GetSocketHandle(FName Socket) const
{
	for (UVaesenFitHandleComponent* Handle : { GripHandle.Get(), LeftGripHandle.Get(), SightHandle.Get(), StockHandle.Get() })
	{
		if (Handle && Handle->Socket == Socket)
		{
			return Handle;
		}
	}
	return nullptr;
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

	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	WeaponMesh->SetStaticMesh(Defaults ? Defaults->GetHeldStaticMesh() : nullptr);
	AlignMesh();
}

void AVaesenWeaponFitRig::AlignMesh()
{
	// The weapon's mesh, held by its Grip socket exactly as the weapon actor holds it.
	WeaponMesh->SetRelativeTransform(FTransform::Identity);
	MeshInGrip = AVaesenWeaponActor::GripAlignment(WeaponMesh, AVaesenWeaponActor::HeldMeshScale(GetWeaponDefaults()));
	WeaponMesh->SetRelativeTransform(MeshInGrip);
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

	// The weapon in the hand (its Hold, normally zero), and a handle on each of its sockets.
	Grip->SetRelativeTransform(Defaults ? Defaults->Hold.InHand : FTransform::Identity);
	WeaponMesh->SetRelativeTransform(MeshInGrip);
	const UStaticMesh* Mesh = GetStaticMesh();
	for (UVaesenFitHandleComponent* Handle : { GripHandle.Get(), LeftGripHandle.Get(), SightHandle.Get(), StockHandle.Get() })
	{
		const UStaticMeshSocket* Socket = Mesh ? Mesh->FindSocket(Handle->Socket) : nullptr;
		Handle->SetVisibility(bShowSockets && Socket != nullptr);
		if (Socket)
		{
			Handle->SetRelativeLocationAndRotation(Socket->RelativeLocation, Socket->RelativeRotation);
		}
	}

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

void AVaesenWeaponFitRig::SetSocketFromGrip(FName Name, const FTransform& InGrip)
{
	UStaticMesh* Mesh = GetStaticMesh();
	if (!Mesh)
	{
		return;
	}
	Mesh->Modify();
	UStaticMeshSocket* Socket = Mesh->FindSocket(Name);
	if (!Socket)
	{
		Socket = NewObject<UStaticMeshSocket>(Mesh);
		Socket->SocketName = Name;
		Mesh->AddSocket(Socket);
	}
	// Grip space to the mesh's own space (the mesh sits under the grip by MeshInGrip).
	const FTransform InMesh = InGrip * MeshInGrip.Inverse();
	Socket->Modify();
	Socket->RelativeLocation = InMesh.GetLocation();
	Socket->RelativeRotation = InMesh.Rotator();
	Mesh->MarkPackageDirty();
}

void AVaesenWeaponFitRig::SaveWeapon()
{
#if WITH_EDITOR
	TArray<UPackage*> Packages;
	if (Weapon)
	{
		Packages.Add(Weapon->GetOutermost());
	}
	if (UStaticMesh* Mesh = GetStaticMesh())
	{
		Packages.Add(Mesh->GetOutermost());
	}
	for (UPackage* Package : Packages)
	{
		const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, nullptr, *FileName, Args);
	}
#endif
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
	const UVaesenWeaponItem* Defaults = GetWeaponDefaults();
	if (!Defaults || !GetStaticMesh())
	{
		return;
	}
	const FTransform GripWorld = Grip->GetComponentTransform();

	if (Handle != WeaponMesh)
	{
		// A socket handle: the socket is where it now is (all of them in the weapon's own frame, under the grip).
		FTransform InGrip = Handle->GetComponentTransform().GetRelativeTransform(WeaponMesh->GetComponentTransform()) * MeshInGrip;
		InGrip.SetScale3D(FVector::OneVector);
		SetSocketFromGrip(Handle->Socket, InGrip);
		if (Handle->Socket == VaesenWeaponFitRig::Grip)
		{
			// The right palm's place on the weapon: the weapon slides in the hand, the handle stays in the palm.
			AlignMesh();
		}
		ApplyPose();
		return;
	}

	// The weapon itself, moved to here (where its grip would be).
	const FTransform Moved = MeshInGrip.Inverse() * WeaponMesh->GetComponentTransform();
	// Back under its grip, so its sockets read right again.
	WeaponMesh->SetRelativeTransform(MeshInGrip);
	const FVaesenHeldWeapon Held = GetHeldWeapon();
	const bool bShouldered = IsAimPose() && Defaults->bTwoHanded && Defaults->Hold.bShoulderAim && Defaults->Category != EVaesenWeaponCategory::Bow;
	if (!bShouldered)
	{
		// From the hip: the hand keeps its place, so the Grip socket moves on the weapon to where the hand now is.
		SetSocketFromGrip(VaesenWeaponFitRig::Grip, GripWorld.GetRelativeTransform(Moved));
		AlignMesh();
	}
	else
	{
		// Aimed (the Shoulder Aim node's sums, backwards): the Sight's line turned so the weapon lies
		// as it was put, and the Stock socket moved to where the shoulder pocket now is on the weapon. Without a Stock,
		// the Sight moves to the point in front of the eye instead.
		const FVaesenWeaponHold& Hold = Defaults->Hold;
		const FRotator Aim = GetAim();
		const FQuat Line = Aim.Quaternion() * Hold.AimRotation.Quaternion();
		FTransform SightLocal(FVector(0.0, 0.0, 6.0));
		Held.GetLocalSocketTransform(VaesenWeaponFitRig::Sight, SightLocal);
		FVector StockLocal;
		if (Held.GetLocalSocket(VaesenWeaponFitRig::Stock, StockLocal))
		{
			const FVector Pocket = UVaesenAnimInstance::ShoulderPocket(Body, Aim) + Aim.RotateVector(Hold.ShoulderOffset);
			SetSocketFromGrip(VaesenWeaponFitRig::Stock, FTransform(Moved.InverseTransformPosition(Pocket)));
			SetSocketFromGrip(VaesenWeaponFitRig::Sight, FTransform(Moved.GetRotation().Inverse() * Line, SightLocal.GetLocation()));
		}
		else
		{
			const FVector EyePoint = UVaesenAnimInstance::EyePoint(Body) + Aim.RotateVector(Hold.AimSightOffset);
			SetSocketFromGrip(VaesenWeaponFitRig::Sight, FTransform(Moved.GetRotation().Inverse() * Line, Moved.InverseTransformPosition(EyePoint)));
		}
	}
	ApplyPose();
}
#endif
