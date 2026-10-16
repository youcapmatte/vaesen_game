namespace Vaesen.Api.Modules.Identity;

/// <summary>
/// Accounts, login and token validation for the website, game client and game servers.
/// </summary>
public static class IdentityModule
{
    public static IServiceCollection AddIdentityModule(this IServiceCollection services, IConfiguration configuration)
    {
        return services;
    }

    public static IEndpointRouteBuilder MapIdentityEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/identity").WithTags("Identity");
        return routes;
    }
}
