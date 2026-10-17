namespace Vaesen.Api.Infrastructure.Persistence;

/// <summary>
/// Every key format in the Vaesen table, in one place. See docs/DynamoDB.md for the design.
/// </summary>
public static class VaesenKeys
{
    public const string Profile = "PROFILE";
    public const string CharacterNameSortKey = "CHARNAME";

    public const string CharacterPrefix = "CHAR#";
    public const string GuildPrefix = "GUILD#";
    public const string MemberPrefix = "MEMBER#";

    public static string Account(string accountId) => $"ACCOUNT#{accountId}";
    public static string Character(string characterId) => CharacterPrefix + characterId;
    public static string CharacterName(string name) => $"CHARNAME#{name.Trim().ToLowerInvariant()}";
    public static string Guild(string guildId) => GuildPrefix + guildId;
    public static string GuildMember(string characterId) => MemberPrefix + characterId;
}
