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

    /// <summary>How the character looks (the character creator's result). Null for characters made before it existed.</summary>
    [JsonProperty("appearance", NullValueHandling = NullValueHandling.Ignore)]
    public CharacterAppearance? Appearance { get; set; }

    /// <summary>The world instance the character is in or was in last; decides where Play puts it. Null before it has played.</summary>
    [JsonProperty("lastLocation", NullValueHandling = NullValueHandling.Ignore)]
    public CharacterLocation? LastLocation { get; set; }

    public static string FullName(string firstName, string lastName) => $"{firstName} {lastName}";
}

/// <summary>Where a character is (or was last): which world instance, and when it entered and left.</summary>
public sealed class CharacterLocation
{
    /// <summary>The world definition, e.g. "gyllencreutz".</summary>
    [JsonProperty("worldId")]
    public string WorldId { get; set; } = "";

    /// <summary>The instance: its GameLift game session name, e.g. "gyllencreutz-guild-{guid}".</summary>
    [JsonProperty("sessionName")]
    public string SessionName { get; set; } = "";

    [JsonProperty("enteredAt")]
    public DateTime EnteredAt { get; set; }

    /// <summary>When it left (logged out, lost the connection); null while it's in the world.</summary>
    [JsonProperty("leftAt", NullValueHandling = NullValueHandling.Ignore)]
    public DateTime? LeftAt { get; set; }
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
