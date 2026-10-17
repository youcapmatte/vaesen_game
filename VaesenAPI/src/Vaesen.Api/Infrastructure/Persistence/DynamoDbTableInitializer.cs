using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;

namespace Vaesen.Api.Infrastructure.Persistence;

/// <summary>
/// Development convenience: creates the table in DynamoDB Local after startup. Runs in the background and never
/// fails, so the API starts immediately and still runs (without storage) when DynamoDB Local isn't started.
/// </summary>
public sealed class DynamoDbTableInitializer(
    IAmazonDynamoDB dynamoDb,
    IOptions<DynamoDbOptions> options,
    ILogger<DynamoDbTableInitializer> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken cancellationToken)
    {
        var tableName = options.Value.TableName;
        try
        {
            if (await DynamoDbTable.ExistsAsync(dynamoDb, tableName, cancellationToken))
            {
                return;
            }

            await DynamoDbTable.CreateAsync(dynamoDb, tableName, cancellationToken);
            logger.LogInformation("Created DynamoDB Local table {Table}", tableName);
        }
        catch (Exception ex) when (ex is not OperationCanceledException)
        {
            logger.LogWarning(ex,
                "DynamoDB Local at {Url} is not reachable, so storage won't work. Start it with: docker compose up -d (in VaesenAPI)",
                options.Value.ServiceUrl);
        }
    }
}
