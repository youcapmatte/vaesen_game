#include "AnimGraphNode_VaesenShoulderAim.h"

#define LOCTEXT_NAMESPACE "VaesenShoulderAim"

FText UAnimGraphNode_VaesenShoulderAim::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return GetControllerDescription();
}

FText UAnimGraphNode_VaesenShoulderAim::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Shoulders a long gun on this frame's pose: laid along the aim with the sight line at the eye and the butt "
		"in the shoulder pocket (the shoulder lifts and the head bows to meet it), both hands solved onto the weapon. "
		"Inputs come from UVaesenAnimInstance.");
}

FText UAnimGraphNode_VaesenShoulderAim::GetControllerDescription() const
{
	return LOCTEXT("Title", "Vaesen Shoulder Aim");
}

#undef LOCTEXT_NAMESPACE
