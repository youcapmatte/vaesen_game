using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// A guild: its members (accounts) and the characters it owns. Characters belong to the guild, not to an account;
/// <see cref="CharacterAssignments"/> records which member currently plays which character, and that can change
/// from session to session. Id: "guild-{guid}".
/// </summary>
public sealed class Guild : BaseCore
{
    /// <summary>Unique across the game, case-insensitive.</summary>
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>Accounts that are members. Mirrors each member's <see cref="Account.GuildIds"/>.</summary>
    [JsonProperty("memberAccountIds")]
    public List<string> MemberAccountIds { get; set; } = [];

    /// <summary>Characters the guild owns.</summary>
    [JsonProperty("characterIds")]
    public List<string> CharacterIds { get; set; } = [];

    /// <summary>
    /// Who plays which character right now. A character has at most one player and a member plays at most one of
    /// the guild's characters at a time; characters without an entry are free.
    /// </summary>
    [JsonProperty("characterAssignments")]
    public List<CharacterAssignment> CharacterAssignments { get; set; } = [];

    public bool IsMember(string accountId) => MemberAccountIds.Contains(accountId, StringComparer.Ordinal);

    public bool OwnsCharacter(string characterId) => CharacterIds.Contains(characterId, StringComparer.Ordinal);

    /// <summary>The account currently playing <paramref name="characterId"/>, or null if it's free.</summary>
    public string? GetPlayerOf(string characterId) =>
        CharacterAssignments.Find(a => string.Equals(a.CharacterId, characterId, StringComparison.Ordinal))?.AccountId;

    /// <summary>The character <paramref name="accountId"/> currently plays in this guild, or null.</summary>
    public string? GetCharacterOf(string accountId) =>
        CharacterAssignments.Find(a => string.Equals(a.AccountId, accountId, StringComparison.Ordinal))?.CharacterId;
}

/// <summary>A member playing one of the guild's characters.</summary>
public sealed class CharacterAssignment
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("assignedAt")]
    public DateTime AssignedAt { get; set; }
}

public sealed class CreateGuildRequest
{
    [JsonProperty("name")]
    public string? Name { get; set; }

    /// <summary>The founding account; becomes the first member.</summary>
    [JsonProperty("createdByAccountId")]
    public string? CreatedByAccountId { get; set; }
}

public sealed class GuildMemberRequest
{
    [JsonProperty("accountId")]
    public string? AccountId { get; set; }
}

public sealed class CreateCharacterRequest
{
    /// <summary>Required. The character name becomes "{firstName} {lastName}", which must be unique.</summary>
    [JsonProperty("firstName")]
    public string? FirstName { get; set; }

    /// <summary>Required.</summary>
    [JsonProperty("lastName")]
    public string? LastName { get; set; }

    [JsonProperty("class")]
    public string? Class { get; set; }

    /// <summary>Starting health; <see cref="Character.DefaultHealthPoints"/> when left out.</summary>
    [JsonProperty("healthPoints")]
    public int? HealthPoints { get; set; }

    /// <summary>The member creating it (stored as createdBy). Must be a member of the guild.</summary>
    [JsonProperty("createdByAccountId")]
    public string? CreatedByAccountId { get; set; }
}

public sealed class AssignPlayerRequest
{
    /// <summary>The member who will play the character.</summary>
    [JsonProperty("accountId")]
    public string? AccountId { get; set; }
}
