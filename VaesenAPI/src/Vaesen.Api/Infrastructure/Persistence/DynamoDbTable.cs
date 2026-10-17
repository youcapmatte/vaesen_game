using Amazon.DynamoDBv2;
using Amazon.DynamoDBv2.Model;

namespace Vaesen.Api.Infrastructure.Persistence;

/// <summary>
/// The Vaesen table's schema, used to create it in DynamoDB Local and in tests.
/// Tables in AWS are created separately (docs/DynamoDB.md, section 4.2).
/// </summary>
public static class DynamoDbTable
{
    public static async Task<bool> ExistsAsync(IAmazonDynamoDB dynamoDb, string tableName, CancellationToken cancellationToken)
    {
        try
        {
            await dynamoDb.DescribeTableAsync(tableName, cancellationToken);
            return true;
        }
        catch (ResourceNotFoundException)
        {
            return false;
        }
    }

    public static Task CreateAsync(IAmazonDynamoDB dynamoDb, string tableName, CancellationToken cancellationToken) =>
        dynamoDb.CreateTableAsync(new CreateTableRequest
        {
            TableName = tableName,
            BillingMode = BillingMode.PAY_PER_REQUEST,
            AttributeDefinitions =
            [
                new AttributeDefinition("PK", ScalarAttributeType.S),
                new AttributeDefinition("SK", ScalarAttributeType.S),
                new AttributeDefinition("GSI1PK", ScalarAttributeType.S),
                new AttributeDefinition("GSI1SK", ScalarAttributeType.S),
            ],
            KeySchema =
            [
                new KeySchemaElement("PK", KeyType.HASH),
                new KeySchemaElement("SK", KeyType.RANGE),
            ],
            GlobalSecondaryIndexes =
            [
                new GlobalSecondaryIndex
                {
                    IndexName = DynamoDbOptions.Gsi1,
                    KeySchema =
                    [
                        new KeySchemaElement("GSI1PK", KeyType.HASH),
                        new KeySchemaElement("GSI1SK", KeyType.RANGE),
                    ],
                    Projection = new Projection { ProjectionType = ProjectionType.ALL },
                },
            ],
        }, cancellationToken);
}
