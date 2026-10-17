using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Repositorys;

/// <summary>
/// Characters. They're created and deleted through <see cref="IGuildStore"/> (together with their guild);
/// changes to the character itself go through <see cref="UpdateAsync"/>.
/// </summary>
public interface ICharacterStore
{
    Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default);

    /// <summary>Case-insensitive.</summary>
    Task<Character?> GetByNameAsync(string name, CancellationToken cancellationToken = default);

    /// <summary>
    /// Saves changes. If the name changed from <paramref name="previousName"/>, the new name is reserved and the old one
    /// freed in the same write. Fails with <see cref="CharacterRepository.NameTaken"/>, or <see cref="ErrorCodes.Conflict"/>
    /// if someone else changed the character first.
    /// </summary>
    Task<ResultBase> UpdateAsync(Character character, string previousName, CancellationToken cancellationToken = default);

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

    public async Task<Character?> GetByNameAsync(string name, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(name))
        {
            return null;
        }

        var reservation = NameReservation(name);
        var characterId = await GetReservationOwnerAsync(reservation.Key, reservation.Kind, cancellationToken).ConfigureAwait(false);
        return characterId is null ? null : await GetItemAsync(characterId, cancellationToken).ConfigureAwait(false);
    }

    public Task<ResultBase> UpdateAsync(Character character, string previousName, CancellationToken cancellationToken = default)
    {
        var writes = new List<TransactWrite> { PrepareUpdate(character) };
        var newName = NameReservation(character.Name);
        var oldName = NameReservation(previousName);
        if (!string.Equals(newName.Key, oldName.Key, StringComparison.Ordinal))
        {
            writes.Add(PrepareReservation(newName, character.Id));
            writes.Add(PrepareReservationRelease(oldName, character.Id));
        }

        return DataTransaction.ExecuteAsync(DynamoDb, writes, cancellationToken);
    }

    public Task<IReadOnlyList<Character>> GetManyAsync(IReadOnlyCollection<string> characterIds, CancellationToken cancellationToken = default) =>
        GetItemsAsync(characterIds, cancellationToken);

    public Task<IReadOnlyList<Character>> ListByGuildAsync(string guildId, CancellationToken cancellationToken = default) =>
        QueryIndexAsync(GuildIndexKey(guildId), IndexSortPrefix, cancellationToken);

    protected override (string PartitionKey, string SortKey)? GetIndexKeys(Character item) =>
        (GuildIndexKey(item.GuildId), IndexSortPrefix + item.Name);
}
