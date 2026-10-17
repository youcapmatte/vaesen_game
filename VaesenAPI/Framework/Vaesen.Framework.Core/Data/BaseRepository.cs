using System.Globalization;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.DocumentModel;
using Amazon.DynamoDBv2.Model;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Core.Data;

/// <summary>
/// DynamoDB storage for one model type, in the shared Vaesen table (single-table design, docs/DynamoDB.md).
///
/// Each model is stored as its JSON document plus key attributes:
///   PK = the item's id ("account-..."), SK = "ITEM", Type = class name, and optionally GSI1PK/GSI1SK.
/// Derived repositories add queries (<see cref="QueryIndexAsync"/>) and unique values (<see cref="Reservation"/>).
/// Writes that must happen together, also across models, are prepared with the Prepare* methods and committed
/// with <see cref="DataTransaction"/>.
/// </summary>
public class BaseRepository<T> : IBaseRepository<T> where T : BaseCore
{
    public const string ItemSortKey = "ITEM";

    private const int BatchGetLimit = 100;

    private static readonly JsonSerializerSettings SerializerSettings = new()
    {
        NullValueHandling = NullValueHandling.Ignore,
        DateFormatHandling = DateFormatHandling.IsoDateFormat,
        DateTimeZoneHandling = DateTimeZoneHandling.Utc,
        Converters = { new StringEnumConverter() },
    };

    private readonly IOptions<DataServiceOptions> _options;

    public BaseRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
    {
        DynamoDb = dynamoDb;
        _options = options;
        TimeProvider = timeProvider;
    }

    protected IAmazonDynamoDB DynamoDb { get; }

    protected TimeProvider TimeProvider { get; }

    protected string TableName => _options.Value.TableName;

    /// <summary>Stored in every item's "Type" attribute; makes the table readable in the console.</summary>
    protected virtual string TypeName => typeof(T).Name;

    public async Task<T?> GetItemAsync(string id, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(id))
        {
            return null;
        }

        var response = await DynamoDb.GetItemAsync(new GetItemRequest
        {
            TableName = TableName,
            Key = ItemKey(id),
        }, cancellationToken).ConfigureAwait(false);

        // AWS SDK v4 returns null (not an empty map) when the item doesn't exist.
        return response.Item is { Count: > 0 } ? VisibleOrNull(FromItem(response.Item)) : null;
    }

    public async Task<IReadOnlyList<T>> GetItemsAsync(IReadOnlyCollection<string> ids, CancellationToken cancellationToken = default)
    {
        var results = new List<T>();
        foreach (var chunk in ids.Where(id => !string.IsNullOrWhiteSpace(id)).Distinct(StringComparer.Ordinal).Chunk(BatchGetLimit))
        {
            var keys = chunk.Select(ItemKey).ToList();
            while (keys.Count > 0)
            {
                var response = await DynamoDb.BatchGetItemAsync(new BatchGetItemRequest
                {
                    RequestItems = new Dictionary<string, KeysAndAttributes> { [TableName] = new() { Keys = keys } },
                }, cancellationToken).ConfigureAwait(false);

                if (response.Responses?.TryGetValue(TableName, out var items) == true)
                {
                    results.AddRange(items.Select(FromItem).Where(item => !item.IsDeleted));
                }

                keys = response.UnprocessedKeys?.TryGetValue(TableName, out var unprocessed) == true
                    ? unprocessed.Keys
                    : [];
            }
        }

        return results;
    }

    public Task<ItemAction<T>> CreateItemAsync(T item, CancellationToken cancellationToken = default) =>
        CreateItemAsync(item, [], cancellationToken);

    public virtual async Task<ItemAction<T>> UpdateItemAsync(T item, CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(item);
        var ret = new ItemAction<T>();

        var result = await DataTransaction.ExecuteAsync(DynamoDb, [PrepareUpdate(item)], cancellationToken).ConfigureAwait(false);
        if (!result.Success)
        {
            ret.Fail(result.ErrorMessage, result.ErrorCode);
            return ret;
        }

        ret.Ok(item);
        return ret;
    }

    public async Task<ItemAction<T>> DeleteItemAsync(string id, CancellationToken cancellationToken = default)
    {
        var item = await GetItemAsync(id, cancellationToken).ConfigureAwait(false);
        if (item is null)
        {
            var ret = new ItemAction<T>();
            ret.Fail("Not found.", ErrorCodes.NotFound);
            return ret;
        }

        item.IsDeleted = true;
        return await UpdateItemAsync(item, cancellationToken).ConfigureAwait(false);
    }

    /// <summary>
    /// A write that stores a new item, failing if the id already exists. Sets CreatedAt/UpdatedAt and Version 1.
    /// Commit it with <see cref="DataTransaction.ExecuteAsync"/>, alone or with other writes.
    /// </summary>
    public TransactWrite PrepareCreate(T item)
    {
        ArgumentNullException.ThrowIfNull(item);

        var now = TimeProvider.GetUtcNow().UtcDateTime;
        item.CreatedAt = now;
        item.UpdatedAt = now;
        item.Version = 1;

        return new TransactWrite(
            new TransactWriteItem
            {
                Put = new Put { TableName = TableName, Item = ToItem(item), ConditionExpression = "attribute_not_exists(PK)" },
            },
            $"A {TypeName} with this id already exists.",
            ErrorCodes.Conflict);
    }

    /// <summary>
    /// A write that stores changes to an existing item, failing if someone else updated it since it was read
    /// (its version changed). The item's Version and UpdatedAt are bumped only once the write is committed.
    /// </summary>
    public TransactWrite PrepareUpdate(T item)
    {
        ArgumentNullException.ThrowIfNull(item);

        var expectedVersion = item.Version;
        var now = TimeProvider.GetUtcNow().UtcDateTime;
        var attributes = ToItem(item);
        attributes["version"] = new() { N = (expectedVersion + 1).ToString(CultureInfo.InvariantCulture) };
        attributes["updatedAt"] = new(now.ToString("o", CultureInfo.InvariantCulture));

        return new TransactWrite(
            new TransactWriteItem
            {
                Put = new Put
                {
                    TableName = TableName,
                    Item = attributes,
                    ConditionExpression = "attribute_exists(PK) AND version = :expected",
                    ExpressionAttributeValues = new Dictionary<string, AttributeValue>
                    {
                        [":expected"] = new() { N = expectedVersion.ToString(CultureInfo.InvariantCulture) },
                    },
                },
            },
            $"The {TypeName} was changed or removed by someone else. Reload and try again.",
            ErrorCodes.Conflict,
            () =>
            {
                item.Version = expectedVersion + 1;
                item.UpdatedAt = now;
            });
    }

    /// <summary>A write that claims a unique value for <paramref name="ownerId"/>, failing if it's already taken.</summary>
    public TransactWrite PrepareReservation(Reservation reservation, string ownerId)
    {
        ArgumentNullException.ThrowIfNull(reservation);

        return new TransactWrite(
            new TransactWriteItem
            {
                Put = new Put { TableName = TableName, Item = reservation.ToItem(ownerId), ConditionExpression = "attribute_not_exists(PK)" },
            },
            reservation.ErrorMessage,
            reservation.ErrorCode);
    }

    /// <summary>Frees a unique value (e.g. a deleted character's name), only if <paramref name="ownerId"/> still holds it.</summary>
    public TransactWrite PrepareReservationRelease(Reservation reservation, string ownerId)
    {
        ArgumentNullException.ThrowIfNull(reservation);

        return new TransactWrite(
            new TransactWriteItem
            {
                Delete = new Delete
                {
                    TableName = TableName,
                    Key = new Dictionary<string, AttributeValue> { ["PK"] = new(reservation.Key), ["SK"] = new(reservation.Kind) },
                    ConditionExpression = $"{Reservation.OwnerAttribute} = :owner",
                    ExpressionAttributeValues = new Dictionary<string, AttributeValue> { [":owner"] = new(ownerId) },
                },
            },
            "The reservation is held by someone else.",
            ErrorCodes.Conflict);
    }

    /// <summary>
    /// Stores a new item together with reservations of its unique values (e.g. a username), all or nothing.
    /// On a clash the result fails with the reservation's error code and <see cref="ResultBase{T}.Result"/> is null.
    /// </summary>
    protected async Task<ItemAction<T>> CreateItemAsync(T item, IReadOnlyList<Reservation> reservations, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(item);
        var ret = new ItemAction<T>();

        var writes = new List<TransactWrite> { PrepareCreate(item) };
        writes.AddRange(reservations.Select(r => PrepareReservation(r, item.Id)));

        var result = await DataTransaction.ExecuteAsync(DynamoDb, writes, cancellationToken).ConfigureAwait(false);
        if (!result.Success)
        {
            ret.Fail(result.ErrorMessage, result.ErrorCode);
            return ret;
        }

        ret.Ok(item);
        return ret;
    }

    /// <summary>The id of the item holding a reservation (e.g. the account that owns a username), or null.</summary>
    protected async Task<string?> GetReservationOwnerAsync(string key, string kind, CancellationToken cancellationToken)
    {
        var response = await DynamoDb.GetItemAsync(new GetItemRequest
        {
            TableName = TableName,
            Key = new Dictionary<string, AttributeValue> { ["PK"] = new(key), ["SK"] = new(kind) },
        }, cancellationToken).ConfigureAwait(false);

        return response.Item is { Count: > 0 } && response.Item.TryGetValue(Reservation.OwnerAttribute, out var owner) ? owner.S : null;
    }

    /// <summary>All items of this type whose GSI1PK is <paramref name="partitionKey"/>, ordered by GSI1SK.</summary>
    protected async Task<IReadOnlyList<T>> QueryIndexAsync(string partitionKey, string? sortKeyPrefix, CancellationToken cancellationToken)
    {
        var results = new List<T>();
        var values = new Dictionary<string, AttributeValue> { [":pk"] = new(partitionKey) };
        var condition = "GSI1PK = :pk";
        if (!string.IsNullOrEmpty(sortKeyPrefix))
        {
            condition += " AND begins_with(GSI1SK, :prefix)";
            values[":prefix"] = new(sortKeyPrefix);
        }

        Dictionary<string, AttributeValue>? startKey = null;
        do
        {
            var response = await DynamoDb.QueryAsync(new QueryRequest
            {
                TableName = TableName,
                IndexName = DataServiceOptions.Gsi1,
                KeyConditionExpression = condition,
                ExpressionAttributeValues = values,
                ExclusiveStartKey = startKey,
            }, cancellationToken).ConfigureAwait(false);

            results.AddRange((response.Items ?? []).Select(FromItem).Where(item => !item.IsDeleted));
            startKey = response.LastEvaluatedKey is { Count: > 0 } ? response.LastEvaluatedKey : null;
        } while (startKey is not null);

        return results;
    }

    /// <summary>Override to put the item in GSI1, e.g. characters under their guild.</summary>
    protected virtual (string PartitionKey, string SortKey)? GetIndexKeys(T item) => null;

    protected Dictionary<string, AttributeValue> ToItem(T item)
    {
        var json = JsonConvert.SerializeObject(item, SerializerSettings);
        var attributes = Document.FromJson(json).ToAttributeMap();
        attributes["PK"] = new(item.Id);
        attributes["SK"] = new(ItemSortKey);
        attributes["Type"] = new(TypeName);
        if (GetIndexKeys(item) is { } index)
        {
            attributes["GSI1PK"] = new(index.PartitionKey);
            attributes["GSI1SK"] = new(index.SortKey);
        }

        return attributes;
    }

    protected static T FromItem(Dictionary<string, AttributeValue> attributes)
    {
        var json = Document.FromAttributeMap(attributes).ToJson();
        return JsonConvert.DeserializeObject<T>(json, SerializerSettings)
            ?? throw new InvalidOperationException($"Could not read a {typeof(T).Name} from DynamoDB.");
    }

    private static Dictionary<string, AttributeValue> ItemKey(string id) => new()
    {
        ["PK"] = new(id),
        ["SK"] = new(ItemSortKey),
    };

    private static T? VisibleOrNull(T item) => item.IsDeleted ? null : item;
}

/// <summary>
/// A unique value claimed by an item, stored as its own row (PK = key, SK = kind) so DynamoDB can enforce uniqueness.
/// Example: <c>new Reservation("USERNAME#ingrid", "USERNAME", "Username is taken.", "username_taken")</c>.
/// </summary>
public sealed record Reservation(string Key, string Kind, string ErrorMessage, string ErrorCode)
{
    public const string OwnerAttribute = "OwnerId";

    internal Dictionary<string, AttributeValue> ToItem(string ownerId) => new()
    {
        ["PK"] = new(Key),
        ["SK"] = new(Kind),
        ["Type"] = new("Reservation"),
        [OwnerAttribute] = new(ownerId),
    };
}
