namespace Vaesen.Api.Modules.Players;

/// <summary>
/// Player profiles, progression and match results reported by game servers.
/// </summary>
public static class PlayersModule
{
    public static IServiceCollection AddPlayersModule(this IServiceCollection services, IConfiguration configuration)
    {
        return services;
    }

    public static IEndpointRouteBuilder MapPlayersEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/players").WithTags("Players");
        return routes;
    }
}
