using System.Globalization;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// What the engine works out itself instead of asking the model: which facts are deep lore or giveaway tells, and how
/// much each fact raises the tension. The model is never asked for, or blamed for, a value code can compute.
/// </summary>
public static class Derived
{
    /// <summary>A load-bearing fact raises the tension this many times as much as an ordinary fact.</summary>
    public const double LoadBearingWeight = 2;

    /// <summary>A decoy fact raises it half as much: following a false trail is not progress.</summary>
    public const double DecoyWeight = 0.5;

    /// <summary>How far past the climax threshold the deep solve's path takes the tension.</summary>
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
    /// Sets the fact weights (fixes 2, section 4). Colour and season facts, and what the group knows on arrival (the
    /// hook's facts and the opening events'), weigh nothing, so the dial starts at its start; decoy facts weigh half,
    /// load-bearing ones double. The weights are scaled so that the deep solve's path (its lever's facts and the facts
    /// needed to reach them) ends <see cref="ClimaxMargin"/> past the climax. Beat thresholds outside the range between
    /// the start and the climax are scaled into it. Returns a warning for each adjustment.
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

        var graph = new FactGraph(packet);
        var opening = graph.OpeningKnowledge();
        double Units(CaseFact f) =>
            f.Kind is FactKinds.Colour or FactKinds.Season || opening.Contains(f.Id) ? 0
            : FactPointsTo.IsDecoy(f.PointsTo) ? DecoyWeight
            : f.LoadBearing ? LoadBearingWeight
            : 1;

        var path = DeepPath(packet, graph, opening);
        var pathUnits = path.Select(packet.FindFact).OfType<CaseFact>().Sum(Units);
        if (pathUnits <= 0)
        {
            // No deep lever, or nothing on its path left to find: scale on the load-bearing facts instead.
            path = packet.Facts.Where(f => f.LoadBearing).Select(f => f.Id).ToList();
            pathUnits = path.Select(packet.FindFact).OfType<CaseFact>().Sum(Units);
        }

        var unit = pathUnits > 0 ? (climax * (1 + ClimaxMargin) - start) / pathUnits : 0;
        packet.Tension.FactWeights = packet.Facts
            .GroupBy(f => f.Id, StringComparer.Ordinal)
            .ToDictionary(g => g.Key, g => Math.Round(unit * Units(g.First()), 4), StringComparer.Ordinal);
        packet.Tension.DeepPath = path;
        packet.Tension.DeepPathTotal = Math.Round(start + path.Sum(id => packet.Tension.FactWeights.GetValueOrDefault(id)), 3);
        packet.Tension.Arrival = Math.Round(graph.Tension(opening), 3);

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

        foreach (var beat in beats.Where(b => b.Threshold <= packet.Tension.Arrival))
        {
            issues.Add(CoherenceIssue.Warning(IssueCodes.Beats, $"beat:{beat.Id}",
                string.Create(inv, $"This beat fires on arrival: its threshold {beat.Threshold:0.00} is reached before anything is found.")));
        }

        return issues;
    }

    /// <summary>
    /// The facts players must uncover for the deep solve: the deep lever's facts, and for each, the facts gating the
    /// easiest channel into it, and so on back to the opening. Facts already known on arrival are not on the path.
    /// </summary>
    private static List<string> DeepPath(CasePacket packet, FactGraph graph, HashSet<string> opening)
    {
        var lever = packet.ResolutionLevers.FirstOrDefault(l => l.SolveDepth == SolveDepths.Deep && l.RequiresFactIds.Count > 0);
        if (lever is null)
        {
            return [];
        }

        var path = new List<string>();
        var queue = new Queue<string>(lever.RequiresFactIds);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        while (queue.Count > 0)
        {
            var fact = queue.Dequeue();
            if (!seen.Add(fact) || opening.Contains(fact) || packet.FindFact(fact) is null)
            {
                continue;
            }

            path.Add(fact);
            var easiest = packet.Channels
                .Where(c => c.FactId == fact)
                .Select(c => graph.FactGates(c).Where(g => !opening.Contains(g)).ToList())
                .OrderBy(gates => gates.Count)
                .FirstOrDefault();
            foreach (var gate in easiest ?? [])
            {
                queue.Enqueue(gate);
            }
        }

        return path;
    }
}
