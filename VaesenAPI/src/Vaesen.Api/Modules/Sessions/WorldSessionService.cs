using System.Collections.Concurrent;
using Microsoft.Extensions.Options;
using Vaesen.Api.Modules.Billing;

namespace Vaesen.Api.Modules.Sessions;

/// <summary>
/// Gets a player into a persistent world: finds (or starts) the world's game session and reserves a player session on it.
/// Travelling between worlds is just joining another world; the client then connects to the returned address.
/// </summary>
public sealed class WorldSessionService(
    IGameLiftGateway gameLift,
    ISubscriptionService subscriptions,
    IOptions<SessionsOptions> options)
{
    // Serialises find-or-create per world so concurrent joins don't start duplicate world sessions.
    // Only covers a single API instance; multiple instances will need a distributed lock.
    private static readonly ConcurrentDictionary<string, SemaphoreSlim> WorldLocks = new();

    public IReadOnlyList<WorldDefinition> Worlds => options.Value.Worlds;

    public async Task<JoinWorldResult> JoinAsync(string worldId, string playerId, CancellationToken cancellationToken)
    {
        var world = Worlds.FirstOrDefault(w => string.Equals(w.Id, worldId, StringComparison.OrdinalIgnoreCase));
        if (world is null)
        {
            return JoinWorldResult.UnknownWorld();
        }

        if (!await subscriptions.HasActiveSubscriptionAsync(playerId, cancellationToken))
        {
            return JoinWorldResult.NoSubscription();
        }

        var worldLock = WorldLocks.GetOrAdd(world.Id, _ => new SemaphoreSlim(1, 1));
        GameSessionInfo session;
        await worldLock.WaitAsync(cancellationToken);
        try
        {
            session = await gameLift.FindWorldSessionAsync(world.Id, cancellationToken)
                ?? await gameLift.CreateWorldSessionAsync(world, cancellationToken);
        }
        finally
        {
            worldLock.Release();
        }

        if (session.State != GameSessionState.Active)
        {
            return JoinWorldResult.Starting(world);
        }

        var playerSession = await gameLift.CreatePlayerSessionAsync(session.GameSessionId, playerId, cancellationToken);
        return playerSession is null
            ? JoinWorldResult.Full(world)
            : JoinWorldResult.Ready(world, playerSession);
    }
}

public enum JoinWorldStatus
{
    Ready,
    Starting,
    Full,
    UnknownWorld,
    NoSubscription,
}

public sealed record JoinWorldResult(JoinWorldStatus Status, WorldDefinition? World, PlayerSessionInfo? PlayerSession)
{
    public static JoinWorldResult Ready(WorldDefinition world, PlayerSessionInfo session) => new(JoinWorldStatus.Ready, world, session);
    public static JoinWorldResult Starting(WorldDefinition world) => new(JoinWorldStatus.Starting, world, null);
    public static JoinWorldResult Full(WorldDefinition world) => new(JoinWorldStatus.Full, world, null);
    public static JoinWorldResult UnknownWorld() => new(JoinWorldStatus.UnknownWorld, null, null);
    public static JoinWorldResult NoSubscription() => new(JoinWorldStatus.NoSubscription, null, null);
}
