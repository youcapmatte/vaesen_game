using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A setting (Vaesen: Scandinavia around 1870): the world truths nothing lower may contradict, and the lists the
/// engine draws from. The engine is setting-agnostic; a new setting is a new content pack, not new code.
/// </summary>
public sealed class Setting : ContentRecord
{
    [JsonProperty("era")]
    public string Era { get; set; } = "";

    /// <summary>The in-world year play starts in.</summary>
    [JsonProperty("startYear")]
    public int StartYear { get; set; }

    [JsonProperty("worldTruths")]
    public List<string> WorldTruths { get; set; } = [];

    [JsonProperty("hookChannels")]
    public List<HookChannel> HookChannels { get; set; } = [];

    [JsonProperty("institutions")]
    public List<Institution> Institutions { get; set; } = [];

    [JsonProperty("periodConventions")]
    public PeriodConventions PeriodConventions { get; set; } = new();

    /// <summary>The kinds of hook-giver, with what each means (fixes 4, section 9), for the per-Society cooldown.</summary>
    [JsonProperty("hookGiverTypes")]
    public List<HookGiverType> HookGiverTypes { get; set; } = [];

    public HookGiverType? FindHookGiverType(string? id) => HookGiverTypes.Find(t => t.Id == id);

    public HookChannel? FindHookChannel(string? id) => HookChannels.Find(c => c.Id == id);

    /// <summary>The railways, canals, steamer routes and telegraph lines of the period, with when they opened (fixes 4, section 15).</summary>
    [JsonProperty("periodTransport")]
    public List<PeriodTransportLine> PeriodTransport { get; set; } = [];

    /// <summary>Where each region's county seat, courts, doctors and newspapers are (fixes 4, section 15).</summary>
    [JsonProperty("regionalInstitutions")]
    public List<RegionalInstitution> RegionalInstitutions { get; set; } = [];

    public PeriodTransportLine? FindLine(string? id) => PeriodTransport.Find(l => l.Id == id);
}

/// <summary>A kind of person who brings the trouble to the Society, and the words that recognise one.</summary>
public sealed class HookGiverType
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>One line: who counts ("a crown or parish official: sheriff, constable, clerk").</summary>
    [JsonProperty("meaning")]
    public string Meaning { get; set; } = "";

    /// <summary>Words in an occupation or rank that fit the type ("sheriff", "länsman", "clerk").</summary>
    [JsonProperty("matches")]
    public List<string> Matches { get; set; } = [];

    public bool Fits(CharacterIdentity? identity) =>
        Matches.Count == 0 || (identity is not null && Matches.Exists(m =>
            (identity.Occupation ?? "").Contains(m, StringComparison.OrdinalIgnoreCase) || (identity.Rank ?? "").Contains(m, StringComparison.OrdinalIgnoreCase)));
}

/// <summary>A way trouble reaches the Society: a letter, a newspaper item, a telegram, a visit.</summary>
public sealed class HookChannel
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>The scale of trouble it fits: one of <see cref="HookScales"/>.</summary>
    [JsonProperty("scales")]
    public List<string> Scales { get; set; } = [];

    /// <summary>Relative weight when choosing; the contact-network letter is common but never the only route.</summary>
    [JsonProperty("weight")]
    public double Weight { get; set; } = 1;

    /// <summary>
    /// The only kind of hook-giver this channel can carry (review 2, item 3): a visit from the old world comes from a
    /// being or its messenger, never a human's plea. Empty: any.
    /// </summary>
    [JsonProperty("giverType")]
    public string GiverType { get; set; } = "";
}

public static class HookScales
{
    public const string Private = "private";
    public const string Local = "local";
    public const string Regional = "regional";
    public const string National = "national";
}

public sealed class Institution
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";
}

public sealed class PeriodConventions
{
    [JsonProperty("travel")]
    public List<string> Travel { get; set; } = [];

    [JsonProperty("communication")]
    public List<string> Communication { get; set; } = [];

    [JsonProperty("money")]
    public string Money { get; set; } = "";

    [JsonProperty("socialRanks")]
    public List<string> SocialRanks { get; set; } = [];

    [JsonProperty("notes")]
    public List<string> Notes { get; set; } = [];
}
