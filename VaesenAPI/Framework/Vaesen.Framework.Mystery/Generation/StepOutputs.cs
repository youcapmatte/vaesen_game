using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

// What each creative step asks the model for. These are shaped for structured outputs (no dictionaries, nothing the
// step should not decide yet) and are folded into the CasePacket by the step.

/// <summary>Step 3, conflict: the skeleton of the dispute, matching the creature's reason model.</summary>
public sealed class ConflictOutput
{
    /// <summary>A short working title for the case.</summary>
    [JsonProperty("title")]
    public string Title { get; set; } = "";

    /// <summary>One of the conflict archetype ids offered in the brief.</summary>
    [JsonProperty("conflictArchetypeId")]
    public string ConflictArchetypeId { get; set; } = "";

    /// <summary>The whole hidden truth in prose, for the engine only.</summary>
    [JsonProperty("truthSummary")]
    public string TruthSummary { get; set; } = "";

    [JsonProperty("provocation")]
    public string Provocation { get; set; } = "";

    [JsonProperty("provocationPlaceId")]
    public string ProvocationPlaceId { get; set; } = "";

    [JsonProperty("provocationAffordanceTag")]
    public string ProvocationAffordanceTag { get; set; } = "";

    /// <summary>For a hidden hand: who they are in one line and what they want. Null without one.</summary>
    [JsonProperty("hiddenHandMotive")]
    public string? HiddenHandMotive { get; set; }

    /// <summary>The obvious folklore reading of why it turned, used as a decoy. Null if none.</summary>
    [JsonProperty("decoyReading")]
    public string? DecoyReading { get; set; }

    /// <summary>Signal that nothing plausible fits this creature, level and conflict: the case is re-rolled.</summary>
    [JsonProperty("impossible")]
    public bool Impossible { get; set; }

    [JsonProperty("impossibleReason")]
    public string? ImpossibleReason { get; set; }
}

/// <summary>Step 4, escalation: what has concretely happened.</summary>
public sealed class EscalationOutput
{
    [JsonProperty("events")]
    public List<EscalationDraft> Events { get; set; } = [];
}

public sealed class EscalationDraft
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("when")]
    public string When { get; set; } = "";

    [JsonProperty("placeId")]
    public string? PlaceId { get; set; }

    [OneOf(typeof(SignRegisters))]
    [JsonProperty("register")]
    public string Register { get; set; } = "";

    [JsonProperty("opening")]
    public bool Opening { get; set; }
}

/// <summary>Step 5, humans: the people entangled in the conflict, and the location's ordinary residents.</summary>
public sealed class HumansOutput
{
    [JsonProperty("cast")]
    public List<CastDraft> Cast { get; set; } = [];

    /// <summary>For a hidden-hand case: the cast member behind it.</summary>
    [JsonProperty("hiddenHandCharacterId")]
    public string? HiddenHandCharacterId { get; set; }
}

public sealed class CastDraft
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("instanceRef")]
    public string InstanceRef { get; set; } = InstanceRefs.New;

    [JsonProperty("plotRole")]
    public string PlotRole { get; set; } = "";

    [OneOf(typeof(CastTiers))]
    [JsonProperty("tier")]
    public string Tier { get; set; } = CastTiers.Significant;

    [JsonProperty("roleSlotId")]
    public string? RoleSlotId { get; set; }

    [JsonProperty("identity")]
    public CharacterIdentity? Identity { get; set; }

    [JsonProperty("homePlaceId")]
    public string? HomePlaceId { get; set; }

    [JsonProperty("wants")]
    public CastWants Wants { get; set; } = new();

    [JsonProperty("secrets")]
    public List<string> Secrets { get; set; } = [];

    [JsonProperty("routine")]
    public string Routine { get; set; } = "";

    [JsonProperty("voiceNotes")]
    public string VoiceNotes { get; set; } = "";

    [JsonProperty("relationships")]
    public List<CastRelationship> Relationships { get; set; } = [];
}

/// <summary>Step 6, clues: the facts, their channels, what each mind holds, decoys, beats, resolution and the hook.</summary>
public sealed class CluesOutput
{
    [JsonProperty("facts")]
    public List<CaseFact> Facts { get; set; } = [];

    [JsonProperty("channels")]
    public List<CaseChannel> Channels { get; set; } = [];

    /// <summary>Which facts each escalation event is.</summary>
    [JsonProperty("eventFacts")]
    public List<EventFacts> EventFacts { get; set; } = [];

    /// <summary>What each cast member knows, thinks and lies about.</summary>
    [JsonProperty("minds")]
    public List<CastMind> Minds { get; set; } = [];

    [JsonProperty("decoys")]
    public List<DecoyThread> Decoys { get; set; } = [];

    [JsonProperty("beats")]
    public List<CaseBeat> Beats { get; set; } = [];

    [JsonProperty("climax")]
    public CaseClimax Climax { get; set; } = new();

    [JsonProperty("resolutionLevers")]
    public List<ResolutionLever> ResolutionLevers { get; set; } = [];

    [JsonProperty("hook")]
    public HookDraft Hook { get; set; } = new();

    [JsonProperty("placesUsed")]
    public List<string> PlacesUsed { get; set; } = [];

    [JsonProperty("sceneDescriptions")]
    public List<SceneDescription> SceneDescriptions { get; set; } = [];

    [JsonProperty("seasonThreads")]
    public List<SeasonThreadLink> SeasonThreads { get; set; } = [];

    [JsonProperty("tensionStart")]
    public double TensionStart { get; set; } = 0.1;

    [JsonProperty("factWeights")]
    public List<FactWeight> FactWeights { get; set; } = [];
}

public sealed class EventFacts
{
    [JsonProperty("eventId")]
    public string EventId { get; set; } = "";

    [JsonProperty("factIds")]
    public List<string> FactIds { get; set; } = [];
}

public sealed class CastMind
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("knows")]
    public List<CastKnowledge> Knows { get; set; } = [];

    [JsonProperty("thinks")]
    public List<CastBelief> Thinks { get; set; } = [];

    [JsonProperty("lies")]
    public List<CastLie> Lies { get; set; } = [];
}

public sealed class HookDraft
{
    [JsonProperty("hookGiverId")]
    public string HookGiverId { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    [JsonProperty("assertedFactIds")]
    public List<string> AssertedFactIds { get; set; } = [];

    [JsonProperty("openingEventIds")]
    public List<string> OpeningEventIds { get; set; } = [];
}

public sealed class SceneDescription
{
    [JsonProperty("placeId")]
    public string PlaceId { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}

public sealed class FactWeight
{
    [JsonProperty("factId")]
    public string FactId { get; set; } = "";

    [JsonProperty("weight")]
    public double Weight { get; set; }
}

/// <summary>The critic's judgement on the finished case (spec 7.10, judgement checks).</summary>
public sealed class CriticOutput
{
    [JsonProperty("checks")]
    public List<CriticCheck> Checks { get; set; } = [];

    /// <summary>Two or three sentences on the case as a whole.</summary>
    [JsonProperty("overall")]
    public string Overall { get; set; } = "";
}

public sealed class CriticCheck
{
    /// <summary>One of <see cref="CriticChecks"/>.</summary>
    [JsonProperty("check")]
    public string Check { get; set; } = "";

    [JsonProperty("passed")]
    public bool Passed { get; set; }

    /// <summary>"error" if the case must change, "warning" if it could be better.</summary>
    [JsonProperty("severity")]
    public string Severity { get; set; } = IssueSeverities.Error;

    [JsonProperty("location")]
    public string Location { get; set; } = "";

    [JsonProperty("explanation")]
    public string Explanation { get; set; } = "";

    [JsonProperty("suggestedFix")]
    public string? SuggestedFix { get; set; }
}

/// <summary>The judgement checks the critic answers, one each.</summary>
public static class CriticChecks
{
    public const string DeepLoreTrail = "deepLoreTrail";
    public const string NonTrivial = "nonTrivial";
    public const string KnowledgeGate = "knowledgeGate";
    public const string HumansFromConflict = "humansFromConflict";
    public const string DifficultyOnFocus = "difficultyOnFocus";
    public const string Hook = "hook";
    public const string FairClues = "fairClues";
    public const string HonestRedHerrings = "honestRedHerrings";
    public const string Solves = "solves";
    public const string WorldConsistency = "worldConsistency";

    public static readonly IReadOnlyList<string> All =
        [DeepLoreTrail, NonTrivial, KnowledgeGate, HumansFromConflict, DifficultyOnFocus, Hook, FairClues, HonestRedHerrings, Solves, WorldConsistency];
}
