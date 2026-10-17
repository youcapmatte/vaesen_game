using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Data;

namespace Vaesen.Framework.Common.Repositorys;

/// <summary>Persistence for <see cref="Character"/>, so services can be tested without DynamoDB.</summary>
public interface ICharacterStore
{
    Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default);

    /// <summary>All characters of an account, ordered by name.</summary>
    Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken = default);

    /// <summary>Creates the character and reserves its name (case-insensitive). Fails with <see cref="CharacterRepository.NameTaken"/>.</summary>
    Task<ItemAction<Character>> CreateAsync(Character character, CancellationToken cancellationToken = default);
}

/// <summary>
/// Characters in the Vaesen table:
///   {characterId} / ITEM                   the character (GSI1: ACCOUNT#{accountId} / CHAR#{name})
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

    public static string NameKey(string name) => $"CHARNAME#{name.Trim().ToLowerInvariant()}";

    public static string AccountIndexKey(string accountId) => $"ACCOUNT#{accountId}";

    public Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default) =>
        GetItemAsync(characterId, cancellationToken);

    public Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken = default) =>
        QueryIndexAsync(AccountIndexKey(accountId), IndexSortPrefix, cancellationToken);

    public Task<ItemAction<Character>> CreateAsync(Character character, CancellationToken cancellationToken = default) =>
        CreateItemAsync(
            character,
            [new Reservation(NameKey(character.Name), NameKind, "That character name is taken.", NameTaken)],
            cancellationToken);

    protected override (string PartitionKey, string SortKey)? GetIndexKeys(Character item) =>
        (AccountIndexKey(item.AccountId), IndexSortPrefix + item.Name);
}
