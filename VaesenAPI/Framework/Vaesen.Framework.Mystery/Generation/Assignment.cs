using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The assignment stage (spec 7.1) and the choice of plot centre and focus (7.2). Deterministic C#, no model:
/// the same pack, history, month and seed always give the same brief.
/// </summary>
public static class Assignment
{
    /// <summary>
    /// Picks the in-world date, a level, a creature, a fitting conflict, the case's shape and the hook for a new mystery.
    /// The date comes after the Society's last case (fixes 4, section 11); <paramref name="month"/> pins its month,
    /// otherwise the calendar moves on to the first month with a level and creature that fit. A level the Society used
    /// recently, queued cases included, is on cooldown unless <paramref name="returnLevelId"/> asks to go back to it.
    /// </summary>
    public static AssignmentBrief Assign(ContentPack pack, SocietyHistory history, int? month, int seed, MysteryOptions options, string? returnLevelId = null)
    {
        if (month is < 1 or > 12)
        {
            throw new ArgumentOutOfRangeException(nameof(month), "Month must be 1-12.");
        }

        var random = new Random(seed);
        var brief = new AssignmentBrief { SocietyId = history.SocietyId, Seed = seed, RedHerringDensity = options.RedHerringDensity };
        var dates = Dates(pack.Setting, history, month, options);

        List<LevelDefinition> levels;
        if (returnLevelId is not null)
        {
            var back = pack.FindLevel(returnLevelId)
                ?? throw new InvalidOperationException($"There is no level '{returnLevelId}' to return to.");
            levels = [back];
            brief.ReturnVisit = true;
            brief.PreviousMysteryIds = history.Mysteries.Where(m => m.LevelId == back.Id).Select(m => m.MysteryId).ToList();
        }
        else
        {
            levels = pack.Levels;
        }

        (VaesenDefinition, LevelDefinition, List<ConflictArchetype>)? choice = null;
        var date = dates[0];
        foreach (var candidate in dates)
        {
            var notes = new List<string>();
            var vaesen = VaesenCandidates(pack, history, candidate.Month, options, random, notes);
            var open = levels.Where(l => l.Months.Contains(candidate.Month) && (brief.ReturnVisit || LevelCooldown(l.Id, history, candidate, pack.Setting, options) is null)).ToList();
            choice = Choose(pack, vaesen, open, random);
            if (choice is not null)
            {
                date = candidate;
                brief.Notes.AddRange(notes);
                break;
            }
        }

        if (choice is null)
        {
            // Relax what is relaxed least harmfully first: the level's cooldown, then the month the level is written for.
            var candidates = VaesenCandidates(pack, history, date.Month, options, random, brief.Notes);
            var monthLevels = levels.Where(l => l.Months.Contains(date.Month)).ToList();
            choice = Choose(pack, candidates, monthLevels, random);
            if (choice is not null)
            {
                brief.Notes.Add($"Every level for {date} that fits is on cooldown; the cooldown was ignored.");
            }
            else
            {
                brief.Notes.Add($"No level for {date} fits any eligible creature; levels of other months were allowed.");
                choice = Choose(pack, candidates, levels, random);
            }
        }

        if (choice is null)
        {
            throw new InvalidOperationException(brief.ReturnVisit
                ? $"No eligible creature and conflict fit a return to {levels[0].Name}."
                : "No creature, level and conflict fit together. Add levels or conflicts that offer what the creatures require.");
        }

        var (chosenVaesen, level, conflicts) = choice.Value;
        brief.Year = date.Year;
        brief.Month = date.Month;
        brief.LevelId = level.Id;
        brief.VaesenId = chosenVaesen.Id;

        ChoosePlotCentre(chosenVaesen, conflicts, random, brief);
        brief.ConflictArchetypeIds = conflicts
            .Where(c => c.ReasonKinds.Contains(brief.ReasonKind))
            .Select(c => c.Id)
            .ToList();

        ChooseHook(pack.Setting, history, random, brief);
        brief.PersistentCastIds = pack.Characters.Select(c => c.Id).ToList();
        return brief;
    }

    /// <summary>When a past case is set: its own date, or for cases from before the calendar the setting's start year.</summary>
    public static InWorldDate DateOf(PastMystery m, Setting setting) => new(m.Year > 0 ? m.Year : setting.StartYear, m.Month is >= 1 and <= 12 ? m.Month : 1);

    /// <summary>
    /// The dates a new case may be set on, earliest first: the month after the Society's latest case (or the setting's
    /// start), then onwards; with a month pinned, only the first date in that month.
    /// </summary>
    public static List<InWorldDate> Dates(Setting setting, SocietyHistory history, int? month, MysteryOptions options)
    {
        var latest = history.Mysteries.Count == 0 ? (InWorldDate?)null : history.Mysteries.Select(m => DateOf(m, setting)).Max();
        var earliest = latest?.AddMonths(Math.Max(1, options.MonthsBetweenCases))
            ?? new InWorldDate(setting.StartYear > 0 ? setting.StartYear : 1870, month ?? options.DefaultMonth);
        if (month is not null)
        {
            var date = new InWorldDate(earliest.Year, month.Value);
            return [date < earliest ? date.AddMonths(12) : date];
        }

        return Enumerable.Range(0, Math.Max(1, options.MonthsToSearch)).Select(earliest.AddMonths).ToList();
    }

    /// <summary>
    /// Why a level is on cooldown for a case on <paramref name="date"/>, or null: after a Society's case on a level,
    /// queued or played, it needs that many other finished mysteries and that many in-world months before the next.
    /// </summary>
    public static string? LevelCooldown(string levelId, SocietyHistory history, InWorldDate date, Setting setting, MysteryOptions options)
    {
        var last = history.Mysteries.FindLastIndex(m => m.LevelId == levelId);
        if (last < 0)
        {
            return null;
        }

        var finished = history.Mysteries.Skip(last + 1).Count(m => m.Finished);
        var months = date.Index - DateOf(history.Mysteries[last], setting).Index;
        return finished >= options.LevelCooldownMysteries && months >= options.LevelCooldownMonths
            ? null
            : $"used by {history.Mysteries[last].MysteryId}; {finished} mysteries finished and {months} months passed since";
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
        var preferred = brief.HiddenHand
            ? roll < 0.75 ? CaseFocus.WhoWhy : CaseFocus.How
            : roll < 0.7 ? CaseFocus.What : CaseFocus.How;
        brief.Focus = SupportedFocus(vaesen, preferred, brief.HiddenHand);
    }

    /// <summary>
    /// The preferred focus if the creature supports it, otherwise the nearest it does: with a hidden hand whoWhy, else
    /// how, else what it lists first.
    /// </summary>
    public static string SupportedFocus(VaesenDefinition vaesen, string preferred, bool hiddenHand, IReadOnlyCollection<string>? exclude = null)
    {
        var supported = (vaesen.SupportedFoci.Count == 0 ? CaseFocus.All : vaesen.SupportedFoci)
            .Where(f => exclude is null || !exclude.Contains(f))
            .ToList();
        if (supported.Count == 0)
        {
            return preferred;
        }

        if (supported.Contains(preferred))
        {
            return preferred;
        }

        var order = hiddenHand ? new[] { CaseFocus.WhoWhy, CaseFocus.How, CaseFocus.What } : [CaseFocus.How, CaseFocus.WhoWhy, CaseFocus.What];
        return order.First(supported.Contains);
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

        // A channel that only one kind of giver can carry takes that kind; the others are drawn from the rest.
        if (setting.FindHookChannel(brief.HookChannelId)?.GiverType is { Length: > 0 } only)
        {
            brief.HookGiverType = only;
            return;
        }

        var reserved = setting.HookChannels.Select(c => c.GiverType).Where(t => t.Length > 0).ToHashSet();
        var recentGivers = history.Mysteries.TakeLast(2).Select(m => m.HookGiverType).ToHashSet();
        var all = setting.HookGiverTypes.Select(g => g.Id).Where(g => !reserved.Contains(g)).ToList();
        var givers = all.Where(g => !recentGivers.Contains(g)).ToList();
        if (givers.Count == 0)
        {
            givers = all;
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
