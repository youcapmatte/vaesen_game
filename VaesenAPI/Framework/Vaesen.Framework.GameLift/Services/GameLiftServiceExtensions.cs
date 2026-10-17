using Amazon.GameLift;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

public static class GameLiftServiceExtensions
{
    /// <summary>
    /// Worlds and joining them via Amazon GameLift (the only part of the system that talks to GameLift).
    /// Sessions:Provider "Local" replaces GameLift with servers you run yourself. Controller: SessionController.
    /// Needs AddCoreServices (AWS options) and AddCommonServices (subscriptions).
    /// </summary>
    public static IServiceCollection AddGameLiftServices(this IServiceCollection services, IConfiguration configuration)
    {
        var section = configuration.GetSection(SessionsOptions.SectionName);
        services.Configure<SessionsOptions>(section);

        var provider = section.Get<SessionsOptions>()?.Provider;
        if (string.Equals(provider, SessionProviders.Local, StringComparison.OrdinalIgnoreCase))
        {
            services.AddSingleton<IGameLiftGateway, LocalGameLiftGateway>();
        }
        else
        {
            services.AddAWSService<IAmazonGameLift>();
            services.AddSingleton<IGameLiftGateway, AwsGameLiftGateway>();
        }

        services.AddSingleton<WorldSessionService>();
        return services;
    }
}
