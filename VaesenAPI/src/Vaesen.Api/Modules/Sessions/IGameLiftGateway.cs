namespace Vaesen.Api.Modules.Sessions;

/// <summary>
/// The GameLift calls the Sessions module needs. Kept narrow so it can be faked in tests.
/// </summary>
public interface IGameLiftGateway
{
    /// <summary>Returns the ACTIVE or ACTIVATING game session hosting <paramref name="worldId"/>, if any.</summary>
    Task<GameSessionInfo?> FindWorldSessionAsync(string worldId, CancellationToken cancellationToken);

    Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, CancellationToken cancellationToken);

    /// <summary>Reserves a slot for the player. Returns null if the game session is full.</summary>
    Task<PlayerSessionInfo?> CreatePlayerSessionAsync(string gameSessionId, string playerId, CancellationToken cancellationToken);
}

public enum GameSessionState
{
    Activating,
    Active,
}

public sealed record GameSessionInfo(
    string GameSessionId,
    GameSessionState State,
    string? IpAddress,
    string? DnsName,
    int Port);

public sealed record PlayerSessionInfo(
    string PlayerSessionId,
    string? IpAddress,
    string? DnsName,
    int Port);
