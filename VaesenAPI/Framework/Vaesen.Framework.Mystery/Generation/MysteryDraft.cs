using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// A case under construction (spec 6.3): the brief, the canon it draws on, the packet as built so far and the log of
/// every model call and validator run.
/// </summary>
public sealed class MysteryDraft
{
    public required AssignmentBrief Brief { get; init; }

    public required ContentPack Pack { get; init; }

    public required SocietyHistory History { get; init; }

    public required LevelDefinition Level { get; init; }

    public required VaesenDefinition Vaesen { get; init; }

    /// <summary>The conflict archetypes on offer, best first.</summary>
    public required List<ConflictArchetype> Conflicts { get; init; }

    /// <summary>The Society's world (persistent cast and overlay); null when worlds are off.</summary>
    public SocietyWorld? World { get; init; }

    /// <summary>
    /// The level's public facts the Society's other cases there already explain (fixes 4, section 12.2): those played
    /// are in the overlay; queued ones are here too, so two cases do not explain one fact two ways.
    /// </summary>
    public List<SettledPublicFact> SettledPublicFacts { get; set; } = [];

    /// <summary>
    /// The Society's earlier cases with this creature or at this level, played or queued (review of case 09, problem
    /// 5): the plot steps are told what not to tell again, and the critic checks the case does not.
    /// </summary>
    public List<EarlierPlot> EarlierPlots { get; set; } = [];

    public CasePacket Packet { get; set; } = new();

    public List<GenerationLogEntry> Log { get; } = [];

    public string SystemPrompt { get; set; } = "";

    /// <summary>The whole case context (what the critic sees); each step gets its own part, <see cref="ContextFor"/>.</summary>
    public string CaseContext { get; set; } = "";

    /// <summary>Each step's last passed output by step name, so a fix can patch or revise it in place.</summary>
    public Dictionary<string, object> Outputs { get; } = new(StringComparer.Ordinal);

    /// <summary>The canon <paramref name="step"/> works from: the assignment and only the sections it needs.</summary>
    public string ContextFor(string step) => PromptBuilder.CaseContext(this, step);

    /// <summary>The checks the case has passed so far, step by step; the final run checks only these.</summary>
    public CheckScope Completed { get; set; }

    /// <summary>The step that gave up, when the case failed.</summary>
    public string? FailedStep { get; set; }

    /// <summary>Issues a later step attributed to an earlier one, which is re-run with them (fixes, section 2.3).</summary>
    public List<CoherenceIssue> ReworkFeedback { get; set; } = [];

    public ValidationContext ValidationContext => new()
    {
        Pack = Pack,
        Level = Level,
        Vaesen = [Vaesen],
        Conflict = Pack.FindConflict(Packet.Brief.ConflictArchetypeId),
        SocietyCharacterIds = World?.Characters.Select(c => c.Id).ToHashSet() ?? [],
        WorldNames = World?.NamesThatMatter(Level.Id).Select(c => (c.Id, c.Name)).ToList() ?? [],
        UnavailableCharacterIds = World?.Characters
            .Where(c => c.Status is CharacterStatuses.Dead or CharacterStatuses.Departed)
            .Select(c => c.Id)
            .ToHashSet() ?? [],
        Holders = World is null || World.RosterFor(Level.Id) is null
            ? []
            : World.PeopleOf(Level.Id)
                .Where(c => c.Status is CharacterStatuses.Alive or CharacterStatuses.Absent && c.HomePlaceId is not null && RosterRoles.Holds(c.RosterRole ?? ""))
                .GroupBy(c => c.HomePlaceId!, StringComparer.Ordinal)
                .ToDictionary(g => g.Key, g => (g.First().Id, g.First().Name), StringComparer.Ordinal),
        SettledPublicFacts = SettledPublicFacts,
        ParentChild = World?.Characters
            .SelectMany(c => c.Relationships.Where(r => CaseValidator.IsParentChild(r.Nature)).Select(r => ValidationContext.PairKey(c.Id, r.CharacterId)))
            .ToHashSet(StringComparer.Ordinal) ?? [],
    };

    /// <summary>A deep copy of the packet, for a step to work on without touching the draft until it passes.</summary>
    public CasePacket ClonePacket() => JsonConvert.DeserializeObject<CasePacket>(JsonConvert.SerializeObject(Packet))!;
}

// The stages a case passes through. Each step takes the stage before it and produces the next, and stages can only be
// made by the pipeline, so the order level -> creature -> conflict -> escalation -> humans -> facts -> minds -> arc ->
// hook is enforced by types.

public abstract class DraftStage
{
    private protected DraftStage(MysteryDraft draft) => Draft = draft;

    public MysteryDraft Draft { get; }
}

/// <summary>Level and creature chosen by the assignment (steps 1 and 2).</summary>
public sealed class AssignedStage : DraftStage
{
    internal AssignedStage(MysteryDraft draft) : base(draft) { }
}

public sealed class ConflictStage : DraftStage
{
    internal ConflictStage(MysteryDraft draft) : base(draft) { }
}

public sealed class EscalationStage : DraftStage
{
    internal EscalationStage(MysteryDraft draft) : base(draft) { }
}

public sealed class CastStage : DraftStage
{
    internal CastStage(MysteryDraft draft) : base(draft) { }
}

public sealed class FactsStage : DraftStage
{
    internal FactsStage(MysteryDraft draft) : base(draft) { }
}

public sealed class MindsStage : DraftStage
{
    internal MindsStage(MysteryDraft draft) : base(draft) { }
}

public sealed class ArcStage : DraftStage
{
    internal ArcStage(MysteryDraft draft) : base(draft) { }
}

/// <summary>Every step done: the hook is written and the case can be checked whole.</summary>
public sealed class CluesStage : DraftStage
{
    internal CluesStage(MysteryDraft draft) : base(draft) { }
}

/// <summary>One model call or validator run during generation, for the generation log (spec 7.10: every run is logged).</summary>
/// <summary>A level's public fact another of the Society's cases explains, and how.</summary>
public sealed record SettledPublicFact(string PublicFactId, string Fact, string Explanation, string MysteryId, bool Played);

/// <summary>An earlier case of the Society, as the plot memory tells it: what it was about, never how it was solved.</summary>
public sealed record EarlierPlot(string MysteryId, string Title, string VaesenId, string LevelId, int Year, int Month, string ConflictArchetypeId, string Summary, bool Played, PlotSignature? Signature = null);

public sealed class GenerationLogEntry
{
    [JsonProperty("at")]
    public DateTime At { get; set; } = DateTime.UtcNow;

    [JsonProperty("caseAttempt")]
    public int CaseAttempt { get; set; }

    [JsonProperty("step")]
    public string Step { get; set; } = "";

    /// <summary>One of <see cref="LogEntryKinds"/>: a full answer, a fresh restart, a patch of a few items, or a check.</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = LogEntryKinds.Full;

    [JsonProperty("attempt")]
    public int Attempt { get; set; }

    [JsonProperty("model")]
    public string? Model { get; set; }

    [JsonProperty("inputTokens")]
    public long InputTokens { get; set; }

    [JsonProperty("outputTokens")]
    public long OutputTokens { get; set; }

    [JsonProperty("cacheReadTokens")]
    public long CacheReadTokens { get; set; }

    [JsonProperty("cacheWriteTokens")]
    public long CacheWriteTokens { get; set; }

    [JsonProperty("latencyMs")]
    public long LatencyMs { get; set; }

    /// <summary>The call's cost in USD at list prices (<see cref="Ai.ModelPrices"/>).</summary>
    [JsonProperty("costUsd")]
    public double CostUsd { get; set; }

    /// <summary>A hash of the cached prefix; equal hashes on consecutive calls should mean cache reads.</summary>
    [JsonProperty("prefixHash", NullValueHandling = NullValueHandling.Ignore)]
    public string? PrefixHash { get; set; }

    [JsonProperty("passed")]
    public bool Passed { get; set; }

    [JsonProperty("error")]
    public string? Error { get; set; }

    [JsonProperty("issues")]
    public List<CoherenceIssue> Issues { get; set; } = [];

    /// <summary>The step's instruction as sent (the system prompt and case context are kept once, in the stored case).</summary>
    /// <summary>Why a call was what it was, when that needs saying (a whole-step revision rather than a patch).</summary>
    [JsonProperty("why", NullValueHandling = NullValueHandling.Ignore)]
    public string? Why { get; set; }

    [JsonProperty("instruction")]
    public string? Instruction { get; set; }

    /// <summary>The model's raw answer, kept so a failing step can be inspected.</summary>
    [JsonProperty("output")]
    public string? Output { get; set; }
}

public static class LogEntryKinds
{
    /// <summary>The whole step answered (a first try or a retry with its issues).</summary>
    public const string Full = "full";

    /// <summary>The step started again from nothing after giving up (fixes, section 2.3).</summary>
    public const string Fresh = "fresh";

    /// <summary>Only the items with problems were asked for again and merged back (fixes, section 2.2).</summary>
    public const string Patch = "patch";

    /// <summary>A validator run on the whole case, with no model call.</summary>
    public const string Check = "check";

    /// <summary>A passed step's answer revised in place for the critic's issues (fixes 2, section 1).</summary>
    public const string Fix = "fix";

    /// <summary>A person renamed after names existed; code replaced the old name everywhere (fixes 4, section 4).</summary>
    public const string Rename = "rename";

    /// <summary>Exact text replaced by code for a mechanical fix (review 2, item 2).</summary>
    public const string Replace = "replace";

    /// <summary>Taken from the Society's roster of the level, without a model call (fixes 4, section 12.1).</summary>
    public const string Roster = "roster";

    /// <summary>One fix broke a structural check and its repair did not hold, so that fix alone was undone (review 3, item 1).</summary>
    public const string Undo = "undo";
}
