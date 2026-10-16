namespace Vaesen.Api.Modules.Sessions;

/// <summary>
/// Matchmaking and game/player sessions via Amazon GameLift. The only part of the system that talks to GameLift.
/// </summary>
public static class SessionsModule
{
    public static IServiceCollection AddSessionsModule(this IServiceCollection services, IConfiguration configuration)
    {
        return services;
    }

    public static IEndpointRouteBuilder MapSessionsEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/sessions").WithTags("Sessions");
        return routes;
    }
}
