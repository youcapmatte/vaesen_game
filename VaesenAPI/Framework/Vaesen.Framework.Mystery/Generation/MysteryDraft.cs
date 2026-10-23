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
        WorldNames = World?.Characters.Select(c => (c.Id, c.Name)).ToList() ?? [],
        UnavailableCharacterIds = World?.Characters
            .Where(c => c.Status is CharacterStatuses.Dead or CharacterStatuses.Departed)
            .Select(c => c.Id)
            .ToHashSet() ?? [],
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
}
