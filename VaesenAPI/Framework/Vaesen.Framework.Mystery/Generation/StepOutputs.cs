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

    [IdsOf(IdKinds.Place)]
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

    /// <summary>Signal that the brief's focus does not suit this creature here: only the focus is changed.</summary>
    [JsonProperty("focusUnsuitable")]
    public bool FocusUnsuitable { get; set; }

    [JsonProperty("focusUnsuitableReason")]
    public string? FocusUnsuitableReason { get; set; }

    /// <summary>The level's public facts this case explains or changes (fixes 4, section 12.2); usually none or one.</summary>
    [JsonProperty("publicFactsExplained")]
    public List<PublicFactNote> PublicFactsExplained { get; set; } = [];
}

public sealed class PublicFactNote
{
    [IdsOf(IdKinds.PublicFact)]
    [JsonProperty("publicFactId")]
    public string PublicFactId { get; set; } = "";

    /// <summary>What really lay behind it, or how it changes, in one or two sentences.</summary>
    [JsonProperty("explanation")]
    public string Explanation { get; set; } = "";
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

    [IdsOf(IdKinds.Place)]
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

    /// <summary>Named people in the story who never appear: relatives, the dead, the far away.</summary>
    [JsonProperty("offstagePeople")]
    public List<OffstagePerson> OffstagePeople { get; set; } = [];
}

/// <summary>The residents step: ordinary people of the place, written short on the small model (fixes 4, section 10).</summary>
public sealed class ResidentsOutput
{
    [JsonProperty("residents")]
    public List<CastDraft> Residents { get; set; } = [];
}

public sealed class CastDraft
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>A short handle for a significant character ("the widow"); null for residents.</summary>
    [JsonProperty("handle")]
    public string? Handle { get; set; }

    [JsonProperty("instanceRef")]
    public string InstanceRef { get; set; } = InstanceRefs.New;

    [JsonProperty("plotRole")]
    public string PlotRole { get; set; } = "";

    [OneOf(typeof(CastImportance))]
    [JsonProperty("importance")]
    public string Importance { get; set; } = CastImportance.Significant;

    [JsonProperty("roleSlotId")]
    public string? RoleSlotId { get; set; }

    [JsonProperty("identity")]
    public CharacterIdentity? Identity { get; set; }

    [IdsOf(IdKinds.Place)]
    [JsonProperty("homePlaceId")]
    public string? HomePlaceId { get; set; }

    /// <summary>The place of the level this person keeps or owns as its head (the inn they keep, the farm they own), if any.</summary>
    [IdsOf(IdKinds.Place)]
    [JsonProperty("keeps")]
    public string? Keeps { get; set; }

    /// <summary>The dwelling in their home place where they live (a cottage, the barracks, a farmhouse kitchen), if any.</summary>
    [IdsOf(IdKinds.Affordance)]
    [JsonProperty("homeAffordanceId")]
    public string? HomeAffordanceId { get; set; }

    /// <summary>A significant person whose home place has no dwelling for them: the engine builds them one.</summary>
    [JsonProperty("newDwelling")]
    public bool NewDwelling { get; set; }

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

// Step 6, clues, runs as four sub-steps, each small enough for the API to enforce its schema: facts (6a), minds and
// channels (6b), the arc (6c), and the hook and scenes (6d). Values the engine can work out itself (whether a fact
// is deep lore or a giveaway, how much each fact raises the tension) are not asked for.

/// <summary>Sub-step 6a, facts: every fact of the case, the decoy threads, and which facts each escalation event is.</summary>
public sealed class FactsOutput
{
    [JsonProperty("facts")]
    public List<FactDraft> Facts { get; set; } = [];

    [JsonProperty("decoys")]
    public List<DecoyThread> Decoys { get; set; } = [];

    /// <summary>Which facts each escalation event is.</summary>
    [JsonProperty("eventFacts")]
    public List<EventFacts> EventFacts { get; set; } = [];

    [JsonProperty("seasonThreads")]
    public List<SeasonThreadLink> SeasonThreads { get; set; } = [];
}

/// <summary>A fact as the model writes it; deepLore and giveaway are derived from its lore ids.</summary>
public sealed class FactDraft
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("statement")]
    public string Statement { get; set; } = "";

    [JsonProperty("loadBearing")]
    public bool LoadBearing { get; set; }

    [OneOf(typeof(FactKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = FactKinds.Symptom;

    /// <summary>"trueAnswer", "neutral" or "decoy:{threadId}".</summary>
    [JsonProperty("pointsTo")]
    public string PointsTo { get; set; } = FactPointsTo.Neutral;

    [JsonProperty("loreIds")]
    public List<string> LoreIds { get; set; } = [];
}

public sealed class EventFacts
{
    [IdsOf(IdKinds.Event)]
    [JsonProperty("eventId")]
    public string EventId { get; set; } = "";

    [JsonProperty("factIds")]
    public List<string> FactIds { get; set; } = [];
}

/// <summary>Sub-step 6b, minds and channels: who knows what, and every way into each fact.</summary>
public sealed class MindsOutput
{
    /// <summary>What each cast member knows, thinks and lies about.</summary>
    [JsonProperty("minds")]
    public List<CastMind> Minds { get; set; } = [];

    [JsonProperty("channels")]
    public List<CaseChannel> Channels { get; set; } = [];
}

public sealed class CastMind
{
    [IdsOf(IdKinds.Cast)]
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("knows")]
    public List<CastKnowledge> Knows { get; set; } = [];

    [JsonProperty("thinks")]
    public List<CastBelief> Thinks { get; set; } = [];

    [JsonProperty("lies")]
    public List<CastLie> Lies { get; set; } = [];
}

/// <summary>Sub-step 6c, the arc: beats, the climax and the resolution levers. The engine sets the fact weights.</summary>
public sealed class ArcOutput
{
    [JsonProperty("beats")]
    public List<CaseBeat> Beats { get; set; } = [];

    [JsonProperty("climax")]
    public CaseClimax Climax { get; set; } = new();

    [JsonProperty("resolutionLevers")]
    public List<ResolutionLever> ResolutionLevers { get; set; } = [];
}

/// <summary>
/// The creature's mind, a step of its own after the arc: with the arc's levers and deeds in one schema the grammar is
/// too large for structured outputs.
/// </summary>
public sealed class CreatureOutput
{
    [JsonProperty("creatureMinds")]
    public List<CreatureMind> CreatureMinds { get; set; } = [];
}

/// <summary>A verifying look at fixes (review 2, item 1): for each issue, whether the problem is now gone.</summary>
public sealed class VerifyOutput
{
    [JsonProperty("results")]
    public List<VerifyResult> Results { get; set; } = [];
}

public sealed class VerifyResult
{
    /// <summary>The issue's number as listed (1, 2, ...).</summary>
    [JsonProperty("issue")]
    public int Issue { get; set; }

    [JsonProperty("resolved")]
    public bool Resolved { get; set; }

    /// <summary>When not resolved: what still stands, item by item, in a sentence.</summary>
    [JsonProperty("remaining")]
    public string Remaining { get; set; } = "";
}

/// <summary>Sub-step 6d, the hook and the scenes.</summary>
public sealed class HookOutput
{
    [JsonProperty("hook")]
    public HookDraft Hook { get; set; } = new();

    [IdsOf(IdKinds.Place)]
    [JsonProperty("placesUsed")]
    public List<string> PlacesUsed { get; set; } = [];

    [JsonProperty("sceneDescriptions")]
    public List<SceneDescription> SceneDescriptions { get; set; } = [];
}

public sealed class HookDraft
{
    [IdsOf(IdKinds.Cast)]
    [JsonProperty("hookGiverId")]
    public string HookGiverId { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    [IdsOf(IdKinds.Fact)]
    [JsonProperty("assertedFactIds")]
    public List<string> AssertedFactIds { get; set; } = [];

    [IdsOf(IdKinds.Event)]
    [JsonProperty("openingEventIds")]
    public List<string> OpeningEventIds { get; set; } = [];
}

public sealed class SceneDescription
{
    [IdsOf(IdKinds.Place)]
    [JsonProperty("placeId")]
    public string PlaceId { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
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

    /// <summary>For a mechanical fix (a rename, a date, a wrong id): exact old text and new text; the engine applies them.</summary>
    [JsonProperty("replacements")]
    public List<TextReplacement> Replacements { get; set; } = [];

    /// <summary>The replacements hold everywhere in the case (a rename), not only in the targets.</summary>
    [JsonProperty("everywhere")]
    public bool Everywhere { get; set; }

    /// <summary>Every id the issue touches, so it is routed to the step that owns each (see the critic prompt).</summary>
    [JsonProperty("targets")]
    public List<string> Targets { get; set; } = [];
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

    /// <summary>Facts, what people know and when, their secrets and the reveals agree on when things happened.</summary>
    public const string Timeline = "timeline";

    /// <summary>Sabotage, traps, accidents, wounds and evidence behave as they would in the period (fixes 3, section 3).</summary>
    public const string Mechanisms = "mechanisms";

    /// <summary>A character acting against their own plan has a stated reason (fixes 3, section 4).</summary>
    public const string ActsToPlan = "actsToPlan";

    /// <summary>Threats and bargains say why the threatened party believes them (fixes 3, section 5).</summary>
    public const string Leverage = "leverage";

    /// <summary>No mind believes against what it knows without a reason (fixes 4, section 8).</summary>
    public const string MindsAgree = "mindsAgree";

    /// <summary>People's actions, secrets, lies and beliefs agree with the facts (the review of case 05).</summary>
    public const string NpcRecords = "npcRecords";

    public static readonly IReadOnlyList<string> All =
        [DeepLoreTrail, NonTrivial, KnowledgeGate, HumansFromConflict, DifficultyOnFocus, Hook, FairClues, HonestRedHerrings, Solves, WorldConsistency, Timeline, Mechanisms, ActsToPlan, Leverage, MindsAgree, NpcRecords];
}
