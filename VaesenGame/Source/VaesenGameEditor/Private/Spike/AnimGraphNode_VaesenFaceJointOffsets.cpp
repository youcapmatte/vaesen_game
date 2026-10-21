#include "Spike/AnimGraphNode_VaesenFaceJointOffsets.h"

#define LOCTEXT_NAMESPACE "VaesenFaceJointOffsets"

FText UAnimGraphNode_VaesenFaceJointOffsets::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Title", "Vaesen Face Joint Offsets");
}

FText UAnimGraphNode_VaesenFaceJointOffsets::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Moves face joints per character after the face rig. Place it after the RigLogic node.");
}

FString UAnimGraphNode_VaesenFaceJointOffsets::GetNodeCategory() const
{
	return TEXT("Vaesen");
}

#undef LOCTEXT_NAMESPACE
