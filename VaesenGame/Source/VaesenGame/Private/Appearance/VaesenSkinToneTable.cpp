#include "Appearance/VaesenSkinToneTable.h"

bool UVaesenSkinToneTable::Lookup(FVector2f UV, FVector3f& OutBias, FVector3f& OutGain) const
{
	const int32 N = Resolution;
	if (N < 2 || Bias.Num() != N * N || Gain.Num() != N * N)
	{
		return false;
	}
	const float X = FMath::Clamp(UV.X, 0.f, 1.f) * (N - 1);
	const float Y = FMath::Clamp(UV.Y, 0.f, 1.f) * (N - 1);
	const int32 X0 = FMath::Min(FMath::FloorToInt32(X), N - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt32(Y), N - 2);
	const float FX = X - X0;
	const float FY = Y - Y0;

	// Bilinear over the valid corners of the cell; with none valid, the nearest valid point anywhere.
	FVector3f B = FVector3f::ZeroVector, G = FVector3f::ZeroVector;
	float Total = 0.f;
	for (int32 DY = 0; DY < 2; ++DY)
	{
		for (int32 DX = 0; DX < 2; ++DX)
		{
			const int32 Index = (Y0 + DY) * N + (X0 + DX);
			if (Valid.IsValidIndex(Index) && !Valid[Index])
			{
				continue;
			}
			const float W = (DX ? FX : 1.f - FX) * (DY ? FY : 1.f - FY);
			B += Bias[Index] * W;
			G += Gain[Index] * W;
			Total += W;
		}
	}
	if (Total > KINDA_SMALL_NUMBER)
	{
		OutBias = B / Total;
		OutGain = G / Total;
		return true;
	}
	int32 Nearest = INDEX_NONE;
	float NearestSq = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < N * N; ++Index)
	{
		if (Valid.IsValidIndex(Index) && !Valid[Index])
		{
			continue;
		}
		const float Sq = FMath::Square(Index % N - X) + FMath::Square(Index / N - Y);
		if (Sq < NearestSq)
		{
			NearestSq = Sq;
			Nearest = Index;
		}
	}
	if (Nearest == INDEX_NONE)
	{
		return false;
	}
	OutBias = Bias[Nearest];
	OutGain = Gain[Nearest];
	return true;
}

FLinearColor UVaesenSkinToneTable::ToneRatio(FVector3f FromBias, FVector3f ToBias)
{
	auto Ratio = [](float To, float From) { return From > 0.f ? To / From : 1.f; };
	return FLinearColor(Ratio(ToBias.X, FromBias.X), Ratio(ToBias.Y, FromBias.Y), Ratio(ToBias.Z, FromBias.Z), 1.f);
}

bool UVaesenSkinToneTable::ToneRatioFor(FVector3f FromBias, FVector2f UV, FLinearColor& OutRatio) const
{
	FVector3f ToBias, ToGain;
	if (!Lookup(UV, ToBias, ToGain))
	{
		return false;
	}
	OutRatio = ToneRatio(FromBias, ToBias);
	return true;
}

bool UVaesenSkinToneTable::ToneParameters(FVector3f FromBias, FVector3f FromGain, FVector2f UV, FLinearColor& OutScale, FLinearColor& OutOffset) const
{
	FVector3f ToBias, ToGain;
	if (!Lookup(UV, ToBias, ToGain) || FromGain.GetMin() <= 0.f)
	{
		return false;
	}
	const FVector3f Scale = ToGain / FromGain;
	const FVector3f Offset = (ToBias - FromBias * Scale) / 255.f;
	OutScale = FLinearColor(Scale.X, Scale.Y, Scale.Z, 1.f);
	OutOffset = FLinearColor(Offset.X, Offset.Y, Offset.Z, 0.f);
	return true;
}
