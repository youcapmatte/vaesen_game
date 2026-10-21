#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "Spike/AnimNode_VaesenFaceJointOffsets.h"
#include "AnimGraphNode_VaesenFaceJointOffsets.generated.h"

/** The animation Blueprint node for FAnimNode_VaesenFaceJointOffsets (Phase 0 spike, P0-4). */
UCLASS()
class UAnimGraphNode_VaesenFaceJointOffsets : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_VaesenFaceJointOffsets Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
};
