using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// A playable character. It belongs to a guild, not to an account: which member plays it right now is recorded in
/// the guild's <see cref="Guild.CharacterAssignments"/>. Id: "character-{guid}".
/// </summary>
public sealed class Character : BaseCore
{
    [JsonProperty("guildId")]
    public string GuildId { get; set; } = "";

    /// <summary>Unique across the game, case-insensitive.</summary>
    [JsonProperty("name")]
    public string Name { get; set; } = "";
}
