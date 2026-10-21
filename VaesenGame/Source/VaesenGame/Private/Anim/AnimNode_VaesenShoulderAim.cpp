#include "Anim/AnimNode_VaesenShoulderAim.h"

#include "Anim/VaesenAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"

namespace VaesenShoulderAim
{
	/** Q, but turning no more than MaxDegrees. */
	FQuat Limit(const FQuat& Q, float MaxDegrees)
	{
		const float Max = FMath::DegreesToRadians(MaxDegrees);
		return Q.GetAngle() > Max ? FQuat(Q.GetRotationAxis(), Max) : Q;
	}

	/** Upper arm, forearm and hand turned so the hand reaches Target (and takes its rotation); the elbow bends as it was. */
	void SolveArm(FTransform& Upper, FTransform& Lower, FTransform& Hand, const FTransform& Target)
	{
		FVector Elbow, Wrist;
		AnimationCore::SolveTwoBoneIK(Upper.GetLocation(), Lower.GetLocation(), Hand.GetLocation(), Lower.GetLocation(), Target.GetLocation(),
			Elbow, Wrist, false, 1.0, 1.0);
		const FQuat UpperTurn = FQuat::FindBetweenVectors(Lower.GetLocation() - Upper.GetLocation(), Elbow - Upper.GetLocation());
		const FQuat LowerTurn = FQuat::FindBetweenVectors(UpperTurn.RotateVector(Hand.GetLocation() - Lower.GetLocation()), Wrist - Elbow);
		Upper.SetRotation(UpperTurn * Upper.GetRotation());
		Lower = FTransform(LowerTurn * UpperTurn * Lower.GetRotation(), Elbow, Lower.GetScale3D());
		Hand = FTransform(Target.GetRotation(), Wrist, Hand.GetScale3D());
	}
}

void FAnimNode_VaesenShoulderAim::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const UVaesenAnimInstance* Vaesen = Cast<UVaesenAnimInstance>(InAnimInstance);
	bActive = Vaesen && Vaesen->GetShoulderAimInput(Input);
}

void FAnimNode_VaesenShoulderAim::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FBoneReference* Bone : { &Neck, &Head, &ClavicleR, &UpperArmR, &LowerArmR, &HandR, &UpperArmL, &LowerArmL, &HandL })
	{
		Bone->Initialize(RequiredBones);
	}
}

bool FAnimNode_VaesenShoulderAim::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	for (FBoneReference* Bone : { &Neck, &Head, &ClavicleR, &UpperArmR, &LowerArmR, &HandR, &UpperArmL, &LowerArmL, &HandL })
	{
		if (!Bone->IsValidToEvaluate(RequiredBones))
		{
			return false;
		}
	}
	return true;
}

void FAnimNode_VaesenShoulderAim::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	if (!bActive)
	{
		return;
	}
	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	const auto Pose = [&](const FBoneReference& Bone) { return Output.Pose.GetComponentSpaceTransform(Bone.GetCompactPoseIndex(Bones)); };
	const FTransform NeckPose = Pose(Neck);
	const FTransform HeadPose = Pose(Head);
	const FTransform Clavicle = Pose(ClavicleR);
	FTransform Upper = Pose(UpperArmR);
	FTransform Lower = Pose(LowerArmR);
	FTransform Hand = Pose(HandR);

	// Laid along the aim (the Sight socket's line), the muzzle kicked up by a shot.
	const FVector Along = Input.Aim.GetForwardVector();
	const FQuat Turned = Input.Aim * Input.AimRotation * FRotator(Input.RecoilPitch * Input.Kick, 0.0, 0.0).Quaternion() * Input.SightRotation.Inverse();
	const FVector Kickback = Along * Input.RecoilBack * Input.Kick;
	// The eye (it turns with the head as the head bows), and where the sight goes in front of it.
	const FVector Eye = Input.bHasEyeSocket ? (Input.EyeOnHead * HeadPose).GetLocation() : HeadPose.GetLocation() + FVector(-3.0, 9.0, 9.0);
	const FVector SightFromEye = Input.Aim.RotateVector(Input.EyeOffset);
	const auto PocketOf = [this](const FTransform& OnClavicle, const FTransform& OnUpper)
	{
		const FVector Pocket = Input.bHasPocketSocket ? (Input.PocketOnClavicle * OnClavicle).GetLocation()
			: OnUpper.GetLocation() + Input.Aim.RotateVector(FVector(6.0, -5.0, 0.0));
		return Pocket + Input.Aim.RotateVector(Input.ShoulderOffset);
	};

	FTransform Weapon;
	FTransform NewNeck = NeckPose;
	FTransform NewClavicle = Clavicle;
	if (Input.bHasStock)
	{
		// Where it would be with the sight at the eye, and with the butt in the shoulder pocket: the gap between the
		// two (across the aim) is shared, the shoulder lifting to the stock and the head bowing onto it. A turn moves
		// its point along an arc, not straight, so it's done a few times, each closing what's left.
		const FTransform UpperOnClavicle = Upper.GetRelativeTransform(Clavicle);
		const FVector EyeFromNeck = Eye - NeckPose.GetLocation();
		FQuat Lift = FQuat::Identity;
		FQuat Bow = FQuat::Identity;
		for (int32 Step = 0; Step < 4; ++Step)
		{
			NewClavicle.SetRotation(Lift * Clavicle.GetRotation());
			const FVector Pocket = PocketOf(NewClavicle, UpperOnClavicle * NewClavicle);
			const FVector EyeNow = NeckPose.GetLocation() + Bow.RotateVector(EyeFromNeck);
			FVector Gap = (EyeNow + SightFromEye - Turned.RotateVector(Input.SightLocation)) - (Pocket - Turned.RotateVector(Input.StockLocation));
			Gap -= Along * FVector::DotProduct(Gap, Along);
			if (Gap.SizeSquared() < 0.01f)
			{
				break;
			}
			const FVector ShoulderLever = Pocket - Clavicle.GetLocation();
			Lift = VaesenShoulderAim::Limit(FQuat::FindBetweenVectors(ShoulderLever, ShoulderLever + Gap * Input.ShoulderShare) * Lift, MaxShoulderLift);
			const FVector HeadLever = EyeNow - NeckPose.GetLocation();
			Bow = VaesenShoulderAim::Limit(FQuat::FindBetweenVectors(HeadLever, HeadLever - Gap * (1.0f - Input.ShoulderShare)) * Bow, MaxHeadBow);
		}
		NewClavicle.SetRotation(Lift * Clavicle.GetRotation());
		NewNeck.SetRotation(Bow * NeckPose.GetRotation());
		// The arm rides on the shoulder.
		Upper = UpperOnClavicle * NewClavicle;
		Lower = Lower.GetRelativeTransform(Clavicle) * NewClavicle;
		Hand = Hand.GetRelativeTransform(Clavicle) * NewClavicle;

		// The butt on the lifted shoulder.
		Weapon = FTransform(Turned, PocketOf(NewClavicle, Upper) - Turned.RotateVector(Input.StockLocation) - Kickback);
		OutBoneTransforms.Add(FBoneTransform(Neck.GetCompactPoseIndex(Bones), NewNeck));
		OutBoneTransforms.Add(FBoneTransform(ClavicleR.GetCompactPoseIndex(Bones), NewClavicle));
	}
	else
	{
		// No stock: the sight in front of the eye.
		Weapon = FTransform(Turned, Eye + SightFromEye - Turned.RotateVector(Input.SightLocation) - Kickback);
	}

	// The right hand holds the weapon by its grip; the left holds LeftGrip.
	VaesenShoulderAim::SolveArm(Upper, Lower, Hand, Input.WeaponInHand.Inverse() * Weapon);
	OutBoneTransforms.Add(FBoneTransform(UpperArmR.GetCompactPoseIndex(Bones), Upper));
	OutBoneTransforms.Add(FBoneTransform(LowerArmR.GetCompactPoseIndex(Bones), Lower));
	OutBoneTransforms.Add(FBoneTransform(HandR.GetCompactPoseIndex(Bones), Hand));
	if (Input.bLeftHand)
	{
		FTransform UpperL = Pose(UpperArmL);
		FTransform LowerL = Pose(LowerArmL);
		FTransform HandLeft = Pose(HandL);
		VaesenShoulderAim::SolveArm(UpperL, LowerL, HandLeft, Input.LeftHandInWeapon * Weapon);
		OutBoneTransforms.Add(FBoneTransform(UpperArmL.GetCompactPoseIndex(Bones), UpperL));
		OutBoneTransforms.Add(FBoneTransform(LowerArmL.GetCompactPoseIndex(Bones), LowerL));
		OutBoneTransforms.Add(FBoneTransform(HandL.GetCompactPoseIndex(Bones), HandLeft));
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
