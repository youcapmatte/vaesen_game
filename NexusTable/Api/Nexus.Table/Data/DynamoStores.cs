using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Data.Dynamo;

/// <summary>
/// The stores in DynamoDB, on the shared <see cref="BaseRepository{T}"/> (see VaesenAPI/docs/DynamoDB.md).
/// Table nexus-table-{env}, separate from the game's table:
///   {id} / ITEM                          every model, as its JSON document
///   SUBJECT#{subject} / SUBJECT          reservation: one account per token subject
///   GSI1 SOCIETY#{societyId} / CHAR#{name}      a Society's characters
///   GSI1 SOCIETY#{societyId} / ROLL#{createdAt} a Society's rolls
/// </summary>
public abstract class DynamoStore<T>(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    : BaseRepository<T>(dynamoDb, options, timeProvider), IStore<T>
    where T : BaseCore
{
    public Task<T?> GetAsync(string id, CancellationToken cancellationToken = default) => GetItemAsync(id, cancellationToken);

    public Task<IReadOnlyList<T>> GetManyAsync(IReadOnlyCollection<string> ids, CancellationToken cancellationToken = default) =>
        GetItemsAsync(ids, cancellationToken);

    public virtual async Task<ResultBase<T>> CreateAsync(T item, CancellationToken cancellationToken = default) =>
        await CreateItemAsync(item, cancellationToken).ConfigureAwait(false);

    public async Task<ResultBase<T>> UpdateAsync(T item, CancellationToken cancellationToken = default) =>
        await UpdateItemAsync(item, cancellationToken).ConfigureAwait(false);

    public static string SocietyIndexKey(string societyId) => $"SOCIETY#{societyId}";
}

public sealed class AccountRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    : DynamoStore<Account>(dynamoDb, options, timeProvider), IAccountStore
{
    private const string SubjectKind = "SUBJECT";

    public async Task<Account?> GetBySubjectAsync(string subject, CancellationToken cancellationToken = default)
    {
        var accountId = await GetReservationOwnerAsync(SubjectKey(subject), SubjectKind, cancellationToken).ConfigureAwait(false);
        return accountId is null ? null : await GetItemAsync(accountId, cancellationToken).ConfigureAwait(false);
    }

    public override async Task<ResultBase<Account>> CreateAsync(Account item, CancellationToken cancellationToken = default) =>
        await CreateItemAsync(
            item,
            [new Reservation(SubjectKey(item.Subject), SubjectKind, "This login already has an account.", NexusErrorCodes.SubjectTaken)],
            cancellationToken).ConfigureAwait(false);

    private static string SubjectKey(string subject) => $"SUBJECT#{subject}";
}

public sealed class SocietyRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    : DynamoStore<Society>(dynamoDb, options, timeProvider), ISocietyStore;

public sealed class CharacterRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    : DynamoStore<Character>(dynamoDb, options, timeProvider), ICharacterStore
{
    private const string SortPrefix = "CHAR#";

    public Task<IReadOnlyList<Character>> ListBySocietyAsync(string societyId, CancellationToken cancellationToken = default) =>
        QueryIndexAsync(SocietyIndexKey(societyId), SortPrefix, cancellationToken);

    protected override (string PartitionKey, string SortKey)? GetIndexKeys(Character item) =>
        (SocietyIndexKey(item.SocietyId), SortPrefix + item.Name.ToLowerInvariant());
}

public sealed class RollRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    : DynamoStore<Roll>(dynamoDb, options, timeProvider), IRollStore
{
    private const string SortPrefix = "ROLL#";

    public async Task<IReadOnlyList<Roll>> ListRecentBySocietyAsync(string societyId, int limit, CancellationToken cancellationToken = default)
    {
        // Fine for a test with a handful of players; page from the end of the index once the log grows.
        var rolls = await QueryIndexAsync(SocietyIndexKey(societyId), SortPrefix, cancellationToken).ConfigureAwait(false);
        return rolls.OrderByDescending(r => r.CreatedAt).Take(limit).ToList();
    }

    protected override (string PartitionKey, string SortKey)? GetIndexKeys(Roll item) =>
        (SocietyIndexKey(item.SocietyId), SortPrefix + item.CreatedAt.ToString("o"));
}
