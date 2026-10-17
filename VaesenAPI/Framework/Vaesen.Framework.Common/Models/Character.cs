using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// A playable character: a person with a first and last name. It belongs to a guild, not to an account: which member
/// plays it right now is recorded in the guild's <see cref="Guild.CharacterAssignments"/>. Id: "character-{guid}".
/// </summary>
public sealed class Character : BaseCore
{
    /// <summary>Health a new character starts with when none is given.</summary>
    public const int DefaultHealthPoints = 100;

    [JsonProperty("guildId")]
    public string GuildId { get; set; } = "";

    /// <summary>The character name: "{FirstName} {LastName}". Unique across the game, case-insensitive.</summary>
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("firstName")]
    public string FirstName { get; set; } = "";

    [JsonProperty("lastName")]
    public string LastName { get; set; } = "";

    /// <summary>The character's class, free text for now (e.g. "Occultist").</summary>
    [JsonProperty("class")]
    public string? Class { get; set; }

    [JsonProperty("healthPoints")]
    public int HealthPoints { get; set; } = DefaultHealthPoints;

    public static string FullName(string firstName, string lastName) => $"{firstName} {lastName}";
}

/// <summary>Changes to a character. Fields left null are kept as they are.</summary>
public sealed class UpdateCharacterRequest
{
    /// <summary>Renames the character (its name becomes "{firstName} {lastName}").</summary>
    [JsonProperty("firstName")]
    public string? FirstName { get; set; }

    /// <summary>Renames the character (its name becomes "{firstName} {lastName}").</summary>
    [JsonProperty("lastName")]
    public string? LastName { get; set; }

    /// <summary>"" clears the class.</summary>
    [JsonProperty("class")]
    public string? Class { get; set; }

    [JsonProperty("healthPoints")]
    public int? HealthPoints { get; set; }

    /// <summary>Who made the change (stored as updatedBy). Optional.</summary>
    [JsonProperty("updatedByAccountId")]
    public string? UpdatedByAccountId { get; set; }
}
