#include "Appearance/VaesenRetargetAnimInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Retargeter/IKRetargeter.h"

void UVaesenRetargetAnimInstance::SetSource(USkeletalMeshComponent* Source, UIKRetargeter* Retargeter)
{
	FAnimNode_RetargetPoseFromMesh& Retarget = GetProxyOnGameThread<FVaesenRetargetAnimInstanceProxy>().Retarget;
	Retarget.RetargetFrom = ERetargetSourceMode::CustomSkeletalMeshComponent;
	Retarget.SourceMeshComponent = Source;
	Retarget.IKRetargeterAsset = Retargeter;
	if (USkeletalMeshComponent* const Target = GetSkelMeshComponent(); Target && Source)
	{
		Target->AddTickPrerequisiteComponent(Source);
	}
}
