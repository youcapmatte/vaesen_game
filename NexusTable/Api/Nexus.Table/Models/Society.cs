using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Models;

/// <summary>
/// The guild the players belong to. There is one for the first test; its id comes from NexusOptions.SocietyId.
/// Id: "society-{guid}".
/// </summary>
public sealed class Society : BaseCore
{
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>Where the Society meets between mysteries, e.g. a castle outside Upsala.</summary>
    [JsonProperty("headquarters")]
    public string Headquarters { get; set; } = "";

    [JsonProperty("memberAccountIds")]
    public List<string> MemberAccountIds { get; set; } = [];

    /// <summary>Mysteries played or being played, oldest first.</summary>
    [JsonProperty("mysteryIds")]
    public List<string> MysteryIds { get; set; } = [];
}

/// <summary>Changes the host makes to the Society. Fields left null are kept.</summary>
public sealed class UpdateSocietyRequest
{
    [JsonProperty("name")]
    public string? Name { get; set; }

    [JsonProperty("headquarters")]
    public string? Headquarters { get; set; }
}

/// <summary>The Society with its members and their characters, as the table shows it.</summary>
public sealed class SocietyView
{
    [JsonProperty("society")]
    public Society Society { get; set; } = new();

    [JsonProperty("members")]
    public List<SocietyMember> Members { get; set; } = [];
}

public sealed class SocietyMember
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";

    [JsonProperty("avatarUrl", NullValueHandling = NullValueHandling.Ignore)]
    public string? AvatarUrl { get; set; }

    [JsonProperty("role")]
    public AccountRole Role { get; set; }

    [JsonProperty("character", NullValueHandling = NullValueHandling.Ignore)]
    public Character? Character { get; set; }
}
