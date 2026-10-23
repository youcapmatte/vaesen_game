using System.Globalization;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// What the engine works out itself instead of asking the model: which facts are deep lore or giveaway tells, and how
/// much each fact raises the tension. The model is never asked for, or blamed for, a value code can compute.
/// </summary>
public static class Derived
{
    /// <summary>A load-bearing fact raises the tension this many times as much as any other fact.</summary>
    public const double LoadBearingWeight = 2;

    /// <summary>How far past the climax threshold a complete solution takes the tension.</summary>
    public const double ClimaxMargin = 0.15;

    /// <summary>deepLore and giveaway, from the lore ids each fact rests on.</summary>
    public static void LoreFlags(IEnumerable<CaseFact> facts, VaesenDefinition creature)
    {
        var deep = creature.DeepLore.Select(l => l.Id).ToHashSet(StringComparer.Ordinal);
        var giveaway = creature.GiveawayTells.Select(g => g.Id).ToHashSet(StringComparer.Ordinal);
        foreach (var fact in facts)
        {
            fact.DeepLore = fact.LoreIds.Exists(deep.Contains);
            fact.Giveaway = fact.LoreIds.Exists(giveaway.Contains);
        }
    }

    /// <summary>
    /// Sets the fact weights so that the load-bearing facts, all of which a complete solution uncovers, take the dial
    /// from its start to <see cref="ClimaxMargin"/> past the climax, with other facts weighing less. Beat thresholds
    /// outside the range between the start and the climax are scaled into it. Returns a warning for each adjustment.
    /// </summary>
    public static List<CoherenceIssue> Tension(CasePacket packet)
    {
        var issues = new List<CoherenceIssue>();
        var inv = CultureInfo.InvariantCulture;
        var start = packet.Tension.Start;
        var climax = packet.Climax.Threshold;
        if (climax <= start + 0.2 || climax > 1)
        {
            var corrected = Math.Clamp(climax, start + 0.2, 1);
            issues.Add(CoherenceIssue.Warning(IssueCodes.Beats, "climax",
                string.Create(inv, $"The climax threshold {climax:0.00} was moved to {corrected:0.00}, inside the dial.")));
            climax = packet.Climax.Threshold = corrected;
        }

        var loadBearing = Math.Max(1, packet.Facts.Count(f => f.LoadBearing));
        var unit = (climax * (1 + ClimaxMargin) - start) / (LoadBearingWeight * loadBearing);
        packet.Tension.FactWeights = packet.Facts
            .GroupBy(f => f.Id, StringComparer.Ordinal)
            .ToDictionary(g => g.Key, g => Math.Round(unit * (g.First().LoadBearing ? LoadBearingWeight : 1), 4), StringComparer.Ordinal);

        var beats = packet.Beats;
        if (beats.Exists(b => b.Threshold <= start || b.Threshold >= climax))
        {
            var low = beats.Min(b => b.Threshold);
            var high = beats.Max(b => b.Threshold);
            var from = start + 0.1 * (climax - start);
            var to = start + 0.9 * (climax - start);
            foreach (var beat in beats)
            {
                var place = high > low ? (beat.Threshold - low) / (high - low) : 0.5;
                beat.Threshold = Math.Round(from + place * (to - from), 3);
            }

            issues.Add(CoherenceIssue.Warning(IssueCodes.Beats, "beats",
                string.Create(inv, $"Beat thresholds were scaled into {from:0.00}-{to:0.00}, between the start and the climax, keeping their order.")));
        }

        return issues;
    }
}
