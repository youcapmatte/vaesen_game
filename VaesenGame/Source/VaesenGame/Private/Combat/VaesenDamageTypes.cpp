#include "Combat/VaesenDamageTypes.h"

FString FVaesenDamageReport::ToString() const
{
	TArray<FString> Parts;
	for (const FVaesenDamageLine& Line : Lines)
	{
		Parts.Add(FString::Printf(TEXT("%s %.1f -> prot %.1f -> %.1f -> block -%.1f -> %.1f"),
			*Line.DamageType.ToString(), Line.Raw, Line.Protection, Line.AfterProtection, Line.Absorbed, Line.Final));
	}
	return FString::Printf(TEXT("[%s] %s = %.1f"), *UEnum::GetValueAsString(Outcome), *FString::Join(Parts, TEXT("; ")), TotalFinal);
}

namespace VaesenDamageMath
{
	FVaesenDamageReport Resolve(const FVaesenDamagePacket& Packet, TFunctionRef<float(const FGameplayTag&)> GetProtection,
		const FVaesenBlockProfile* Block, bool bParried)
	{
		FVaesenDamageReport Report;
		Report.Outcome = bParried ? EVaesenHitOutcome::Parried : Block ? EVaesenHitOutcome::Blocked : EVaesenHitOutcome::Hit;

		for (const FVaesenDamageEntry& Entry : Packet.Entries)
		{
			if (!Entry.DamageType.IsValid() || Entry.Amount <= 0.0f)
			{
				continue;
			}

			FVaesenDamageLine& Line = Report.Lines.AddDefaulted_GetRef();
			Line.DamageType = Entry.DamageType;
			Line.Raw = Entry.Amount;
			Line.Protection = GetProtection(Entry.DamageType);
			Line.AfterProtection = FMath::Max(0.0f, Entry.Amount - Line.Protection);

			if (bParried)
			{
				// Negated, and it costs the parrying weapon nothing.
				Line.Final = 0.0f;
			}
			else
			{
				Line.Absorbed = Block ? Line.AfterProtection * Block->GetReduction(Entry.DamageType) : 0.0f;
				Line.Final = Line.AfterProtection - Line.Absorbed;
			}
			Report.TotalFinal += Line.Final;
			Report.TotalAbsorbed += Line.Absorbed;
		}
		return Report;
	}

	bool IsInFront(const FVector& BlockerLocation, const FVector& BlockerForward, const FVector& Source, float HalfAngleDegrees)
	{
		const FVector ToSource = (Source - BlockerLocation).GetSafeNormal2D();
		if (ToSource.IsNearlyZero())
		{
			// Right on top of the blocker: count it as in front.
			return true;
		}
		const float CosAngle = FVector::DotProduct(BlockerForward.GetSafeNormal2D(), ToSource);
		return CosAngle >= FMath::Cos(FMath::DegreesToRadians(HalfAngleDegrees));
	}
}
