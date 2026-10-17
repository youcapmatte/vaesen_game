using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// A playable character. It belongs to a guild, not to an account: which member plays it right now is recorded in
/// the guild's <see cref="Guild.CharacterAssignments"/>. Id: "character-{guid}".
/// </summary>
public sealed class Character : BaseCore
{
    /// <summary>Health a new character starts with when none is given.</summary>
    public const int DefaultHealthPoints = 100;

    [JsonProperty("guildId")]
    public string GuildId { get; set; } = "";

    /// <summary>The character name shown in the game. Unique across the game, case-insensitive; can't be changed.</summary>
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("firstName")]
    public string? FirstName { get; set; }

    [JsonProperty("lastName")]
    public string? LastName { get; set; }

    /// <summary>The character's class, free text for now (e.g. "Occultist").</summary>
    [JsonProperty("class")]
    public string? Class { get; set; }

    [JsonProperty("healthPoints")]
    public int HealthPoints { get; set; } = DefaultHealthPoints;
}

/// <summary>Changes to a character. Fields left null are kept as they are.</summary>
public sealed class UpdateCharacterRequest
{
    [JsonProperty("firstName")]
    public string? FirstName { get; set; }

    [JsonProperty("lastName")]
    public string? LastName { get; set; }

    [JsonProperty("class")]
    public string? Class { get; set; }

    [JsonProperty("healthPoints")]
    public int? HealthPoints { get; set; }

    /// <summary>Who made the change (stored as updatedBy). Optional.</summary>
    [JsonProperty("updatedByAccountId")]
    public string? UpdatedByAccountId { get; set; }
}
