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

    /// <summary>The kinds of hook-giver, for the per-Society cooldown (e.g. "official", "clergy", "relative").</summary>
    [JsonProperty("hookGiverTypes")]
    public List<string> HookGiverTypes { get; set; } = [];

    public HookChannel? FindHookChannel(string? id) => HookChannels.Find(c => c.Id == id);
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
