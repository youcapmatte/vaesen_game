using System.Security.Claims;
using System.Text.Encodings.Web;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Vaesen.Api.Infrastructure.Persistence;
using Vaesen.Api.Modules.Players;
using Vaesen.Api.Modules.Sessions;

namespace Vaesen.Api.Tests;

/// <summary>
/// Runs the API in-process (Development environment) with a fake GameLift, in-memory storage and a header-driven
/// auth scheme instead of Auth0.
/// Pass useTestAuth: false to exercise the real token validation (dev login tokens).
/// </summary>
public sealed class VaesenApiFactory(bool useTestAuth = true) : WebApplicationFactory<Program>
{
    public FakeGameLiftGateway GameLift { get; } = new();

    public HttpClient CreatePlayerClient(string playerId = "auth0|player-1")
    {
        var client = CreateClient();
        client.DefaultRequestHeaders.Add(TestAuthHandler.SubHeader, playerId);
        return client;
    }

    public HttpClient CreateMachineClient(string scope)
    {
        var client = CreateClient();
        client.DefaultRequestHeaders.Add(TestAuthHandler.SubHeader, "server-client-id@clients");
        client.DefaultRequestHeaders.Add(TestAuthHandler.GtyHeader, "client-credentials");
        client.DefaultRequestHeaders.Add(TestAuthHandler.ScopeHeader, scope);
        return client;
    }

    protected override void ConfigureWebHost(IWebHostBuilder builder)
    {
        builder.ConfigureTestServices(services =>
        {
            services.AddSingleton<IGameLiftGateway>(GameLift);

            // No DynamoDB Local in endpoint tests: in-memory repositories, and don't try to create the table.
            services.AddSingleton<ICharacterRepository, InMemoryCharacterRepository>();
            services.RemoveAll<IHostedService>();

            if (!useTestAuth)
            {
                return;
            }

            services
                .AddAuthentication(TestAuthHandler.SchemeName)
                .AddScheme<AuthenticationSchemeOptions, TestAuthHandler>(TestAuthHandler.SchemeName, _ => { });
            services.PostConfigure<AuthenticationOptions>(options =>
            {
                options.DefaultScheme = TestAuthHandler.SchemeName;
                options.DefaultAuthenticateScheme = TestAuthHandler.SchemeName;
                options.DefaultChallengeScheme = TestAuthHandler.SchemeName;
            });
        });
    }
}

public sealed class TestAuthHandler(
    IOptionsMonitor<AuthenticationSchemeOptions> options,
    ILoggerFactory logger,
    UrlEncoder encoder)
    : AuthenticationHandler<AuthenticationSchemeOptions>(options, logger, encoder)
{
    public const string SchemeName = "Test";
    public const string SubHeader = "X-Test-Sub";
    public const string GtyHeader = "X-Test-Gty";
    public const string ScopeHeader = "X-Test-Scope";

    protected override Task<AuthenticateResult> HandleAuthenticateAsync()
    {
        if (!Request.Headers.TryGetValue(SubHeader, out var sub))
        {
            return Task.FromResult(AuthenticateResult.NoResult());
        }

        var claims = new List<Claim> { new("sub", sub.ToString()) };
        if (Request.Headers.TryGetValue(GtyHeader, out var gty))
        {
            claims.Add(new Claim("gty", gty.ToString()));
        }
        if (Request.Headers.TryGetValue(ScopeHeader, out var scope))
        {
            claims.Add(new Claim("scope", scope.ToString()));
        }

        var principal = new ClaimsPrincipal(new ClaimsIdentity(claims, SchemeName));
        return Task.FromResult(AuthenticateResult.Success(new AuthenticationTicket(principal, SchemeName)));
    }
}

public sealed class FakeGameLiftGateway : IGameLiftGateway
{
    public Dictionary<string, GameSessionInfo> SessionsByWorld { get; } = new();
    public HashSet<string> FullSessions { get; } = [];
    public List<WorldDefinition> CreatedWorlds { get; } = [];

    public Task<GameSessionInfo?> FindWorldSessionAsync(string worldId, CancellationToken cancellationToken) =>
        Task.FromResult(SessionsByWorld.GetValueOrDefault(worldId));

    public Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, CancellationToken cancellationToken)
    {
        CreatedWorlds.Add(world);
        var session = new GameSessionInfo($"gsess-{world.Id}", GameSessionState.Activating, null, null, 0);
        SessionsByWorld[world.Id] = session;
        return Task.FromResult(session);
    }

    public Task<PlayerSessionInfo?> CreatePlayerSessionAsync(string gameSessionId, string playerId, CancellationToken cancellationToken)
    {
        if (FullSessions.Contains(gameSessionId))
        {
            return Task.FromResult<PlayerSessionInfo?>(null);
        }

        var session = SessionsByWorld.Values.Single(s => s.GameSessionId == gameSessionId);
        return Task.FromResult<PlayerSessionInfo?>(
            new PlayerSessionInfo($"psess-{playerId}", session.IpAddress, session.DnsName, session.Port));
    }
}
