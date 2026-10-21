#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "VaesenRetargetAnimInstance.generated.h"

class UIKRetargeter;

/** Runs one Retarget Pose From Mesh node as the whole graph (no animation Blueprint needed). */
USTRUCT()
struct FVaesenRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FVaesenRetargetAnimInstanceProxy() = default;
	explicit FVaesenRetargetAnimInstanceProxy(UAnimInstance* InAnimInstance) : FAnimInstanceProxy(InAnimInstance) {}

	FAnimNode_RetargetPoseFromMesh Retarget;

protected:
	virtual FAnimNode_Base* GetCustomRootNode() override { return &Retarget; }
	virtual void GetCustomNodes(TArray<FAnimNode_Base*>& OutNodes) override { OutNodes.Add(&Retarget); }
};

/**
 * Poses a mesh from another mesh's animation through an IK Retargeter: a player's MetaHuman body follows the hidden
 * mannequin mesh that the character's own animation Blueprint and gameplay drive. Set with SetSource after it's the
 * mesh's anim instance.
 */
UCLASS(Transient, NotBlueprintable)
class VAESENGAME_API UVaesenRetargetAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** The mesh to copy the animation from (it must tick first), and the retargeter from its skeleton to this one. */
	void SetSource(USkeletalMeshComponent* Source, UIKRetargeter* Retargeter);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FVaesenRetargetAnimInstanceProxy(this); }
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FVaesenRetargetAnimInstanceProxy*>(InProxy); }
};
