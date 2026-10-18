using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

/// <summary>
/// The GameLift calls the Sessions module needs. Kept narrow so it can be faked in tests.
/// </summary>
public interface IGameLiftGateway
{
    /// <summary>Returns the ACTIVE or ACTIVATING game session named <paramref name="sessionName"/>, if any.</summary>
    Task<GameSessionInfo?> FindWorldSessionAsync(string sessionName, CancellationToken cancellationToken);

    /// <summary>Starts a game session named <paramref name="sessionName"/> running <paramref name="world"/>'s map.</summary>
    Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, string sessionName, string? guildId, CancellationToken cancellationToken);

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
