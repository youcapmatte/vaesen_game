using Amazon.GameLift;
using Vaesen.Api.Modules.Identity;

namespace Vaesen.Api.Modules.Sessions;

/// <summary>
/// Matchmaking and game/player sessions via Amazon GameLift. The only part of the system that talks to GameLift.
/// </summary>
public static class SessionsModule
{
    private const int StartingRetryAfterSeconds = 2;

    public static IServiceCollection AddSessionsModule(this IServiceCollection services, IConfiguration configuration)
    {
        services.Configure<SessionsOptions>(configuration.GetSection(SessionsOptions.SectionName));

        var provider = configuration.GetSection(SessionsOptions.SectionName).Get<SessionsOptions>()?.Provider;
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

    public static IEndpointRouteBuilder MapSessionsEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/sessions").WithTags("Sessions").RequireAuthorization(Policies.Player);

        group.MapGet("/worlds", (WorldSessionService sessions) =>
            sessions.Worlds.Select(w => new WorldResponse(w.Id, w.DisplayName)));

        group.MapPost("/worlds/{worldId}/join", JoinWorld);

        return routes;
    }

    private static async Task<IResult> JoinWorld(
        string worldId,
        HttpContext http,
        WorldSessionService sessions,
        CancellationToken cancellationToken)
    {
        var result = await sessions.JoinAsync(worldId, http.User.GetPlayerId(), cancellationToken);

        switch (result.Status)
        {
            case JoinWorldStatus.Ready:
                var world = result.World!;
                var session = result.PlayerSession!;
                var host = string.IsNullOrEmpty(session.DnsName) ? session.IpAddress : session.DnsName;
                return Results.Ok(new JoinWorldResponse(
                    "ready", world.Id, world.Map,
                    session.IpAddress, session.DnsName, session.Port, session.PlayerSessionId,
                    // Pass straight to ClientTravel/open; the server validates PlayerSessionId with GameLift on PreLogin.
                    ConnectUrl: $"{host}:{session.Port}?PlayerSessionId={session.PlayerSessionId}",
                    RetryAfterSeconds: null));

            case JoinWorldStatus.Starting:
                // The world's server process is still activating; the client polls this endpoint again.
                http.Response.Headers.RetryAfter = StartingRetryAfterSeconds.ToString();
                return Results.Accepted(value: new JoinWorldResponse(
                    "starting", result.World!.Id, result.World.Map,
                    null, null, null, null, null, StartingRetryAfterSeconds));

            case JoinWorldStatus.Full:
                return Results.Problem(statusCode: StatusCodes.Status409Conflict, title: "World is full.");

            case JoinWorldStatus.NoSubscription:
                return Results.Problem(statusCode: StatusCodes.Status403Forbidden, title: "An active subscription is required.");

            default:
                return Results.Problem(statusCode: StatusCodes.Status404NotFound, title: $"Unknown world '{worldId}'.");
        }
    }
}

public sealed record WorldResponse(string Id, string DisplayName);

public sealed record JoinWorldResponse(
    string Status,
    string WorldId,
    string Map,
    string? IpAddress,
    string? DnsName,
    int? Port,
    string? PlayerSessionId,
    string? ConnectUrl,
    int? RetryAfterSeconds);
