using Amazon.DynamoDBv2;
using Amazon.Runtime;

namespace Vaesen.Api.Infrastructure.Persistence;

/// <summary>
/// DynamoDB client and table configuration shared by all modules. Modules register their own repositories.
/// </summary>
public static class PersistenceModule
{
    /// <summary>Health checks with this tag need external dependencies; they're served on /health/ready, not /health.</summary>
    public const string ReadyTag = "ready";

    public static IServiceCollection AddPersistence(this IServiceCollection services, IConfiguration configuration)
    {
        var section = configuration.GetSection(DynamoDbOptions.SectionName);
        services.AddOptions<DynamoDbOptions>()
            .Bind(section)
            .Validate(o => !string.IsNullOrWhiteSpace(o.TableName), "DynamoDb:TableName is required.")
            .ValidateOnStart();

        var options = section.Get<DynamoDbOptions>() ?? new DynamoDbOptions();
        if (!string.IsNullOrWhiteSpace(options.ServiceUrl))
        {
            // DynamoDB Local accepts any credentials. Fail fast when it isn't running instead of retrying for minutes.
            services.AddSingleton<IAmazonDynamoDB>(_ => new AmazonDynamoDBClient(
                new BasicAWSCredentials("local", "local"),
                new AmazonDynamoDBConfig
                {
                    ServiceURL = options.ServiceUrl,
                    Timeout = TimeSpan.FromSeconds(5),
                    MaxErrorRetry = 1,
                }));

            if (options.CreateTableIfMissing)
            {
                services.AddHostedService<DynamoDbTableInitializer>();
            }
        }
        else
        {
            // Region and credentials come from the "AWS" config section, the AWS profile or the ECS task role.
            services.AddAWSService<IAmazonDynamoDB>();
        }

        services.AddHealthChecks().AddCheck<DynamoDbHealthCheck>("dynamodb", tags: [ReadyTag]);
        return services;
    }
}
