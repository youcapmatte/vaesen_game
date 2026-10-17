using Amazon.DynamoDBv2;
using Microsoft.Extensions.Diagnostics.HealthChecks;
using Microsoft.Extensions.Options;

namespace Vaesen.Api.Infrastructure.Persistence;

public sealed class DynamoDbHealthCheck(IAmazonDynamoDB dynamoDb, IOptions<DynamoDbOptions> options) : IHealthCheck
{
    public async Task<HealthCheckResult> CheckHealthAsync(HealthCheckContext context, CancellationToken cancellationToken = default)
    {
        try
        {
            return await DynamoDbTable.ExistsAsync(dynamoDb, options.Value.TableName, cancellationToken)
                ? HealthCheckResult.Healthy()
                : HealthCheckResult.Unhealthy($"DynamoDB table '{options.Value.TableName}' does not exist.");
        }
        catch (Exception ex)
        {
            return HealthCheckResult.Unhealthy("DynamoDB is not reachable.", ex);
        }
    }
}
