using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Core.Data;

/// <summary>
/// One conditional write prepared by a repository (<see cref="BaseRepository{T}.PrepareCreate"/>,
/// <see cref="BaseRepository{T}.PrepareUpdate"/>, <see cref="BaseRepository{T}.PrepareReservation"/>), to be committed
/// together with others by <see cref="DataTransaction"/>.
/// </summary>
/// <param name="Write">The DynamoDB write, with its condition.</param>
/// <param name="ErrorMessage">Reported when this write's condition fails.</param>
/// <param name="ErrorCode">Reported when this write's condition fails, e.g. "conflict" or "username_taken".</param>
/// <param name="OnCommitted">Applied to the in-memory model once everything is committed (e.g. the new version).</param>
public sealed record TransactWrite(TransactWriteItem Write, string ErrorMessage, string ErrorCode, Action? OnCommitted = null);

/// <summary>Commits prepared writes, possibly for different models, all or nothing.</summary>
public static class DataTransaction
{
    private const string ConditionalCheckFailed = "ConditionalCheckFailed";

    /// <summary>DynamoDB's limit for one transaction.</summary>
    public const int MaxWrites = 100;

    /// <summary>
    /// Writes everything or nothing. If a condition fails (item exists, version changed, value reserved), the result
    /// fails with that write's error code and nothing is stored.
    /// </summary>
    public static async Task<ResultBase> ExecuteAsync(IAmazonDynamoDB dynamoDb, IReadOnlyList<TransactWrite> writes, CancellationToken cancellationToken)
    {
        ArgumentNullException.ThrowIfNull(writes);
        var ret = new ResultBase();
        if (writes.Count == 0)
        {
            return ret.Ok();
        }

        if (writes.Count > MaxWrites)
        {
            throw new ArgumentException($"A transaction can hold at most {MaxWrites} writes.", nameof(writes));
        }

        try
        {
            if (writes.Count == 1)
            {
                var put = writes[0].Write.Put;
                await dynamoDb.PutItemAsync(new PutItemRequest
                {
                    TableName = put.TableName,
                    Item = put.Item,
                    ConditionExpression = put.ConditionExpression,
                    ExpressionAttributeValues = put.ExpressionAttributeValues,
                }, cancellationToken).ConfigureAwait(false);
            }
            else
            {
                await dynamoDb.TransactWriteItemsAsync(new TransactWriteItemsRequest
                {
                    TransactItems = writes.Select(w => w.Write).ToList(),
                }, cancellationToken).ConfigureAwait(false);
            }
        }
        catch (ConditionalCheckFailedException)
        {
            return ret.Fail(writes[0].ErrorMessage, writes[0].ErrorCode);
        }
        catch (TransactionCanceledException ex)
        {
            // Reasons come in the same order as the writes.
            var reasons = ex.CancellationReasons ?? [];
            for (var i = 0; i < reasons.Count && i < writes.Count; i++)
            {
                if (reasons[i].Code == ConditionalCheckFailed)
                {
                    return ret.Fail(writes[i].ErrorMessage, writes[i].ErrorCode);
                }
            }

            // E.g. TransactionConflict: another transaction touched the same items at the same moment.
            return ret.Fail("The data was changed by someone else at the same time. Try again.", ErrorCodes.Conflict);
        }

        foreach (var write in writes)
        {
            write.OnCommitted?.Invoke();
        }

        return ret.Ok();
    }
}
