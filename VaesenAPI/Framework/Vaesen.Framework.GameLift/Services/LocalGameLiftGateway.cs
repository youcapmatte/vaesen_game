using Microsoft.Extensions.Options;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

/// <summary>
/// Stand-in for GameLift when there is no fleet: every world is assumed to be running locally on 127.0.0.1:LocalPort,
/// and every instance of a world (e.g. each guild's hub) is the same local server.
/// Player session ids are generated here; the local server runs without GameLift and does not validate them.
/// </summary>
public sealed class LocalGameLiftGateway(IOptions<SessionsOptions> options) : IGameLiftGateway
{
    private const string Host = "127.0.0.1";
    private const string SessionPrefix = "local-";

    public Task<GameSessionInfo?> FindWorldSessionAsync(string sessionName, CancellationToken cancellationToken)
    {
        var world = FindWorld(sessionName);
        return Task.FromResult(world is null ? null : ToSession(world, sessionName));
    }

    public Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, string sessionName, string? guildId, CancellationToken cancellationToken) =>
        Task.FromResult<GameSessionInfo>(ToSession(world, sessionName));

    public Task<PlayerSessionInfo?> CreatePlayerSessionAsync(string gameSessionId, string playerId, CancellationToken cancellationToken)
    {
        var world = FindWorld(gameSessionId[SessionPrefix.Length..])
            ?? throw new InvalidOperationException($"No world for local session '{gameSessionId}'.");
        return Task.FromResult<PlayerSessionInfo?>(
            new PlayerSessionInfo($"psess-local-{Guid.NewGuid():N}", Host, null, world.LocalPort));
    }

    private WorldDefinition? FindWorld(string sessionName) => options.Value.Worlds.FirstOrDefault(w => w.OwnsSession(sessionName));

    private static GameSessionInfo ToSession(WorldDefinition world, string sessionName) =>
        new(SessionPrefix + sessionName, GameSessionState.Active, Host, null, world.LocalPort);
}
