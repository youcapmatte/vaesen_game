using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>A playable character, owned by an account. Id: "character-{guid}".</summary>
public sealed class Character : BaseCore
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    /// <summary>Unique across the game, case-insensitive.</summary>
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("guildId")]
    public string? GuildId { get; set; }
}
