#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Anim/AnimNode_VaesenShoulderAim.h"
#include "AnimGraphNode_VaesenShoulderAim.generated.h"

/** The anim graph node for FAnimNode_VaesenShoulderAim (a long gun shouldered, both hands on it). */
UCLASS()
class VAESENGAMEANIMGRAPH_API UAnimGraphNode_VaesenShoulderAim : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_VaesenShoulderAim Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
