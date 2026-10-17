using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Data;

namespace Vaesen.Framework.Common.Repositorys;

/// <summary>Reading characters. They're created through <see cref="IGuildStore.CreateCharacterAsync"/>, together with their guild.</summary>
public interface ICharacterStore
{
    Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default);

    Task<IReadOnlyList<Character>> GetManyAsync(IReadOnlyCollection<string> characterIds, CancellationToken cancellationToken = default);

    /// <summary>All characters of a guild, ordered by name.</summary>
    Task<IReadOnlyList<Character>> ListByGuildAsync(string guildId, CancellationToken cancellationToken = default);
}

/// <summary>
/// Characters in the Vaesen table:
///   {characterId} / ITEM                   the character (GSI1: GUILD#{guildId} / CHAR#{name})
///   CHARNAME#{lowercase name} / CHARNAME   name reservation, so names are unique
/// </summary>
public sealed class CharacterRepository : BaseRepository<Character>, ICharacterStore
{
    public const string NameTaken = "character_name_taken";

    private const string NameKind = "CHARNAME";
    private const string IndexSortPrefix = "CHAR#";

    public CharacterRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
        : base(dynamoDb, options, timeProvider)
    {
    }

    public static string GuildIndexKey(string guildId) => $"GUILD#{guildId}";

    /// <summary>The reservation that keeps character names unique.</summary>
    public static Reservation NameReservation(string name) =>
        new($"CHARNAME#{name.Trim().ToLowerInvariant()}", NameKind, "That character name is taken.", NameTaken);

    public Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default) =>
        GetItemAsync(characterId, cancellationToken);

    public Task<IReadOnlyList<Character>> GetManyAsync(IReadOnlyCollection<string> characterIds, CancellationToken cancellationToken = default) =>
        GetItemsAsync(characterIds, cancellationToken);

    public Task<IReadOnlyList<Character>> ListByGuildAsync(string guildId, CancellationToken cancellationToken = default) =>
        QueryIndexAsync(GuildIndexKey(guildId), IndexSortPrefix, cancellationToken);

    protected override (string PartitionKey, string SortKey)? GetIndexKeys(Character item) =>
        (GuildIndexKey(item.GuildId), IndexSortPrefix + item.Name);
}
