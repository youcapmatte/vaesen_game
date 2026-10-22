using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A creature of the old world, as data (spec 5.2). There is no class per creature: a Näck and a werewolf are two
/// records of this type. Written from public folklore, never from rulebook prose.
/// </summary>
public sealed class VaesenDefinition : ContentRecord
{
    /// <summary>Related creatures share lore through their family (e.g. "caretaker-spirit", "water-spirit").</summary>
    [JsonProperty("family")]
    public string Family { get; set; } = "";

    [JsonProperty("summary")]
    public string Summary { get; set; } = "";

    /// <summary>Affordance tags a level must offer, all of them, for this creature to appear there.</summary>
    [JsonProperty("requiredAffordances")]
    public List<string> RequiredAffordances { get; set; } = [];

    /// <summary>What it is bound to or protects.</summary>
    [JsonProperty("domain")]
    public string Domain { get; set; } = "";

    [JsonProperty("reasonModel")]
    public ReasonModel ReasonModel { get; set; } = new();

    /// <summary>The reasons it may act against people; the engine picks one per case. Values: <see cref="ReasonKinds"/>.</summary>
    [JsonProperty("reasonKinds")]
    public List<string> ReasonKinds { get; set; } = [];

    /// <summary><see cref="ResolutionShapes"/>: can its grievance be answered, or does depth move to a hidden hand?</summary>
    [JsonProperty("resolutionShape")]
    public string ResolutionShape { get; set; } = ResolutionShapes.Negotiable;

    /// <summary>For afflictions: the possible origins, each saying whether a person is behind it.</summary>
    [JsonProperty("originSet")]
    public List<VaesenOrigin> OriginSet { get; set; } = [];

    /// <summary>What everyone assumes about it. Misdirection, never the solution trail.</summary>
    [JsonProperty("commonKnowledge")]
    public List<LoreItem> CommonKnowledge { get; set; } = [];

    /// <summary>Lesser-known truths. The solution trail is built from these.</summary>
    [JsonProperty("deepLore")]
    public List<LoreItem> DeepLore { get; set; } = [];

    /// <summary>Signs so iconic they identify the creature on sight.</summary>
    [JsonProperty("giveawayTells")]
    public List<VaesenSign> GiveawayTells { get; set; } = [];

    /// <summary>Signs that fit this creature and several others; safe for openings.</summary>
    [JsonProperty("ambiguousSigns")]
    public List<VaesenSign> AmbiguousSigns { get; set; } = [];

    [JsonProperty("confrontationConditions")]
    public ConfrontationConditions ConfrontationConditions { get; set; } = new();

    [JsonProperty("weaknesses")]
    public List<string> Weaknesses { get; set; } = [];

    [JsonProperty("protections")]
    public List<string> Protections { get; set; } = [];

    /// <summary>Tags of the <see cref="ConflictArchetype"/>s it fits.</summary>
    [JsonProperty("compatibleConflictTags")]
    public List<string> CompatibleConflictTags { get; set; } = [];

    /// <summary>Seasonal behaviour, if any ("strongest at midsummer"); months it is unlikely to stir go here too.</summary>
    [JsonProperty("seasonality")]
    public string? Seasonality { get; set; }

    /// <summary>Months (1-12) it never appears in. Empty: any month.</summary>
    [JsonProperty("excludedMonths")]
    public List<int> ExcludedMonths { get; set; } = [];

    /// <summary>The whole lore of the creature as allowed facts: everything a case may state about it.</summary>
    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public IEnumerable<LoreItem> AllLore => CommonKnowledge.Concat(DeepLore);
}

/// <summary>The creature's reasoning, before any case: disposition, what tips it over, the harm, the answer.</summary>
public sealed class ReasonModel
{
    [JsonProperty("nature")]
    public string Nature { get; set; } = "";

    [JsonProperty("trigger")]
    public string Trigger { get; set; } = "";

    [JsonProperty("expression")]
    public string Expression { get; set; } = "";

    [JsonProperty("resolution")]
    public string Resolution { get; set; } = "";
}

public static class ReasonKinds
{
    public const string ProvokedOnly = "provokedOnly";
    public const string SelfOriginatingGrievance = "selfOriginatingGrievance";
    public const string DomainViolated = "domainViolated";
    public const string BargainInvited = "bargainInvited";
    public const string Affliction = "affliction";
    public const string None = "none";

    public static readonly IReadOnlyList<string> All =
        [ProvokedOnly, SelfOriginatingGrievance, DomainViolated, BargainInvited, Affliction, None];
}

public static class ResolutionShapes
{
    public const string Negotiable = "negotiable";
    public const string NotNegotiable = "notNegotiable";

    public static readonly IReadOnlyList<string> All = [Negotiable, NotNegotiable];
}

/// <summary>One possible origin of an affliction (e.g. a curse laid by a jilted neighbour).</summary>
public sealed class VaesenOrigin
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("hiddenHand")]
    public bool HiddenHand { get; set; }

    /// <summary>When there is a hidden hand: one of <see cref="HandDispositions"/>.</summary>
    [JsonProperty("handDisposition")]
    public string? HandDisposition { get; set; }
}

public static class HandDispositions
{
    public const string Hostile = "hostile";
    public const string Desperate = "desperate";
    public const string LostControl = "lostControl";

    public static readonly IReadOnlyList<string> All = [Hostile, Desperate, LostControl];
}

public sealed class LoreItem
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}

/// <summary>A sign of the creature, with the register it is noticed in (spread openings across registers).</summary>
public sealed class VaesenSign
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    /// <summary>One of <see cref="SignRegisters"/>.</summary>
    [JsonProperty("register")]
    public string Register { get; set; } = SignRegisters.Physical;

    /// <summary>For ambiguous signs: other creatures or families it also fits.</summary>
    [JsonProperty("alsoFits")]
    public List<string> AlsoFits { get; set; } = [];
}

public static class SignRegisters
{
    public const string Physical = "physical";
    public const string Animal = "animal";
    public const string Weather = "weather";
    public const string Sound = "sound";
    public const string Behaviour = "behaviour";
    public const string Health = "health";
    public const string Dream = "dream";
    public const string Atmosphere = "atmosphere";
    public const string Social = "social";

    public static readonly IReadOnlyList<string> All = [Physical, Animal, Weather, Sound, Behaviour, Health, Dream, Atmosphere, Social];
}

/// <summary>When and where the creature can be confronted at all (spec 7.9).</summary>
public sealed class ConfrontationConditions
{
    /// <summary>One of <see cref="ConfrontationKinds"/>.</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = ConfrontationKinds.None;

    /// <summary>For time-gated creatures: "night", "nightfall", "full moon", "midsummer night".</summary>
    [JsonProperty("time")]
    public string? Time { get; set; }

    /// <summary>For place-gated creatures: affordance tags of where it can be met ("farmstead", "water").</summary>
    [JsonProperty("placeTags")]
    public List<string> PlaceTags { get; set; } = [];

    [JsonProperty("notes")]
    public string? Notes { get; set; }
}

public static class ConfrontationKinds
{
    public const string None = "none";
    public const string Time = "time";
    public const string Place = "place";
    public const string Both = "both";

    public static readonly IReadOnlyList<string> All = [None, Time, Place, Both];
}
