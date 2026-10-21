#include "Appearance/VaesenFacePresetData.h"

#include "Appearance/VaesenAppearance.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"

TArray<FVaesenFaceJointOffset> UVaesenFacePresetData::CombinedOffsets(const TArray<FVaesenFaceSliderValue>& Sliders) const
{
	TMap<FName, FVector> Sum;
	for (const FVaesenFaceSliderValue& Slider : Sliders)
	{
		const TMap<FName, FVaesenSliderJointOffsets>& Side = Slider.Value >= 0.f ? PositiveOffsets : NegativeOffsets;
		if (const FVaesenSliderJointOffsets* Offsets = Side.Find(Slider.Name))
		{
			const float Weight = FMath::Abs(Slider.Value);
			for (const FVaesenFaceJointOffset& Offset : Offsets->Offsets)
			{
				Sum.FindOrAdd(Offset.Joint) += Offset.Offset * Weight;
			}
		}
	}

	TArray<FVaesenFaceJointOffset> Out;
	Out.Reserve(Sum.Num());
	for (const TPair<FName, FVector>& Pair : Sum)
	{
		Out.Add({ Pair.Key, Pair.Value });
	}
	return Out;
}

UGroomBindingAsset* UVaesenFacePresetData::FindBinding(const UGroomAsset* Groom) const
{
	const FSoftObjectPath Path(Groom);
	for (const FVaesenGroomBinding& Entry : GroomBindings)
	{
		if (Entry.Groom.ToSoftObjectPath() == Path)
		{
			return Entry.Binding.LoadSynchronous();
		}
	}
	return nullptr;
}
