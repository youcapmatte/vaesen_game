#include "Appearance/VaesenAppearanceComponent.h"

#include "Appearance/VaesenAppearancePresets.h"
#include "Appearance/VaesenFacePresetData.h"
#include "Appearance/VaesenSkinToneTable.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Spike/AnimNode_VaesenFaceJointOffsets.h"

namespace VaesenAppearance
{
	FName SliderMorph(FName Slider, bool bPositive)
	{
		return FName(*FString::Printf(TEXT("slider_%s_%s"), *Slider.ToString(), bPositive ? TEXT("pos") : TEXT("neg")));
	}

	const FName AgeSlider(TEXT("age"));
}

UVaesenAppearanceComponent::UVaesenAppearanceComponent()
{
	SetIsReplicatedByDefault(true);

	for (const TCHAR* Curve : {
		TEXT("head_wm1_normal_head_wm1_browsRaiseInner_L"), TEXT("head_wm1_normal_head_wm1_browsRaiseInner_R"),
		TEXT("head_wm1_normal_head_wm1_browsRaiseOuter_L"), TEXT("head_wm1_normal_head_wm1_browsRaiseOuter_R"),
		TEXT("head_wm2_normal_head_wm2_browsDown_L"), TEXT("head_wm2_normal_head_wm2_browsDown_R"),
		TEXT("head_wm1_normal_head_wm1_squintInner_L"), TEXT("head_wm1_normal_head_wm1_squintInner_R"),
		TEXT("head_wm3_normal_head_wm3_smile_L"), TEXT("head_wm3_normal_head_wm3_smile_R"),
		TEXT("head_wm1_normal_head_wm1_purse_UL"), TEXT("head_wm1_normal_head_wm1_purse_UR"),
		TEXT("head_wm1_normal_head_wm1_purse_DL"), TEXT("head_wm1_normal_head_wm1_purse_DR"),
		TEXT("head_wm2_normal_head_wm2_neckStretch_L"), TEXT("head_wm2_normal_head_wm2_neckStretch_R") })
	{
		AgeWrinkleCurves.Add(FName(Curve));
	}
}

void UVaesenAppearanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVaesenAppearanceComponent, Appearance);
}

void UVaesenAppearanceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Appearance.IsSet())
	{
		ApplyAppearance();
	}
}

void UVaesenAppearanceComponent::SetAppearance(const FVaesenAppearance& InAppearance)
{
	Appearance = InAppearance;
	ApplyAppearance();
}

void UVaesenAppearanceComponent::OnRep_Appearance()
{
	ApplyAppearance();
}

void UVaesenAppearanceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpawnedMetaHuman)
	{
		SpawnedMetaHuman->Destroy();
		SpawnedMetaHuman = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UVaesenAppearanceComponent::ClearMetaHuman()
{
	if (SpawnedMetaHuman)
	{
		SpawnedMetaHuman->Destroy();
		SpawnedMetaHuman = nullptr;
	}
	SpawnedFaceId = NAME_None;
	Appearance = FVaesenAppearance();
}

bool UVaesenAppearanceComponent::UpdateMetaHuman()
{
	if (!PresetTable || (SpawnedMetaHuman && SpawnedFaceId == Appearance.FacePresetId))
	{
		return false;
	}
	const FVaesenAppearancePresetRow* Row = PresetTable->FindRow<FVaesenAppearancePresetRow>(Appearance.FacePresetId, TEXT("ApplyAppearance"), /*bWarnIfRowMissing*/ false);
	if (!Row || Row->Kind != EVaesenAppearancePresetKind::Face)
	{
		UE_LOG(LogTemp, Warning, TEXT("Appearance: no face preset '%s' in %s"), *Appearance.FacePresetId.ToString(), *GetNameSafe(PresetTable));
		return false;
	}
	// Loaded synchronously for now; the creator and character spawning will stream these in ahead of time.
	UClass* MetaHumanClass = Row->MetaHumanClass.LoadSynchronous();
	if (!MetaHumanClass || !GetOwner())
	{
		return false;
	}
	FacePreset = Row->FacePreset.LoadSynchronous();

	if (SpawnedMetaHuman)
	{
		SpawnedMetaHuman->Destroy();
	}
	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	USceneComponent* const Parent = AttachParent ? AttachParent.Get() : GetOwner()->GetRootComponent();
	const FTransform Where = MetaHumanOffset * Parent->GetComponentTransform();
	SpawnedMetaHuman = GetWorld()->SpawnActor<AActor>(MetaHumanClass, Where, Params);
	if (SpawnedMetaHuman)
	{
		SpawnedMetaHuman->AttachToComponent(Parent, FAttachmentTransformRules::KeepWorldTransform);
	}
	SpawnedFaceId = Appearance.FacePresetId;
	TargetActor = SpawnedMetaHuman;
	AppliedSliders.Reset();
	AppliedHairStyle = AppliedEyebrows = AppliedFacialHair = NAME_None;
	return SpawnedMetaHuman != nullptr;
}

void UVaesenAppearanceComponent::ApplyAppearance()
{
	if (GetNetMode() == NM_DedicatedServer || !Appearance.IsSet())
	{
		return;
	}
	const bool bSpawned = UpdateMetaHuman();
	if (bSpawned)
	{
		// A freshly spawned MetaHuman sets its parts up over its first moments (its component loads animation classes
		// asynchronously); apply once more when that has settled.
		GetWorld()->GetTimerManager().SetTimer(SettleTimer, this, &UVaesenAppearanceComponent::ApplyAfterSettle, 1.5f, false);
	}
	ApplyGrooms();
	ApplySkinTone();
	if (USkeletalMeshComponent* Face = FindFace())
	{
		ApplyFace(Face);
		ApplySkinAge(Face);
		ApplyEyeColor(Face);
	}
	// After the grooms: a swapped groom has fresh materials.
	ApplyHairColor();
	if (bSpawned && SpawnedMetaHuman)
	{
		OnMetaHumanReady.Broadcast(SpawnedMetaHuman);
	}
}

void UVaesenAppearanceComponent::ApplyAfterSettle()
{
	AppliedHairStyle = AppliedEyebrows = AppliedFacialHair = NAME_None;
	ApplyAppearance();
	if (SpawnedMetaHuman)
	{
		OnMetaHumanReady.Broadcast(SpawnedMetaHuman);
	}
}

const FVaesenAppearancePresetRow* UVaesenAppearanceComponent::FindPreset(FName Id) const
{
	return PresetTable && !Id.IsNone() ? PresetTable->FindRow<FVaesenAppearancePresetRow>(Id, TEXT("Appearance"), /*bWarnIfRowMissing*/ false) : nullptr;
}

void UVaesenAppearanceComponent::ApplySkinTone()
{
	FLinearColor Scale, Offset, Ratio;
	if (!SkinToneTable || !FacePreset
		|| !SkinToneTable->ToneParameters(FacePreset->BaseSkinBias, FacePreset->BaseSkinGain, Appearance.SkinTone, Scale, Offset)
		|| !SkinToneTable->ToneRatioFor(FacePreset->BaseSkinBias, Appearance.SkinTone, Ratio))
	{
		return;
	}
	// Face and body share the skin material (M_skin_unified_baked, with the Vaesen tone parameters), so they match.
	TArray<USkeletalMeshComponent*> Meshes;
	if (AActor* Target = GetTarget())
	{
		Target->GetComponents(Meshes);
	}
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == FaceComponentName || Mesh->GetFName() == TEXT("Body"))
		{
			Mesh->SetVectorParameterValueOnMaterials(TEXT("VaesenToneScale"), FVector(Scale.R, Scale.G, Scale.B));
			Mesh->SetVectorParameterValueOnMaterials(TEXT("VaesenToneOffset"), FVector(Offset.R, Offset.G, Offset.B));
			Mesh->SetVectorParameterValueOnMaterials(TEXT("VaesenToneRatio"), FVector(Ratio.R, Ratio.G, Ratio.B));
		}
	}
}

USkeletalMeshComponent* UVaesenAppearanceComponent::FindFace() const
{
	TArray<USkeletalMeshComponent*> Meshes;
	if (AActor* Target = GetTarget())
	{
		Target->GetComponents(Meshes);
	}
	USkeletalMeshComponent* const* Found = Meshes.FindByPredicate([this](const USkeletalMeshComponent* Mesh) { return Mesh->GetFName() == FaceComponentName; });
	return Found ? *Found : nullptr;
}

void UVaesenAppearanceComponent::ApplyFace(USkeletalMeshComponent* Face)
{
	using namespace VaesenAppearance;

	// Sculpt sliders: a morph pair per slider (the face mesh carries them at every LOD), only one side weighted.
	for (const FName Old : AppliedSliders)
	{
		Face->SetMorphTarget(SliderMorph(Old, true), 0.f);
		Face->SetMorphTarget(SliderMorph(Old, false), 0.f);
	}
	AppliedSliders.Reset();
	// Each slider's value as its weight (times the face's strength for it); the morphs and joint offsets both use it.
	TArray<FVaesenFaceSliderValue> Weights;
	for (const FVaesenFaceSliderValue& Slider : Appearance.FaceSliders)
	{
		const float Weight = FacePreset ? FacePreset->SliderWeight(Slider.Name, Slider.Value) : Slider.Value;
		Face->SetMorphTarget(SliderMorph(Slider.Name, true), FMath::Max(Weight, 0.f));
		Face->SetMorphTarget(SliderMorph(Slider.Name, false), FMath::Max(-Weight, 0.f));
		AppliedSliders.Add(Slider.Name);
		Weights.Add({ Slider.Name, Weight });
	}

	// Age changes the skin only (ApplySkinAge), never the face's shape: the face's age morph stays off.
	Face->SetMorphTarget(SliderMorph(AgeSlider, true), 0.f);

	// Joint offsets of every slider (and age), summed, for the face's joint-offset node.
	if (FacePreset)
	{
		FAnimNode_VaesenFaceJointOffsets::SetOffsetsFor(Face, FacePreset->CombinedOffsets(Weights));
	}

	// Age wrinkles: a floor under the face rig's wrinkle-map curves.
	TMap<FName, float> Floors;
	if (Appearance.Age > 0.f)
	{
		for (const FName Curve : AgeWrinkleCurves)
		{
			Floors.Add(Curve, AgeWrinkleStrength * Appearance.Age);
		}
	}
	FAnimNode_VaesenFaceJointOffsets::SetCurveFloorsFor(Face, MoveTemp(Floors));
}

void UVaesenAppearanceComponent::ApplySkinAge(USkeletalMeshComponent* Face)
{
	// Below the face's own age the skin blends toward the young skin, above it toward the old one.
	const float Base = FacePreset ? FacePreset->BaseAge : 0.3f;
	const float Age = Appearance.Age;
	const float Young = Age < Base && Base > 0.f ? (Base - Age) / Base : 0.f;
	const float Old = Age > Base && Base < 1.f ? (Age - Base) / (1.f - Base) : 0.f;
	Face->SetScalarParameterValueOnMaterials(TEXT("VaesenAgeYoung"), Young * AgeSkinStrength);
	Face->SetScalarParameterValueOnMaterials(TEXT("VaesenAgeOld"), Old * AgeSkinStrength);
	UTexture* const YoungNormal = FacePreset ? FacePreset->AgeYoungNormal.LoadSynchronous() : nullptr;
	UTexture* const OldNormal = FacePreset ? FacePreset->AgeOldNormal.LoadSynchronous() : nullptr;
	UTexture* const YoungColor = FacePreset ? FacePreset->AgeYoungColor.LoadSynchronous() : nullptr;
	UTexture* const OldColor = FacePreset ? FacePreset->AgeOldColor.LoadSynchronous() : nullptr;
	for (int32 Index = 0; Index < Face->GetNumMaterials(); ++Index)
	{
		if (UMaterialInstanceDynamic* const Material = Cast<UMaterialInstanceDynamic>(Face->GetMaterial(Index)))
		{
			if (YoungNormal)
			{
				Material->SetTextureParameterValue(TEXT("VaesenAgeYoungNormal"), YoungNormal);
			}
			if (OldNormal)
			{
				Material->SetTextureParameterValue(TEXT("VaesenAgeOldNormal"), OldNormal);
			}
			if (YoungColor)
			{
				Material->SetTextureParameterValue(TEXT("VaesenAgeYoungColor"), YoungColor);
			}
			if (OldColor)
			{
				Material->SetTextureParameterValue(TEXT("VaesenAgeOldColor"), OldColor);
			}
		}
	}
}

void UVaesenAppearanceComponent::ApplyEyeColor(USkeletalMeshComponent* Face)
{
	const FVaesenAppearancePresetRow* const Row = FindPreset(Appearance.EyeColorId);
	if (!Row || Row->Kind != EVaesenAppearancePresetKind::EyeColor)
	{
		return;
	}
	// The built eyes use a baked iris texture (their colour parameters are switched off), so the row's iris texture
	// replaces it (MetaHuman irises share one layout).
	UTexture* const Iris = Cast<UTexture>(Row->Asset.LoadSynchronous());
	if (!Iris)
	{
		return;
	}
	for (int32 Index = 0; Index < Face->GetNumMaterials(); ++Index)
	{
		if (Face->GetMaterialSlotNames()[Index].ToString().StartsWith(TEXT("eyeLeft")) || Face->GetMaterialSlotNames()[Index].ToString().StartsWith(TEXT("eyeRight")))
		{
			if (UMaterialInstanceDynamic* const Material = Face->CreateDynamicMaterialInstance(Index))
			{
				Material->SetTextureParameterValue(TEXT("Iris Basecolor Baked"), Iris);
			}
		}
	}
}

void UVaesenAppearanceComponent::ApplyGrooms()
{
	AActor* const Target = GetTarget();
	if (!Target || !PresetTable)
	{
		return;
	}
	TArray<UGroomComponent*> Grooms;
	Target->GetComponents(Grooms);
	auto Find = [&Grooms](const TCHAR* Name) -> UGroomComponent*
	{
		UGroomComponent* const* Found = Grooms.FindByPredicate([Name](const UGroomComponent* Groom) { return Groom->GetFName() == Name; });
		return Found ? *Found : nullptr;
	};
	auto Set = [this](UGroomComponent* Component, UGroomAsset* Groom)
	{
		if (!Component || Component->GroomAsset == Groom)
		{
			return;
		}
		UGroomBindingAsset* const Binding = Groom && FacePreset ? FacePreset->FindBinding(Groom) : nullptr;
		if (Groom && !Binding)
		{
			UE_LOG(LogTemp, Warning, TEXT("Appearance: no binding of %s for %s; it won't follow the face"), *Groom->GetName(), *GetNameSafe(FacePreset));
		}
		// The groom's own materials (the MetaHuman Blueprint overrides them with the built groom's).
		Component->EmptyOverrideMaterials();
		Component->SetGroomAsset(Groom, Binding);
	};
	auto GroomOf = [](const FVaesenAppearancePresetRow* Row) { return Row ? Cast<UGroomAsset>(Row->Asset.LoadSynchronous()) : nullptr; };

	// Hairstyle: the row's groom; a row without one is bald.
	if (AppliedHairStyle != Appearance.HairStyleId)
	{
		if (const FVaesenAppearancePresetRow* const Row = FindPreset(Appearance.HairStyleId))
		{
			Set(Find(TEXT("Hair")), GroomOf(Row));
		}
		AppliedHairStyle = Appearance.HairStyleId;
	}
	// Eyebrows: the row's groom; a row without one keeps the face's own.
	if (AppliedEyebrows != Appearance.EyebrowStyleId)
	{
		if (UGroomAsset* const Groom = GroomOf(FindPreset(Appearance.EyebrowStyleId)))
		{
			Set(Find(TEXT("Eyebrows")), Groom);
		}
		AppliedEyebrows = Appearance.EyebrowStyleId;
	}
	// Facial hair: a moustache or a beard (by the groom's name) on its component, the other cleared; none clears both.
	if (AppliedFacialHair != Appearance.FacialHairId || Appearance.FacialHairId.IsNone())
	{
		UGroomAsset* const Groom = GroomOf(FindPreset(Appearance.FacialHairId));
		const bool bMustache = Groom && Groom->GetName().Contains(TEXT("Mustache"));
		Set(Find(TEXT("Mustache")), bMustache ? Groom : nullptr);
		Set(Find(TEXT("Beard")), Groom && !bMustache ? Groom : nullptr);
		AppliedFacialHair = Appearance.FacialHairId;
	}
}

void UVaesenAppearanceComponent::ApplyHairColor()
{
	TArray<UGroomComponent*> Grooms;
	if (AActor* Target = GetTarget())
	{
		Target->GetComponents(Grooms);
	}
	for (UGroomComponent* Groom : Grooms)
	{
		// The MetaHuman hair material: natural colour (melanin, redness), white strands (its default 0.3 greys every
		// hairstyle), and a dye colour that multiplies the hair (white is undyed).
		Groom->SetScalarParameterValueOnMaterials(TEXT("hairMelanin"), Appearance.HairMelanin);
		Groom->SetScalarParameterValueOnMaterials(TEXT("hairRedness"), Appearance.HairRedness);
		Groom->SetScalarParameterValueOnMaterials(TEXT("WhiteAmount"), Appearance.HairGrey);
		const FLinearColor Dye = FMath::Lerp(FLinearColor::White, HairDyeColor, Appearance.HairDye);
		Groom->SetVectorParameterValueOnMaterials(TEXT("hairDye"), FVector(Dye.R, Dye.G, Dye.B));
	}
}
