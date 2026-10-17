using Microsoft.AspNetCore.Authentication.JwtBearer;

namespace Vaesen.Api.Modules.Identity;

/// <summary>
/// Login and token validation for the website, game client and game servers (accounts live in the Accounts module).
/// Tokens are issued by Auth0; this API only validates them.
/// </summary>
public static class IdentityModule
{
    public static IServiceCollection AddIdentityModule(this IServiceCollection services, IConfiguration configuration)
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
            .AddPolicy(Policies.GameServer, policy => policy
                .RequireAuthenticatedUser()
                .RequireAssertion(context => context.User.HasScope(Auth0Options.GameServerScope)))
            .AddPolicy(Policies.Admin, policy => policy
                .RequireAuthenticatedUser()
                .RequireAssertion(context => context.User.HasScope(Auth0Options.AdminScope)));

        return services;
    }

    public static IEndpointRouteBuilder MapIdentityEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/identity").WithTags("Identity");

        // Lets the game client confirm its token works and learn its own player id.
        group.MapGet("/me", (HttpContext http) => Results.Ok(new MeResponse(http.User.GetPlayerId())))
            .RequireAuthorization(Policies.Player);

        return routes;
    }
}

public sealed class Auth0Options
{
    public const string SectionName = "Auth0";

    /// <summary>Scope granted to the game-server machine-to-machine application in Auth0.</summary>
    public const string GameServerScope = "game:server";

    /// <summary>Scope for back-office tools (website admin pages, support scripts).</summary>
    public const string AdminScope = "vaesen:admin";

    /// <summary>Auth0 tenant domain, e.g. "vaesen.eu.auth0.com".</summary>
    public string Domain { get; set; } = "";

    /// <summary>Identifier of the API registered in Auth0, e.g. "https://api.vaesen.game".</summary>
    public string Audience { get; set; } = "";
}

public static class Policies
{
    /// <summary>A human player signed in through Auth0 (game client or website).</summary>
    public const string Player = "Player";

    /// <summary>A dedicated game server using its machine-to-machine credentials.</summary>
    public const string GameServer = "GameServer";

    /// <summary>Back-office access to any account's data.</summary>
    public const string Admin = "Admin";
}

public sealed record MeResponse(string PlayerId);
