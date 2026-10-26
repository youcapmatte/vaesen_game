using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Models;

/// <summary>
/// A person at the table, created on their first sign-in (Discord through Auth0, or the dev login).
/// Id: "account-{guid}". Unique by <see cref="Subject"/>.
/// </summary>
public sealed class Account : BaseCore
{
    /// <summary>The token subject ("sub"): Auth0's user_id, e.g. "oauth2|discord|1234...", or "dev|{player}".</summary>
    [JsonProperty("subject")]
    public string Subject { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";

    [JsonProperty("avatarUrl", NullValueHandling = NullValueHandling.Ignore)]
    public string? AvatarUrl { get; set; }

    /// <summary>Only the host configures the AI and starts mysteries. See NexusOptions.HostSubjects.</summary>
    [JsonProperty("role")]
    [JsonConverter(typeof(StringEnumConverter))]
    public AccountRole Role { get; set; } = AccountRole.Player;

    /// <summary>
    /// Let in by the host. A new sign-in waits until the host approves it on the Admin page; until then it can only read
    /// its own account. Accounts stored before approval existed have no such field and read as approved.
    /// </summary>
    [JsonProperty("isApproved")]
    public bool IsApproved { get; set; } = true;

    /// <summary>The character this person plays at the table, if they have made one.</summary>
    [JsonProperty("activeCharacterId", NullValueHandling = NullValueHandling.Ignore)]
    public string? ActiveCharacterId { get; set; }
}

public enum AccountRole
{
    Player,
    Host,
}
