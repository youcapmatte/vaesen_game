using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The assignment stage (spec 7.1) and the choice of plot centre and focus (7.2). Deterministic C#, no model:
/// the same pack, history, month and seed always give the same brief.
/// </summary>
public static class Assignment
{
    /// <summary>Picks a level, a creature, a fitting conflict, the case's shape and the hook for a new mystery.</summary>
    public static AssignmentBrief Assign(ContentPack pack, SocietyHistory history, int month, int seed, MysteryOptions options)
    {
        if (month is < 1 or > 12)
        {
            throw new ArgumentOutOfRangeException(nameof(month), "Month must be 1-12.");
        }

        var random = new Random(seed);
        var brief = new AssignmentBrief { SocietyId = history.SocietyId, Month = month, Seed = seed, RedHerringDensity = options.RedHerringDensity };

        var monthLevels = pack.Levels.Where(l => l.Month == month).ToList();
        var candidates = VaesenCandidates(pack, history, month, options, random, brief.Notes);
        var choice = Choose(pack, candidates, monthLevels, random);
        if (choice is null)
        {
            // Re-roll the level only when no eligible creature fits any level of the month.
            brief.Notes.Add($"No level for month {month} fits any eligible creature; levels of other months were allowed.");
            choice = Choose(pack, candidates, pack.Levels, random);
        }

        if (choice is null)
        {
            throw new InvalidOperationException(
                "No creature, level and conflict fit together. Add levels or conflicts that offer what the creatures require.");
        }

        var (vaesen, level, conflicts) = choice.Value;
        brief.LevelId = level.Id;
        brief.VaesenId = vaesen.Id;

        ChoosePlotCentre(vaesen, conflicts, random, brief);
        brief.ConflictArchetypeIds = conflicts
            .Where(c => c.ReasonKinds.Contains(brief.ReasonKind))
            .Select(c => c.Id)
            .ToList();

        ChooseHook(pack.Setting, history, random, brief);
        brief.PersistentCastIds = pack.Characters.Select(c => c.Id).ToList();
        return brief;
    }

    /// <summary>Creatures allowed this month, the never-faced first, the most recently faced last and on cooldown dropped.</summary>
    private static List<VaesenDefinition> VaesenCandidates(
        ContentPack pack, SocietyHistory history, int month, MysteryOptions options, Random random, List<string> notes)
    {
        var faced = history.Mysteries.SelectMany(m => m.VaesenIds).ToList();
        var recent = history.Mysteries.TakeLast(options.VaesenCooldown).SelectMany(m => m.VaesenIds).ToHashSet();

        var allowed = pack.Vaesen.Where(v => !v.ExcludedMonths.Contains(month)).ToList();
        var eligible = allowed.Where(v => !recent.Contains(v.Id)).ToList();
        if (eligible.Count == 0)
        {
            notes.Add("Every creature is on cooldown; the cooldown was ignored.");
            eligible = allowed;
        }

        // Never faced first (shuffled), then by how long ago they were last faced.
        return eligible
            .Select(v => (v, lastFaced: faced.LastIndexOf(v.Id), tiebreak: random.Next()))
            .OrderBy(x => x.lastFaced)
            .ThenBy(x => x.tiebreak)
            .Select(x => x.v)
            .ToList();
    }

    private static (VaesenDefinition, LevelDefinition, List<ConflictArchetype>)? Choose(
        ContentPack pack, List<VaesenDefinition> candidates, List<LevelDefinition> levels, Random random)
    {
        foreach (var vaesen in candidates)
        {
            var fitting = levels
                .Where(l => ContentValidator.LevelFits(l, vaesen, pack.Vocabulary))
                .Select(l => (level: l, conflicts: FittingConflicts(pack, vaesen, l)))
                .Where(x => x.conflicts.Count > 0)
                .ToList();
            if (fitting.Count > 0)
            {
                var pick = fitting[random.Next(fitting.Count)];
                return (vaesen, pick.level, pick.conflicts);
            }
        }

        return null;
    }

    /// <summary>Conflicts that suit the creature and whose provocation can physically exist in the level.</summary>
    public static List<ConflictArchetype> FittingConflicts(ContentPack pack, VaesenDefinition vaesen, LevelDefinition level)
    {
        var offered = pack.Vocabulary.ExpandAll(level.AllTags());
        return pack.Conflicts
            .Where(c => c.Tags.Intersect(vaesen.CompatibleConflictTags).Any())
            .Where(c => c.ReasonKinds.Intersect(vaesen.ReasonKinds).Any())
            .Where(c => c.ProvocationAffordances.Count == 0 || c.ProvocationAffordances.Any(offered.Contains))
            .OrderByDescending(c => c.Tags.Intersect(vaesen.CompatibleConflictTags).Count())
            .ThenBy(c => c.Id, StringComparer.Ordinal)
            .ToList();
    }

    /// <summary>
    /// Spec 7.2: the reason kind (and for afflictions the origin), whether a hidden hand is present, the plot centre, and
    /// the focus. With a hidden hand the difficulty goes into the hand and the why; without one, into identity.
    /// </summary>
    private static void ChoosePlotCentre(VaesenDefinition vaesen, List<ConflictArchetype> conflicts, Random random, AssignmentBrief brief)
    {
        var kinds = vaesen.ReasonKinds.Where(k => conflicts.Exists(c => c.ReasonKinds.Contains(k))).ToList();
        brief.ReasonKind = kinds[random.Next(kinds.Count)];

        if (brief.ReasonKind == ReasonKinds.Affliction && vaesen.OriginSet.Count > 0)
        {
            var origin = vaesen.OriginSet[random.Next(vaesen.OriginSet.Count)];
            brief.OriginId = origin.Id;
            brief.HiddenHand = origin.HiddenHand;
            brief.HandDisposition = origin.HiddenHand ? origin.HandDisposition : null;
        }
        else
        {
            // A bargain needs someone who struck it; other reasons may still have a person behind them, less often.
            brief.HiddenHand = brief.ReasonKind == ReasonKinds.BargainInvited || random.NextDouble() < 0.25;
            if (brief.HiddenHand)
            {
                brief.HandDisposition = HandDispositions.All[random.Next(HandDispositions.All.Count)];
            }
        }

        brief.PlotCentreKind = brief.HiddenHand
            ? PlotCentreKinds.HiddenHand
            : brief.ReasonKind is ReasonKinds.SelfOriginatingGrievance or ReasonKinds.None or ReasonKinds.Affliction
                ? PlotCentreKinds.Nature
                : PlotCentreKinds.Provocation;

        var roll = random.NextDouble();
        brief.Focus = brief.HiddenHand
            ? roll < 0.75 ? CaseFocus.WhoWhy : CaseFocus.How
            : roll < 0.7 ? CaseFocus.What : CaseFocus.How;
    }

    /// <summary>A weighted hook channel and a hook-giver type, both kept off what the Society saw last.</summary>
    private static void ChooseHook(Setting setting, SocietyHistory history, Random random, AssignmentBrief brief)
    {
        var lastChannel = history.Mysteries.LastOrDefault()?.HookChannelId;
        var channels = setting.HookChannels.Where(c => c.Id != lastChannel).ToList();
        if (channels.Count == 0)
        {
            channels = setting.HookChannels;
        }

        brief.HookChannelId = channels.Count == 0 ? "" : WeightedPick(channels, c => c.Weight, random).Id;

        var recentGivers = history.Mysteries.TakeLast(2).Select(m => m.HookGiverType).ToHashSet();
        var givers = setting.HookGiverTypes.Where(g => !recentGivers.Contains(g)).ToList();
        if (givers.Count == 0)
        {
            givers = setting.HookGiverTypes;
        }

        brief.HookGiverType = givers.Count == 0 ? "" : givers[random.Next(givers.Count)];
    }

    private static T WeightedPick<T>(IReadOnlyList<T> items, Func<T, double> weight, Random random)
    {
        var total = items.Sum(i => Math.Max(0, weight(i)));
        var target = random.NextDouble() * total;
        foreach (var item in items)
        {
            target -= Math.Max(0, weight(item));
            if (target <= 0)
            {
                return item;
            }
        }

        return items[^1];
    }
}
