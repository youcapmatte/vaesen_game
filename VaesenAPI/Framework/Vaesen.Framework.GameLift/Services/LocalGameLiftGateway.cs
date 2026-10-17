using Microsoft.Extensions.Options;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

/// <summary>
/// Stand-in for GameLift when there is no fleet: every world is assumed to be running locally on 127.0.0.1:LocalPort.
/// Player session ids are generated here; the local server runs without GameLift and does not validate them.
/// </summary>
public sealed class LocalGameLiftGateway(IOptions<SessionsOptions> options) : IGameLiftGateway
{
    private const string Host = "127.0.0.1";

    public Task<GameSessionInfo?> FindWorldSessionAsync(string worldId, CancellationToken cancellationToken)
    {
        var world = options.Value.Worlds.FirstOrDefault(w => w.Id == worldId);
        return Task.FromResult(world is null ? null : ToSession(world));
    }

    public Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, CancellationToken cancellationToken) =>
        Task.FromResult<GameSessionInfo>(ToSession(world));

    public Task<PlayerSessionInfo?> CreatePlayerSessionAsync(string gameSessionId, string playerId, CancellationToken cancellationToken)
    {
        var world = options.Value.Worlds.Single(w => $"local-{w.Id}" == gameSessionId);
        return Task.FromResult<PlayerSessionInfo?>(
            new PlayerSessionInfo($"psess-local-{Guid.NewGuid():N}", Host, null, world.LocalPort));
    }

    private static GameSessionInfo ToSession(WorldDefinition world) =>
        new($"local-{world.Id}", GameSessionState.Active, Host, null, world.LocalPort);
}
