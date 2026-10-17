using System.Globalization;
using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Microsoft.Extensions.Options;
using Vaesen.Api.Infrastructure.Persistence;

namespace Vaesen.Api.Modules.Players;

/// <summary>
/// Characters in the Vaesen table:
///   CHAR#{id} / PROFILE                     the character (GSI1: ACCOUNT#{accountId} / CHAR#{name})
///   CHARNAME#{lowercase name} / CHARNAME    name reservation, so names are unique
/// </summary>
public sealed class DynamoDbCharacterRepository(IAmazonDynamoDB dynamoDb, IOptions<DynamoDbOptions> options)
    : ICharacterRepository
{
    private string Table => options.Value.TableName;

    public async Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken)
    {
        var response = await dynamoDb.GetItemAsync(new GetItemRequest
        {
            TableName = Table,
            Key = Key(VaesenKeys.Character(characterId), VaesenKeys.Profile),
        }, cancellationToken);

        // AWS SDK v4 returns null (not an empty map) when the item doesn't exist.
        return response.Item is { Count: > 0 } ? FromItem(response.Item) : null;
    }

    public async Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken)
    {
        var characters = new List<Character>();
        Dictionary<string, AttributeValue>? startKey = null;
        do
        {
            var response = await dynamoDb.QueryAsync(new QueryRequest
            {
                TableName = Table,
                IndexName = DynamoDbOptions.Gsi1,
                KeyConditionExpression = "GSI1PK = :pk AND begins_with(GSI1SK, :prefix)",
                ExpressionAttributeValues = new Dictionary<string, AttributeValue>
                {
                    [":pk"] = new(VaesenKeys.Account(accountId)),
                    [":prefix"] = new(VaesenKeys.CharacterPrefix),
                },
                ExclusiveStartKey = startKey,
            }, cancellationToken);

            characters.AddRange((response.Items ?? []).Select(FromItem));
            startKey = response.LastEvaluatedKey is { Count: > 0 } ? response.LastEvaluatedKey : null;
        } while (startKey is not null);

        return characters;
    }

    public async Task<bool> TryCreateAsync(Character character, CancellationToken cancellationToken)
    {
        // Both writes succeed or neither does: no character without its name, no name without its character.
        try
        {
            await dynamoDb.TransactWriteItemsAsync(new TransactWriteItemsRequest
            {
                TransactItems =
                [
                    new TransactWriteItem
                    {
                        Put = new Put
                        {
                            TableName = Table,
                            Item = ToItem(character),
                            ConditionExpression = "attribute_not_exists(PK)",
                        },
                    },
                    new TransactWriteItem
                    {
                        Put = new Put
                        {
                            TableName = Table,
                            Item = new Dictionary<string, AttributeValue>
                            {
                                ["PK"] = new(VaesenKeys.CharacterName(character.Name)),
                                ["SK"] = new(VaesenKeys.CharacterNameSortKey),
                                ["Type"] = new("CharacterName"),
                                ["CharacterId"] = new(character.Id),
                            },
                            ConditionExpression = "attribute_not_exists(PK)",
                        },
                    },
                ],
            }, cancellationToken);
            return true;
        }
        catch (TransactionCanceledException)
        {
            return false;
        }
    }

    private static Dictionary<string, AttributeValue> Key(string pk, string sk) => new()
    {
        ["PK"] = new(pk),
        ["SK"] = new(sk),
    };

    private static Dictionary<string, AttributeValue> ToItem(Character character)
    {
        var item = new Dictionary<string, AttributeValue>
        {
            ["PK"] = new(VaesenKeys.Character(character.Id)),
            ["SK"] = new(VaesenKeys.Profile),
            ["GSI1PK"] = new(VaesenKeys.Account(character.AccountId)),
            ["GSI1SK"] = new(VaesenKeys.CharacterPrefix + character.Name),
            ["Type"] = new("Character"),
            ["Id"] = new(character.Id),
            ["AccountId"] = new(character.AccountId),
            ["Name"] = new(character.Name),
            ["Version"] = new() { N = character.Version.ToString(CultureInfo.InvariantCulture) },
        };
        if (character.GuildId is not null)
        {
            item["GuildId"] = new(character.GuildId);
        }
        return item;
    }

    private static Character FromItem(Dictionary<string, AttributeValue> item) => new(
        item["Id"].S,
        item["AccountId"].S,
        item["Name"].S,
        item.TryGetValue("GuildId", out var guild) ? guild.S : null,
        int.Parse(item["Version"].N, CultureInfo.InvariantCulture));
}
