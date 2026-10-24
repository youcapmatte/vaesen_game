#include "Anim/AnimNode_VaesenShoulderAim.h"

#include "Anim/VaesenAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "TwoBoneIK.h"
#include "Weapons/VaesenWeaponTypes.h"

namespace VaesenShoulderAim
{
	/** Q, but turning no more than MaxDegrees. */
	FQuat Limit(const FQuat& Q, float MaxDegrees)
	{
		const float Max = FMath::DegreesToRadians(MaxDegrees);
		return Q.GetAngle() > Max ? FQuat(Q.GetRotationAxis(), Max) : Q;
	}

	/** T turned by Turn about Pivot. */
	FTransform TurnAbout(const FTransform& T, const FQuat& Turn, const FVector& Pivot)
	{
		return FTransform(Turn * T.GetRotation(), Pivot + Turn.RotateVector(T.GetLocation() - Pivot), T.GetScale3D());
	}

	/** Upper arm, forearm and hand turned so the hand reaches Target; it takes Target's rotation if bTurnHand. */
	void SolveArm(FTransform& Upper, FTransform& Lower, FTransform& Hand, const FTransform& Target, bool bTurnHand = true)
	{
		FVector Elbow, Wrist;
		AnimationCore::SolveTwoBoneIK(Upper.GetLocation(), Lower.GetLocation(), Hand.GetLocation(), Lower.GetLocation(), Target.GetLocation(),
			Elbow, Wrist, false, 1.0, 1.0);
		const FQuat UpperTurn = FQuat::FindBetweenVectors(Lower.GetLocation() - Upper.GetLocation(), Elbow - Upper.GetLocation());
		const FQuat LowerTurn = FQuat::FindBetweenVectors(UpperTurn.RotateVector(Hand.GetLocation() - Lower.GetLocation()), Wrist - Elbow);
		Upper.SetRotation(UpperTurn * Upper.GetRotation());
		Lower = FTransform(LowerTurn * UpperTurn * Lower.GetRotation(), Elbow, Lower.GetScale3D());
		Hand = FTransform(bTurnHand ? Target.GetRotation() : LowerTurn * UpperTurn * Hand.GetRotation(), Wrist, Hand.GetScale3D());
	}

	/** Eased from A to B as Alpha goes 0..1. */
	FVector Ease(const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::Lerp(A, B, FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f)));
	}

	/**
	 * Where the right palm is while working the bolt (weapon space), at Phase of the cycle: from the grip to the knob,
	 * up, back, forward, down, and back to the grip.
	 */
	FVector BoltPalm(const FVector& Grip, const FVector& Knob, float Phase)
	{
		const FVector Lifted = Knob + FVector(0.0, -1.5, 3.0);
		const FVector Pulled = Lifted + FVector(-9.0, 0.0, 0.0);
		const auto Part = [Phase](float From, float To) { return (Phase - From) / (To - From); };
		if (Phase < VaesenBolt::Reached)
		{
			return Ease(Grip, Knob, Part(0.0f, VaesenBolt::Reached));
		}
		if (Phase < VaesenBolt::Lifted)
		{
			return Ease(Knob, Lifted, Part(VaesenBolt::Reached, VaesenBolt::Lifted));
		}
		if (Phase < VaesenBolt::Back)
		{
			return Ease(Lifted, Pulled, Part(VaesenBolt::Lifted, VaesenBolt::Back));
		}
		if (Phase < VaesenBolt::Forward)
		{
			return Ease(Pulled, Lifted, Part(VaesenBolt::Back, VaesenBolt::Forward));
		}
		if (Phase < VaesenBolt::Down)
		{
			return Ease(Lifted, Knob, Part(VaesenBolt::Forward, VaesenBolt::Down));
		}
		return Ease(Knob, Grip, Part(VaesenBolt::Down, 1.0f));
	}
}

void FAnimNode_VaesenShoulderAim::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const UVaesenAnimInstance* Vaesen = Cast<UVaesenAnimInstance>(InAnimInstance);
	bActive = Vaesen && Vaesen->GetShoulderAimInput(Input);
}

void FAnimNode_VaesenShoulderAim::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	for (FBoneReference* Bone : { &Spine, &Neck, &Head, &ClavicleR, &UpperArmR, &LowerArmR, &HandR, &UpperArmL, &LowerArmL, &HandL, &WeaponBone })
	{
		Bone->Initialize(RequiredBones);
	}
}

bool FAnimNode_VaesenShoulderAim::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	for (FBoneReference* Bone : { &Spine, &Neck, &Head, &ClavicleR, &UpperArmR, &LowerArmR, &HandR, &UpperArmL, &LowerArmL, &HandL, &WeaponBone })
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
	using namespace VaesenShoulderAim;
	const FBoneContainer& Bones = Output.Pose.GetPose().GetBoneContainer();
	const auto Pose = [&](const FBoneReference& Bone) { return Output.Pose.GetComponentSpaceTransform(Bone.GetCompactPoseIndex(Bones)); };
	const auto Emit = [&](const FBoneReference& Bone, const FTransform& Transform) { OutBoneTransforms.Add(FBoneTransform(Bone.GetCompactPoseIndex(Bones), Transform)); };

	FTransform Hand = Pose(HandR);
	// The weapon bone sits on the right palm (hand_r_weapon); the weapon on it by its own hold.
	const FTransform& WeaponBoneInHand = Input.PalmOnHand;
	if (!Input.bHeld)
	{
		Emit(WeaponBone, WeaponBoneInHand * Hand);
		OutBoneTransforms.Sort(FCompareBoneTransformIndex());
		return;
	}
	const FTransform SpinePose = Pose(Spine);
	const FTransform NeckPose = Pose(Neck);
	const FTransform HeadPose = Pose(Head);
	const FTransform ClaviclePose = Pose(ClavicleR);
	FTransform Upper = Pose(UpperArmR);
	FTransform Lower = Pose(LowerArmR);
	FTransform UpperL = Pose(UpperArmL);
	FTransform LowerL = Pose(LowerArmL);
	FTransform HandLeft = Pose(HandL);

	// Carried at a run: the wrist turns the blade along the carry direction (about the hand, the arm swings on).
	const float Carry = Input.bShoulder ? 0.0f : FMath::Clamp(Input.CarryBlend, 0.0f, 1.0f);
	if (Carry > 0.0f)
	{
		const FVector Blade = (Input.GripOnWeaponBone * WeaponBoneInHand * Hand).GetUnitAxis(EAxis::X);
		const FQuat Turn = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Blade, Input.CarryDirection.GetSafeNormal()), Carry);
		Hand.SetRotation((Turn * Hand.GetRotation()).GetNormalized());
	}
	const FTransform InHand = Input.GripOnWeaponBone * WeaponBoneInHand * Hand;

	FTransform Weapon = InHand;
	const float Blend = Input.bShoulder ? FMath::Clamp(Input.ShoulderBlend, 0.0f, 1.0f) : 0.0f;
	if (Blend > 0.0f)
	{
		// A shot rocks the upper body back about the chest (spine_05): what hangs from it (neck, shoulders, arms) goes too.
		const FVector Along = Input.Aim.GetForwardVector();
		const FVector Right = Input.Aim.GetRightVector();
		const float RockAngle = FMath::DegreesToRadians(Input.RecoilBodyPitch * Input.Kick);
		FQuat Rock(Right, RockAngle);
		if (Rock.RotateVector(Along).Z < Along.Z)
		{
			Rock = FQuat(Right, -RockAngle);
		}
		const FVector Chest = SpinePose.GetLocation();
		const FTransform NewSpine(Rock * SpinePose.GetRotation(), Chest, SpinePose.GetScale3D());
		const FTransform Neck0 = TurnAbout(NeckPose, Rock, Chest);
		const FTransform Head0 = TurnAbout(HeadPose, Rock, Chest);
		const FTransform Clavicle0 = TurnAbout(ClaviclePose, Rock, Chest);
		FTransform Upper0 = TurnAbout(Upper, Rock, Chest);
		FTransform Lower0 = TurnAbout(Lower, Rock, Chest);
		FTransform Hand0 = TurnAbout(Hand, Rock, Chest);

		// Laid along the aim (the Sight socket's line), the muzzle kicked up by a shot.
		const FQuat Turned = Input.Aim * Input.AimRotation * FRotator(Input.RecoilPitch * Input.Kick, 0.0, 0.0).Quaternion() * Input.SightRotation.Inverse();
		// The eye (it turns with the head as the head bows), and where the sight goes in front of it.
		const FVector Eye = Input.bHasEyeSocket ? (Input.EyeOnHead * Head0).GetLocation() : Head0.GetLocation() + FVector(-3.0, 9.0, 9.0);
		const FVector SightFromEye = Input.Aim.RotateVector(Input.EyeOffset);
		const auto PocketOf = [this](const FTransform& OnClavicle, const FTransform& OnUpper)
		{
			const FVector Pocket = Input.bHasPocketSocket ? (Input.PocketOnClavicle * OnClavicle).GetLocation()
				: OnUpper.GetLocation() + Input.Aim.RotateVector(FVector(6.0, -5.0, 0.0));
			return Pocket + Input.Aim.RotateVector(Input.ShoulderOffset);
		};

		FTransform Shouldered;
		FTransform NewNeck = Neck0;
		FTransform NewClavicle = Clavicle0;
		if (Input.bHasStock)
		{
			// Where it would be with the sight at the eye, and with the butt in the shoulder pocket: the gap between the
			// two (across the aim) is shared, the shoulder lifting to the stock and the head bowing onto it. A turn moves
			// its point along an arc, not straight, so it's done a few times, each closing what's left.
			const FTransform UpperOnClavicle = Upper0.GetRelativeTransform(Clavicle0);
			const FVector EyeFromNeck = Eye - Neck0.GetLocation();
			FQuat Lift = FQuat::Identity;
			FQuat Bow = FQuat::Identity;
			for (int32 Step = 0; Step < 4; ++Step)
			{
				NewClavicle.SetRotation(Lift * Clavicle0.GetRotation());
				const FVector Pocket = PocketOf(NewClavicle, UpperOnClavicle * NewClavicle);
				const FVector EyeNow = Neck0.GetLocation() + Bow.RotateVector(EyeFromNeck);
				FVector Gap = (EyeNow + SightFromEye - Turned.RotateVector(Input.SightLocation)) - (Pocket - Turned.RotateVector(Input.StockLocation));
				Gap -= Along * FVector::DotProduct(Gap, Along);
				if (Gap.SizeSquared() < 0.01f)
				{
					break;
				}
				const FVector ShoulderLever = Pocket - Clavicle0.GetLocation();
				Lift = Limit(FQuat::FindBetweenVectors(ShoulderLever, ShoulderLever + Gap * Input.ShoulderShare) * Lift, MaxShoulderLift);
				const FVector HeadLever = EyeNow - Neck0.GetLocation();
				Bow = Limit(FQuat::FindBetweenVectors(HeadLever, HeadLever - Gap * (1.0f - Input.ShoulderShare)) * Bow, MaxHeadBow);
			}
			// A shot drives the shoulder back with the butt (along the aim), the head staying on the stock.
			NewClavicle.SetRotation(Lift * Clavicle0.GetRotation());
			const FVector KickedLever = PocketOf(NewClavicle, UpperOnClavicle * NewClavicle) - Clavicle0.GetLocation();
			const FQuat Driven = Limit(FQuat::FindBetweenVectors(KickedLever, KickedLever - Along * Input.RecoilBack * Input.Kick), MaxShoulderLift);
			NewClavicle.SetRotation(Driven * Lift * Clavicle0.GetRotation());
			NewNeck.SetRotation(Bow * Neck0.GetRotation());
			// The arm rides on the shoulder; the butt goes on the lifted shoulder (which the rock pushed back).
			Upper0 = UpperOnClavicle * NewClavicle;
			Lower0 = Lower0.GetRelativeTransform(Clavicle0) * NewClavicle;
			Hand0 = Hand0.GetRelativeTransform(Clavicle0) * NewClavicle;
			Shouldered = FTransform(Turned, PocketOf(NewClavicle, Upper0) - Turned.RotateVector(Input.StockLocation));
		}
		else
		{
			// No stock: the sight in front of the eye, pushed back by the shot.
			Shouldered = FTransform(Turned, Eye + SightFromEye - Turned.RotateVector(Input.SightLocation) - Along * Input.RecoilBack * Input.Kick);
		}

		// Blended with the pose as it was (in and out of aiming).
		const auto Mix = [Blend](const FTransform& From, const FTransform& To)
		{
			FTransform Out;
			Out.Blend(From, To, Blend);
			return Out;
		};
		Emit(Spine, Mix(SpinePose, NewSpine));
		Emit(Neck, Mix(NeckPose, NewNeck));
		Emit(ClavicleR, Mix(ClaviclePose, NewClavicle));
		Upper = Mix(Upper, Upper0);
		Lower = Mix(Lower, Lower0);
		Hand = Mix(Hand, Hand0);
		// The left arm hangs from the rocked chest too.
		UpperL = Mix(UpperL, TurnAbout(UpperL, Rock, Chest));
		LowerL = Mix(LowerL, TurnAbout(LowerL, Rock, Chest));
		HandLeft = Mix(HandLeft, TurnAbout(HandLeft, Rock, Chest));
		Weapon = Mix(InHand, Shouldered);
	}

	// The weapon on its bone.
	Emit(WeaponBone, Input.GripOnWeaponBone.Inverse() * Weapon);

	// The right hand: on the grip, or working the bolt.
	const FTransform PalmOnWeapon = Input.PalmOnHand.Inverse() * Input.GripOnWeaponBone.Inverse();
	if (Input.bCycling)
	{
		const FTransform Palm = Input.GripOnWeaponBone.Inverse();
		const FVector BoltPalmLocation = BoltPalm(Palm.GetLocation(), Input.BoltLocation, Input.CyclePhase);
		const FTransform PalmTarget(Palm.GetRotation(), BoltPalmLocation);
		SolveArm(Upper, Lower, Hand, Input.PalmOnHand.Inverse() * PalmTarget * Weapon);
	}
	else if (Blend > 0.0f)
	{
		SolveArm(Upper, Lower, Hand, PalmOnWeapon * Weapon);
	}
	if (Blend > 0.0f || Input.bCycling)
	{
		Emit(UpperArmR, Upper);
		Emit(LowerArmR, Lower);
		Emit(HandR, Hand);
	}
	else if (Carry > 0.0f)
	{
		Emit(HandR, Hand);
	}

	// The left hand on LeftGrip.
	if (Input.bLeftHand)
	{
		SolveArm(UpperL, LowerL, HandLeft, Input.LeftHandInWeapon * Weapon, Input.bLeftHandTurns);
		Emit(UpperArmL, UpperL);
		Emit(LowerArmL, LowerL);
		Emit(HandL, HandLeft);
	}
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}
