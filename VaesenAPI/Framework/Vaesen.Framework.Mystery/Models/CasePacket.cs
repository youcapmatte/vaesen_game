using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// The hidden GM file of one mystery (spec section 8): the output of generation. Facts and channels are the backbone:
/// the fact graph is built from them, NPC knowledge points at them, the ledger records them as discovered and logs cite
/// them. Players must never be able to read this; the runtime only ever shows them channel reveals.
/// </summary>
public sealed class CasePacket
{
    public const int CurrentSchemaVersion = 1;

    [JsonProperty("schemaVersion")]
    public int SchemaVersion { get; set; } = CurrentSchemaVersion;

    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("seasonId")]
    public string? SeasonId { get; set; }

    [JsonProperty("title")]
    public string Title { get; set; } = "";

    [JsonProperty("generatedAt")]
    public DateTime GeneratedAt { get; set; }

    [JsonProperty("models")]
    public CaseModels Models { get; set; } = new();

    [JsonProperty("brief")]
    public CaseBrief Brief { get; set; } = new();

    [JsonProperty("truth")]
    public CaseTruth Truth { get; set; } = new();

    [JsonProperty("hook")]
    public CaseHook Hook { get; set; } = new();

    [JsonProperty("level")]
    public CaseLevel Level { get; set; } = new();

    [JsonProperty("facts")]
    public List<CaseFact> Facts { get; set; } = [];

    [JsonProperty("channels")]
    public List<CaseChannel> Channels { get; set; } = [];

    [JsonProperty("cast")]
    public List<CastMember> Cast { get; set; } = [];

    [JsonProperty("decoys")]
    public List<DecoyThread> Decoys { get; set; } = [];

    [JsonProperty("beats")]
    public List<CaseBeat> Beats { get; set; } = [];

    [JsonProperty("climax")]
    public CaseClimax Climax { get; set; } = new();

    [JsonProperty("resolutionLevers")]
    public List<ResolutionLever> ResolutionLevers { get; set; } = [];

    [JsonProperty("seasonThreads")]
    public List<SeasonThreadLink> SeasonThreads { get; set; } = [];

    [JsonProperty("tension")]
    public CaseTension Tension { get; set; } = new();

    [JsonProperty("validation")]
    public CaseValidation Validation { get; set; } = new();

    public CaseFact? FindFact(string? id) => Facts.Find(f => f.Id == id);

    public CaseChannel? FindChannel(string? id) => Channels.Find(c => c.Id == id);

    public CastMember? FindCast(string? characterId) => Cast.Find(c => c.CharacterId == characterId);
}

public sealed class CaseModels
{
    [JsonProperty("creative")]
    public string Creative { get; set; } = "";

    [JsonProperty("critic")]
    public string Critic { get; set; } = "";
}

/// <summary>What the assignment stage chose: the level, the creature(s), the month and the shape of the case.</summary>
public sealed class CaseBrief
{
    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    [JsonProperty("vaesenIds")]
    public List<string> VaesenIds { get; set; } = [];

    [JsonProperty("month")]
    public int Month { get; set; }

    [JsonProperty("conflictArchetypeId")]
    public string ConflictArchetypeId { get; set; } = "";

    [JsonProperty("hookChannelId")]
    public string HookChannelId { get; set; } = "";

    [JsonProperty("hookGiverType")]
    public string HookGiverType { get; set; } = "";

    /// <summary>The seed the deterministic stages used, so a case can be reproduced up to the model's own choices.</summary>
    [JsonProperty("seed")]
    public int Seed { get; set; }
}

/// <summary>The whole hidden truth. Only generation, the validator and the orchestrator ever read it.</summary>
public sealed class CaseTruth
{
    [JsonProperty("summary")]
    public string Summary { get; set; } = "";

    [OneOf(typeof(ReasonKinds))]
    [JsonProperty("reasonKind")]
    public string ReasonKind { get; set; } = "";

    /// <summary>For afflictions: the chosen <see cref="VaesenOrigin"/> id.</summary>
    [JsonProperty("origin")]
    public string? Origin { get; set; }

    [JsonProperty("plotCentre")]
    public PlotCentre PlotCentre { get; set; } = new();

    /// <summary>One of <see cref="CaseFocus"/>.</summary>
    [OneOf(typeof(CaseFocus))]
    [JsonProperty("focus")]
    public string Focus { get; set; } = CaseFocus.What;

    /// <summary>What provoked the trouble, in prose.</summary>
    [JsonProperty("provocation")]
    public string Provocation { get; set; } = "";

    /// <summary>Where the provocation physically is.</summary>
    [JsonProperty("provocationPlaceId")]
    public string ProvocationPlaceId { get; set; } = "";

    /// <summary>The affordance tag the provocation depends on (logging needs "forest").</summary>
    [JsonProperty("provocationAffordanceTag")]
    public string ProvocationAffordanceTag { get; set; } = "";

    /// <summary>For a hidden hand: their motive, fixed in the conflict step.</summary>
    [JsonProperty("hiddenHandMotive")]
    public string? HiddenHandMotive { get; set; }

    /// <summary>The obvious folklore reading of why it turned, if used as a decoy.</summary>
    [JsonProperty("decoyReading")]
    public string? DecoyReading { get; set; }

    /// <summary>What the creature (or the hand) has concretely done: the symptoms players meet.</summary>
    [JsonProperty("escalation")]
    public List<EscalationEvent> Escalation { get; set; } = [];
}

public static class CaseFocus
{
    /// <summary>Identifying the creature is the puzzle.</summary>
    public const string What = "what";

    /// <summary>The creature is obvious; who is involved and why is hidden.</summary>
    public const string WhoWhy = "whoWhy";

    /// <summary>Everything is understood; the resolution is the dilemma.</summary>
    public const string How = "how";

    public static readonly IReadOnlyList<string> All = [What, WhoWhy, How];
}

public sealed class PlotCentre
{
    /// <summary>One of <see cref="PlotCentreKinds"/>.</summary>
    [OneOf(typeof(PlotCentreKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = PlotCentreKinds.Provocation;

    /// <summary>For a hidden hand: the cast member behind it.</summary>
    [JsonProperty("characterId")]
    public string? CharacterId { get; set; }
}

public static class PlotCentreKinds
{
    public const string HiddenHand = "hiddenHand";
    public const string Provocation = "provocation";
    public const string Nature = "nature";

    public static readonly IReadOnlyList<string> All = [HiddenHand, Provocation, Nature];
}

/// <summary>One concrete thing that has happened, described in the plainest terms its mundane reading allows.</summary>
public sealed class EscalationEvent
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>When it happened, relative to the Society's arrival ("three weeks ago", "the night before").</summary>
    [JsonProperty("when")]
    public string When { get; set; } = "";

    [JsonProperty("placeId")]
    public string? PlaceId { get; set; }

    /// <summary>The register it is noticed in (<see cref="SignRegisters"/>), to spread openings across registers.</summary>
    [OneOf(typeof(SignRegisters))]
    [JsonProperty("register")]
    public string Register { get; set; } = SignRegisters.Physical;

    /// <summary>The fact this event is, once the players learn of it.</summary>
    [JsonProperty("factIds")]
    public List<string> FactIds { get; set; } = [];

    /// <summary>Part of the opening convergence (known before or on arrival).</summary>
    [JsonProperty("opening")]
    public bool Opening { get; set; }
}

public sealed class CaseHook
{
    [JsonProperty("channel")]
    public string Channel { get; set; } = "";

    [JsonProperty("hookGiverId")]
    public string HookGiverId { get; set; } = "";

    /// <summary>The hook as the Society receives it (the letter, the article, the plea).</summary>
    [JsonProperty("text")]
    public string Text { get; set; } = "";

    /// <summary>Facts the hook states. They are the players' opening knowledge.</summary>
    [JsonProperty("assertedFactIds")]
    public List<string> AssertedFactIds { get; set; } = [];

    /// <summary>The <see cref="EscalationEvent"/>s the hook mentions.</summary>
    [JsonProperty("openingEventIds")]
    public List<string> OpeningEventIds { get; set; } = [];
}

public sealed class CaseLevel
{
    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    [JsonProperty("placesUsed")]
    public List<string> PlacesUsed { get; set; } = [];

    /// <summary>Scene text per place id, written for this case (the level's own description is the fallback).</summary>
    [JsonProperty("sceneDescriptions")]
    public Dictionary<string, string> SceneDescriptions { get; set; } = [];
}

/// <summary>One fact of the case. Load-bearing facts are the ones the case cannot be solved without.</summary>
public sealed class CaseFact
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("statement")]
    public string Statement { get; set; } = "";

    [JsonProperty("loadBearing")]
    public bool LoadBearing { get; set; }

    /// <summary>One of <see cref="FactKinds"/>.</summary>
    [OneOf(typeof(FactKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = FactKinds.Symptom;

    /// <summary>"trueAnswer", "neutral" or "decoy:{threadId}". See <see cref="FactPointsTo"/>.</summary>
    [JsonProperty("pointsTo")]
    public string PointsTo { get; set; } = FactPointsTo.Neutral;

    /// <summary>A giveaway tell: identifies the creature on sight.</summary>
    [JsonProperty("giveaway")]
    public bool Giveaway { get; set; }

    /// <summary>Built from the creature's deep lore (the solution trail), not from common knowledge.</summary>
    [JsonProperty("deepLore")]
    public bool DeepLore { get; set; }

    /// <summary>The <see cref="LoreItem"/> ids of the creature this fact rests on, if any.</summary>
    [JsonProperty("loreIds")]
    public List<string> LoreIds { get; set; } = [];
}

public static class FactKinds
{
    public const string Symptom = "symptom";
    public const string Cause = "cause";
    public const string Identity = "identity";
    public const string Alibi = "alibi";
    public const string Motive = "motive";
    public const string Resolution = "resolution";
    public const string Season = "season";
    public const string Colour = "colour";

    public static readonly IReadOnlyList<string> All = [Symptom, Cause, Identity, Alibi, Motive, Resolution, Season, Colour];
}

public static class FactPointsTo
{
    public const string TrueAnswer = "trueAnswer";
    public const string Neutral = "neutral";
    public const string DecoyPrefix = "decoy:";

    public static string Decoy(string threadId) => DecoyPrefix + threadId;

    public static bool IsDecoy(string? pointsTo) => pointsTo?.StartsWith(DecoyPrefix, StringComparison.Ordinal) == true;

    public static string? DecoyThreadId(string? pointsTo) => IsDecoy(pointsTo) ? pointsTo![DecoyPrefix.Length..] : null;
}

/// <summary>One way into a fact: a character who will tell, a trace at a place, a document, an overheard remark.</summary>
public sealed class CaseChannel
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("factId")]
    public string FactId { get; set; } = "";

    /// <summary>One of <see cref="ChannelKinds"/>.</summary>
    [OneOf(typeof(ChannelKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = ChannelKinds.Testimony;

    [JsonProperty("sourceCharacterId")]
    public string? SourceCharacterId { get; set; }

    [JsonProperty("placeId")]
    public string? PlaceId { get; set; }

    [JsonProperty("affordanceId")]
    public string? AffordanceId { get; set; }

    [JsonProperty("access")]
    public ChannelAccess Access { get; set; } = new();

    /// <summary>What the players perceive through this channel. This, not the fact statement, is what they are told.</summary>
    [JsonProperty("reveal")]
    public string Reveal { get; set; } = "";
}

public static class ChannelKinds
{
    public const string Testimony = "testimony";
    public const string Trace = "trace";
    public const string Document = "document";
    public const string Overheard = "overheard";
    public const string KnowledgeGated = "knowledgeGated";
    public const string Event = "event";

    public static readonly IReadOnlyList<string> All = [Testimony, Trace, Document, Overheard, KnowledgeGated, Event];
}

/// <summary>
/// What it takes to open a channel, as short strings: "trust&gt;=2", "knowsFact:f7", "roll:investigation",
/// "time:night", "place:mill". Parsed by <see cref="AccessRequirement.Parse"/>.
/// </summary>
public sealed class ChannelAccess
{
    [JsonProperty("requires")]
    public List<string> Requires { get; set; } = [];
}

public sealed class CastMember
{
    /// <summary>A canonical character id for persistent characters, otherwise "npc-{slug}".</summary>
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>One of <see cref="InstanceRefs"/>.</summary>
    [JsonProperty("instanceRef")]
    public string InstanceRef { get; set; } = InstanceRefs.New;

    /// <summary>Their part in this case ("hook-giver", "hidden hand", "afflicted", "suspect", "witness", "resident").</summary>
    [JsonProperty("plotRole")]
    public string PlotRole { get; set; } = "";

    /// <summary>One of <see cref="CastTiers"/>.</summary>
    [OneOf(typeof(CastTiers))]
    [JsonProperty("tier")]
    public string Tier { get; set; } = CastTiers.Significant;

    /// <summary>The <see cref="RoleSlot"/> of the conflict archetype they fill, if any.</summary>
    [JsonProperty("roleSlotId")]
    public string? RoleSlotId { get; set; }

    /// <summary>For new characters: who they are. Persistent ones take this from their definition.</summary>
    [JsonProperty("identity")]
    public CharacterIdentity? Identity { get; set; }

    [JsonProperty("homePlaceId")]
    public string? HomePlaceId { get; set; }

    [JsonProperty("knows")]
    public List<CastKnowledge> Knows { get; set; } = [];

    [JsonProperty("thinks")]
    public List<CastBelief> Thinks { get; set; } = [];

    [JsonProperty("wants")]
    public CastWants Wants { get; set; } = new();

    [JsonProperty("secrets")]
    public List<string> Secrets { get; set; } = [];

    [JsonProperty("lies")]
    public List<CastLie> Lies { get; set; } = [];

    [JsonProperty("routine")]
    public string Routine { get; set; } = "";

    [JsonProperty("voiceNotes")]
    public string VoiceNotes { get; set; } = "";

    /// <summary>Ties to other cast members ("her brother", "owes him money").</summary>
    [JsonProperty("relationships")]
    public List<CastRelationship> Relationships { get; set; } = [];
}

public static class InstanceRefs
{
    /// <summary>A canonical or Society character already in the persistent cast.</summary>
    public const string Persistent = "persistent";

    /// <summary>Created for this case.</summary>
    public const string New = "new";

    /// <summary>A background character raised into the story by this case.</summary>
    public const string Promoted = "promoted";

    public static readonly IReadOnlyList<string> All = [Persistent, New, Promoted];
}

public static class CastTiers
{
    public const string Significant = "significant";
    public const string Resident = "resident";

    public static readonly IReadOnlyList<string> All = [Significant, Resident];
}

/// <summary>A fact a character knows and how they came to know it.</summary>
public sealed class CastKnowledge
{
    [JsonProperty("factId")]
    public string FactId { get; set; } = "";

    /// <summary>"witnessed", "self" (they did it), "lore" (old knowledge), "told:{characterId}" or "found:{channelId}".</summary>
    [JsonProperty("source")]
    public string Source { get; set; } = KnowledgeSources.Witnessed;

    [JsonProperty("when")]
    public string When { get; set; } = "";
}

public static class KnowledgeSources
{
    public const string Witnessed = "witnessed";
    public const string Self = "self";
    public const string Lore = "lore";
    public const string ToldPrefix = "told:";
    public const string FoundPrefix = "found:";

    public static string Told(string characterId) => ToldPrefix + characterId;

    public static string Found(string channelId) => FoundPrefix + channelId;
}

/// <summary>What a character thinks, possibly wrong.</summary>
public sealed class CastBelief
{
    [JsonProperty("belief")]
    public string Belief { get; set; } = "";

    [JsonProperty("true")]
    public bool True { get; set; }
}

public sealed class CastWants
{
    [JsonProperty("drives")]
    public List<string> Drives { get; set; } = [];

    [JsonProperty("fears")]
    public List<string> Fears { get; set; } = [];

    [JsonProperty("guilt")]
    public string? Guilt { get; set; }

    /// <summary>How exposed they believe they are, 0 (safe) to 1 (found out).</summary>
    [JsonProperty("exposureBelief")]
    public double ExposureBelief { get; set; }

    /// <summary>The actions their drives permit when desperation rises (see <see cref="DesperationTiers"/>).</summary>
    [JsonProperty("permittedActions")]
    public List<CharacterAction> PermittedActions { get; set; } = [];
}

/// <summary>A drive-permitted action, unlocked at a desperation tier.</summary>
public sealed class CharacterAction
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>One of <see cref="DesperationTiers"/>.</summary>
    [OneOf(typeof(DesperationTiers))]
    [JsonProperty("tier")]
    public string Tier { get; set; } = DesperationTiers.Low;

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>What it does to the world: channels it closes or opens, facts it emits.</summary>
    [JsonProperty("effects")]
    public List<string> Effects { get; set; } = [];
}

public static class DesperationTiers
{
    /// <summary>Lie, deflect.</summary>
    public const string Low = "low";

    /// <summary>Hide or destroy evidence, intimidate, plead.</summary>
    public const string Medium = "medium";

    /// <summary>Flee, confess, help the group, or at the extreme silence someone.</summary>
    public const string High = "high";

    public static readonly IReadOnlyList<string> All = [Low, Medium, High];
}

public sealed class CastLie
{
    /// <summary>The fact they lie about.</summary>
    [JsonProperty("about")]
    public string About { get; set; } = "";

    [JsonProperty("story")]
    public string Story { get; set; } = "";
}

public sealed class CastRelationship
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("nature")]
    public string Nature { get; set; } = "";

    /// <summary>-3 (hatred) to +3 (love).</summary>
    [JsonProperty("sentiment")]
    public int Sentiment { get; set; }
}

/// <summary>A false trail that can be followed to its own honest end (spec 7.7).</summary>
public sealed class DecoyThread
{
    [JsonProperty("threadId")]
    public string ThreadId { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>A decoy creature with its own arc, if any.</summary>
    [JsonProperty("vaesenId")]
    public string? VaesenId { get; set; }

    /// <summary>The facts that point down this trail.</summary>
    [JsonProperty("factIds")]
    public List<string> FactIds { get; set; } = [];

    /// <summary>True facts that clear the trail from every angle.</summary>
    [JsonProperty("clearedByFactIds")]
    public List<string> ClearedByFactIds { get; set; } = [];

    [JsonProperty("resolution")]
    public string Resolution { get; set; } = "";
}

/// <summary>An authored beat that fires when tension crosses its threshold, where and when it may.</summary>
public sealed class CaseBeat
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>0 to 1 on the tension dial.</summary>
    [JsonProperty("threshold")]
    public double Threshold { get; set; }

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("requires")]
    public BeatRequirements Requires { get; set; } = new();

    /// <summary>Facts the beat puts into the world (and usually in front of the players).</summary>
    [JsonProperty("emitsFactIds")]
    public List<string> EmitsFactIds { get; set; } = [];

    /// <summary>The cast member who acts, if any.</summary>
    [JsonProperty("actorId")]
    public string? ActorId { get; set; }
}

public sealed class BeatRequirements
{
    [JsonProperty("place")]
    public string? Place { get; set; }

    [JsonProperty("time")]
    public string? Time { get; set; }
}

public sealed class CaseClimax
{
    [JsonProperty("threshold")]
    public double Threshold { get; set; } = 0.85;

    [JsonProperty("conditions")]
    public BeatRequirements Conditions { get; set; } = new();

    [JsonProperty("description")]
    public string Description { get; set; } = "";
}

/// <summary>A way to end the case once the truth is understood, and what it does to the world.</summary>
public sealed class ResolutionLever
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>One of <see cref="SolveDepths"/>.</summary>
    [OneOf(typeof(SolveDepths))]
    [JsonProperty("solveDepth")]
    public string SolveDepth { get; set; } = SolveDepths.Crude;

    /// <summary>Facts the players must have to see this lever at all.</summary>
    [JsonProperty("requiresFactIds")]
    public List<string> RequiresFactIds { get; set; } = [];

    [JsonProperty("consequences")]
    public LeverConsequences Consequences { get; set; } = new();
}

public static class SolveDepths
{
    /// <summary>Simply defeats or destroys.</summary>
    public const string Crude = "crude";

    /// <summary>Eases the grievance or drives the creature off without answering the cause.</summary>
    public const string Partial = "partial";

    /// <summary>Answers the grievance at its root.</summary>
    public const string Deep = "deep";

    public static readonly IReadOnlyList<string> All = [Crude, Partial, Deep];
}

public sealed class LeverConsequences
{
    [JsonProperty("world")]
    public List<string> World { get; set; } = [];

    [JsonProperty("attitudes")]
    public List<AttitudeChange> Attitudes { get; set; } = [];

    /// <summary>Who dies or leaves if the Society ends it this way; written back to the world.</summary>
    [JsonProperty("fates")]
    public List<CharacterFate> Fates { get; set; } = [];
}

public sealed class CharacterFate
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    /// <summary>"dead" or "departed" (<see cref="CharacterStatuses"/>).</summary>
    [JsonProperty("status")]
    public string Status { get; set; } = CharacterStatuses.Departed;

    [JsonProperty("reason")]
    public string Reason { get; set; } = "";
}

public sealed class AttitudeChange
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("change")]
    public string Change { get; set; } = "";

    /// <summary>How their attitude toward the Society moves, -3 to +3.</summary>
    [JsonProperty("delta")]
    public int Delta { get; set; }
}

public sealed class SeasonThreadLink
{
    [JsonProperty("threadId")]
    public string ThreadId { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("factIds")]
    public List<string> FactIds { get; set; } = [];
}

public sealed class CaseTension
{
    [JsonProperty("start")]
    public double Start { get; set; } = 0.1;

    /// <summary>How much uncovering each fact raises the dial.</summary>
    [JsonProperty("factWeights")]
    public Dictionary<string, double> FactWeights { get; set; } = [];
}

public sealed class CaseValidation
{
    [JsonProperty("passed")]
    public bool Passed { get; set; }

    [JsonProperty("issues")]
    public List<CoherenceIssue> Issues { get; set; } = [];

    /// <summary>How many validator runs it took, over all steps.</summary>
    [JsonProperty("runs")]
    public int Runs { get; set; }
}
