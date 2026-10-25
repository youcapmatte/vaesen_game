using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>What the assignment stage needs to know about a Society: what it has already faced, and how it was hooked.</summary>
public sealed class SocietyHistory
{
    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("societyName")]
    public string SocietyName { get; set; } = "";

    /// <summary>Past mysteries, oldest first.</summary>
    [JsonProperty("mysteries")]
    public List<PastMystery> Mysteries { get; set; } = [];

    /// <summary>The Society's investigators, for the prompt (name, archetype, a line about them).</summary>
    [JsonProperty("investigators")]
    public List<string> Investigators { get; set; } = [];
}

public sealed class PastMystery
{
    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    [JsonProperty("vaesenIds")]
    public List<string> VaesenIds { get; set; } = [];

    [JsonProperty("hookChannelId")]
    public string HookChannelId { get; set; } = "";

    [JsonProperty("hookGiverType")]
    public string HookGiverType { get; set; } = "";

    [JsonProperty("conflictArchetypeId")]
    public string ConflictArchetypeId { get; set; } = "";

    [JsonProperty("reasonKind")]
    public string ReasonKind { get; set; } = "";

    /// <summary>The in-world year it is set in; 0 for cases from before the calendar (the setting's start year).</summary>
    [JsonProperty("year")]
    public int Year { get; set; }

    [JsonProperty("month")]
    public int Month { get; set; }

    /// <summary>Played to the end (solved or failed); a queued case is not.</summary>
    [JsonProperty("finished")]
    public bool Finished { get; set; }
}

/// <summary>An in-world month: the Society's calendar only moves forward (fixes 4, section 11).</summary>
public readonly record struct InWorldDate(int Year, int Month) : IComparable<InWorldDate>
{
    /// <summary>Months since year 0, for differences.</summary>
    public int Index => (Year * 12) + Month - 1;

    public InWorldDate AddMonths(int months) => From(Index + months);

    public static InWorldDate From(int index) => new(index / 12, (index % 12) + 1);

    public int CompareTo(InWorldDate other) => Index.CompareTo(other.Index);

    public static bool operator <(InWorldDate a, InWorldDate b) => a.Index < b.Index;

    public static bool operator >(InWorldDate a, InWorldDate b) => a.Index > b.Index;

    public static bool operator <=(InWorldDate a, InWorldDate b) => a.Index <= b.Index;

    public static bool operator >=(InWorldDate a, InWorldDate b) => a.Index >= b.Index;

    public override string ToString() => $"{System.Globalization.CultureInfo.InvariantCulture.DateTimeFormat.GetMonthName(Month)} {Year}";
}

/// <summary>Output of the assignment stage (spec 7.1, 7.2): everything decided before the model writes anything.</summary>
public sealed class AssignmentBrief
{
    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("month")]
    public int Month { get; set; }

    /// <summary>The in-world year: later than (or the same year as, in a later month) the Society's last case.</summary>
    [JsonProperty("year")]
    public int Year { get; set; }

    [JsonProperty("seed")]
    public int Seed { get; set; }

    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    /// <summary>A deliberate return to a level the Society has been to (fixes 4, section 11): the level's cooldown is waived.</summary>
    [JsonProperty("returnVisit")]
    public bool ReturnVisit { get; set; }

    /// <summary>For a return visit: the Society's earlier mysteries on this level, oldest first.</summary>
    [JsonProperty("previousMysteryIds")]
    public List<string> PreviousMysteryIds { get; set; } = [];

    [JsonProperty("vaesenId")]
    public string VaesenId { get; set; } = "";

    /// <summary>Conflict archetypes that fit the creature, the reason kind and the level, best first.</summary>
    [JsonProperty("conflictArchetypeIds")]
    public List<string> ConflictArchetypeIds { get; set; } = [];

    [OneOf(typeof(ReasonKinds))]
    [JsonProperty("reasonKind")]
    public string ReasonKind { get; set; } = "";

    [JsonProperty("originId")]
    public string? OriginId { get; set; }

    [JsonProperty("hiddenHand")]
    public bool HiddenHand { get; set; }

    [JsonProperty("handDisposition")]
    public string? HandDisposition { get; set; }

    [OneOf(typeof(PlotCentreKinds))]
    [JsonProperty("plotCentreKind")]
    public string PlotCentreKind { get; set; } = PlotCentreKinds.Provocation;

    [OneOf(typeof(CaseFocus))]
    [JsonProperty("focus")]
    public string Focus { get; set; } = CaseFocus.What;

    [JsonProperty("hookChannelId")]
    public string HookChannelId { get; set; } = "";

    [JsonProperty("hookGiverType")]
    public string HookGiverType { get; set; } = "";

    /// <summary>Canonical characters the case may draw in.</summary>
    [JsonProperty("persistentCastIds")]
    public List<string> PersistentCastIds { get; set; } = [];

    /// <summary>Season threads this case should carry (descriptions); empty until seasons are built.</summary>
    [JsonProperty("seasonThreads")]
    public List<string> SeasonThreads { get; set; } = [];

    /// <summary>"low", "medium" or "high": how much misdirection to build.</summary>
    [JsonProperty("redHerringDensity")]
    public string RedHerringDensity { get; set; } = "low";

    /// <summary>Things the assignment had to relax (no level for the month, every creature on cooldown).</summary>
    [JsonProperty("notes")]
    public List<string> Notes { get; set; } = [];

    /// <summary>
    /// Month variants the content tool should write (fixes 7, section 8): "level-forsby-october@11" when no level was
    /// written for the case's month and another month's version stood in for it.
    /// </summary>
    [JsonProperty("wantedVariants")]
    public List<string> WantedVariants { get; set; } = [];
}
