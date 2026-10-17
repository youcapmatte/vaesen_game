using System.Globalization;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Microsoft.Extensions.Options;
using Vaesen.Api.Infrastructure.Persistence;

namespace Vaesen.Api.Modules.Accounts;

/// <summary>
/// Accounts in the Vaesen table:
///   ACCOUNT#{id} / PROFILE            the account
///   USERNAME#{lowercase} / USERNAME   username reservation and lookup -> AccountId
///   AUTH0#{auth0UserId} / AUTH0       Auth0 link and lookup -> AccountId
/// </summary>
public sealed class DynamoDbAccountRepository(IAmazonDynamoDB dynamoDb, IOptions<DynamoDbOptions> options)
    : IAccountRepository
{
    private const string ConditionalCheckFailed = "ConditionalCheckFailed";

    private string Table => options.Value.TableName;

    public async Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken)
    {
        var item = await GetItemAsync(VaesenKeys.Account(accountId), VaesenKeys.Profile, cancellationToken);
        return item is null ? null : FromItem(item);
    }

    public Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken) =>
        GetViaLookupAsync(VaesenKeys.Username(username), VaesenKeys.UsernameSortKey, cancellationToken);

    public Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken) =>
        GetViaLookupAsync(VaesenKeys.Auth0User(auth0UserId), VaesenKeys.Auth0UserSortKey, cancellationToken);

    public async Task<CreateAccountResult> TryCreateAsync(Account account, CancellationToken cancellationToken)
    {
        // All succeed or none do: no account without its username, no reserved username without an account.
        var writes = new List<TransactWriteItem>
        {
            PutIfAbsent(ToItem(account)),
            PutIfAbsent(LookupItem(VaesenKeys.Username(account.Username), VaesenKeys.UsernameSortKey, "Username", account.Id)),
        };
        if (account.Auth0UserId is not null)
        {
            writes.Add(PutIfAbsent(LookupItem(VaesenKeys.Auth0User(account.Auth0UserId), VaesenKeys.Auth0UserSortKey, "Auth0User", account.Id)));
        }

        try
        {
            await dynamoDb.TransactWriteItemsAsync(new TransactWriteItemsRequest { TransactItems = writes }, cancellationToken);
            return CreateAccountResult.Created;
        }
        catch (TransactionCanceledException ex)
        {
            // Reasons are in the same order as the writes.
            var reasons = ex.CancellationReasons ?? [];
            if (reasons.Count > 1 && reasons[1].Code == ConditionalCheckFailed)
            {
                return CreateAccountResult.UsernameTaken;
            }
            if (reasons.Count > 2 && reasons[2].Code == ConditionalCheckFailed)
            {
                return CreateAccountResult.Auth0UserIdTaken;
            }
            throw;
        }
    }

    private async Task<Account?> GetViaLookupAsync(string pk, string sk, CancellationToken cancellationToken)
    {
        var lookup = await GetItemAsync(pk, sk, cancellationToken);
        return lookup is null ? null : await GetAsync(lookup["AccountId"].S, cancellationToken);
    }

    private async Task<Dictionary<string, AttributeValue>?> GetItemAsync(string pk, string sk, CancellationToken cancellationToken)
    {
        var response = await dynamoDb.GetItemAsync(new GetItemRequest
        {
            TableName = Table,
            Key = new Dictionary<string, AttributeValue> { ["PK"] = new(pk), ["SK"] = new(sk) },
        }, cancellationToken);

        // AWS SDK v4 returns null (not an empty map) when the item doesn't exist.
        return response.Item is { Count: > 0 } ? response.Item : null;
    }

    private TransactWriteItem PutIfAbsent(Dictionary<string, AttributeValue> item) => new()
    {
        Put = new Put { TableName = Table, Item = item, ConditionExpression = "attribute_not_exists(PK)" },
    };

    private static Dictionary<string, AttributeValue> LookupItem(string pk, string sk, string type, string accountId) => new()
    {
        ["PK"] = new(pk),
        ["SK"] = new(sk),
        ["Type"] = new(type),
        ["AccountId"] = new(accountId),
    };

    private static Dictionary<string, AttributeValue> ToItem(Account account)
    {
        var item = new Dictionary<string, AttributeValue>
        {
            ["PK"] = new(VaesenKeys.Account(account.Id)),
            ["SK"] = new(VaesenKeys.Profile),
            ["Type"] = new("Account"),
            ["Id"] = new(account.Id),
            ["Username"] = new(account.Username),
            ["SubscriptionStatus"] = new(account.Subscription.Status.ToString()),
            ["CreatedAt"] = new(FormatDate(account.CreatedAt)),
            ["Version"] = new() { N = account.Version.ToString(CultureInfo.InvariantCulture) },
        };
        AddIfSet(item, "Email", account.Email);
        AddIfSet(item, "Auth0UserId", account.Auth0UserId);
        AddIfSet(item, "SubscriptionPlan", account.Subscription.Plan);
        if (account.Subscription.CurrentPeriodEnd is { } periodEnd)
        {
            item["SubscriptionPeriodEnd"] = new(FormatDate(periodEnd));
        }
        return item;
    }

    private static Account FromItem(Dictionary<string, AttributeValue> item) => new(
        item["Id"].S,
        item["Username"].S,
        GetString(item, "Email"),
        GetString(item, "Auth0UserId"),
        new AccountSubscription(
            Enum.Parse<SubscriptionStatus>(item["SubscriptionStatus"].S),
            GetString(item, "SubscriptionPlan"),
            GetString(item, "SubscriptionPeriodEnd") is { } periodEnd ? ParseDate(periodEnd) : null),
        ParseDate(item["CreatedAt"].S),
        int.Parse(item["Version"].N, CultureInfo.InvariantCulture));

    private static void AddIfSet(Dictionary<string, AttributeValue> item, string name, string? value)
    {
        if (value is not null)
        {
            item[name] = new(value);
        }
    }

    private static string? GetString(Dictionary<string, AttributeValue> item, string name) =>
        item.TryGetValue(name, out var value) ? value.S : null;

    /// <summary>ISO-8601 in UTC, so values sort correctly as strings.</summary>
    private static string FormatDate(DateTimeOffset value) =>
        value.ToUniversalTime().ToString("O", CultureInfo.InvariantCulture);

    private static DateTimeOffset ParseDate(string value) =>
        DateTimeOffset.Parse(value, CultureInfo.InvariantCulture, DateTimeStyles.RoundtripKind);
}
