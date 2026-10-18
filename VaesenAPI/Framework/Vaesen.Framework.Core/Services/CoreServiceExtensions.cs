using Amazon.DynamoDBv2;
using Amazon.Runtime;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Microsoft.Extensions.Hosting;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Middleware;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Core.Services;

public static class CoreServiceExtensions
{
    /// <summary>Health checks with this tag need external dependencies; they're served on /health/ready, not /health.</summary>
    public const string ReadyHealthTag = "ready";

    /// <summary>
    /// Infrastructure every Vaesen API needs: AWS + DynamoDB data service, Auth0 authentication and the
    /// authorization policies, security context, health checks and error handling.
    /// </summary>
    public static IServiceCollection AddCoreServices(this IServiceCollection services, IConfiguration configuration, IHostEnvironment environment)
    {
        services.AddHttpContextAccessor();
        services.TryAddSingleton(TimeProvider.System);
        services.AddSingleton<ISecurityService, SecurityService>();
        services.AddScoped<ResultExceptionMiddleware>();

        // Region and credentials for every AWS client: the "AWS" config section, the AWS profile or the ECS task role.
        services.AddDefaultAWSOptions(configuration.GetAWSOptions());

        services.AddDataService(configuration);
        services.AddAuth0Authentication(configuration, environment);

        return services;
    }

    private static void AddDataService(this IServiceCollection services, IConfiguration configuration)
    {
        var section = configuration.GetSection(DataServiceOptions.SectionName);
        services.AddOptions<DataServiceOptions>()
            .Bind(section)
            .Validate(o => !string.IsNullOrWhiteSpace(o.TableName), "DataService:TableName is required.")
            .ValidateOnStart();

        var options = section.Get<DataServiceOptions>() ?? new DataServiceOptions();
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
            services.AddAWSService<IAmazonDynamoDB>();
        }

        services.AddHealthChecks().AddCheck<DynamoDbHealthCheck>("dynamodb", tags: [ReadyHealthTag]);
    }

    private static void AddAuth0Authentication(this IServiceCollection services, IConfiguration configuration, IHostEnvironment environment)
    {
        services.Configure<Auth0Options>(configuration.GetSection(Auth0Options.SectionName));
        var auth0 = configuration.GetSection(Auth0Options.SectionName).Get<Auth0Options>() ?? new Auth0Options();

        services
            .AddAuthentication(JwtBearerDefaults.AuthenticationScheme)
            .AddJwtBearer(options =>
            {
                options.Authority = $"https://{auth0.Domain}/";
                options.Audience = auth0.Audience;
                // Keep Auth0's claim names ("sub", "scope", "gty") instead of the legacy SOAP claim URIs.
                options.MapInboundClaims = false;
            });

        services
            .AddAuthorizationBuilder()
            .AddPolicy(Policies.Player, policy => policy
                .RequireAuthenticatedUser()
                .RequireAssertion(context => context.User.IsPlayer()))
            .AddPolicy(Policies.GameServer, policy =>
            {
                if (environment.IsDevelopment())
                {
                    // Open in Development, so local and GameLift Anywhere servers can report without machine credentials.
                    policy.RequireAssertion(_ => true);
                }
                else
                {
                    policy.RequireAuthenticatedUser()
                        .RequireAssertion(context => context.User.HasScope(Auth0Options.GameServerScope));
                }
            })
            .AddPolicy(Policies.Admin, policy =>
            {
                if (environment.IsDevelopment())
                {
                    // Open in Development, so back-office endpoints can be tried without an admin token.
                    policy.RequireAssertion(_ => true);
                }
                else
                {
                    policy.RequireAuthenticatedUser()
                        .RequireAssertion(context => context.User.HasScope(Auth0Options.AdminScope));
                }
            });
    }
}
