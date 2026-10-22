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

    public CasePacket Packet { get; set; } = new();

    public List<GenerationLogEntry> Log { get; } = [];

    public string SystemPrompt { get; set; } = "";

    public string CaseContext { get; set; } = "";

    public ValidationContext ValidationContext => new()
    {
        Pack = Pack,
        Level = Level,
        Vaesen = [Vaesen],
        Conflict = Pack.FindConflict(Packet.Brief.ConflictArchetypeId),
    };

    /// <summary>A deep copy of the packet, for a step to work on without touching the draft until it passes.</summary>
    public CasePacket ClonePacket() => JsonConvert.DeserializeObject<CasePacket>(JsonConvert.SerializeObject(Packet))!;
}

// The stages a case passes through. Each step takes the stage before it and produces the next, and stages can only be
// made by the pipeline, so the order level -> creature -> conflict -> escalation -> humans -> clues is enforced by types.

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

    [JsonProperty("passed")]
    public bool Passed { get; set; }

    [JsonProperty("error")]
    public string? Error { get; set; }

    [JsonProperty("issues")]
    public List<CoherenceIssue> Issues { get; set; } = [];

    /// <summary>The model's raw answer, kept so a failing step can be inspected.</summary>
    [JsonProperty("output")]
    public string? Output { get; set; }
}
