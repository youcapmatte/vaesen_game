using Amazon.GameLift;
using Amazon.GameLift.Model;
using Microsoft.Extensions.Options;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

public sealed class AwsGameLiftGateway(IAmazonGameLift gameLift, IOptions<SessionsOptions> options) : IGameLiftGateway
{
    /// <summary>Game property keys the Unreal server reads in OnStartGameSession.</summary>
    public const string WorldIdProperty = "worldId";
    public const string MapProperty = "map";

    private GameLiftOptions GameLift => options.Value.GameLift;

    public async Task<GameSessionInfo?> FindWorldSessionAsync(string worldId, CancellationToken cancellationToken)
    {
        string? nextToken = null;
        do
        {
            var response = await gameLift.DescribeGameSessionsAsync(new DescribeGameSessionsRequest
            {
                FleetId = GameLift.FleetId,
                AliasId = GameLift.AliasId,
                Location = NullIfEmpty(GameLift.Location),
                NextToken = nextToken,
            }, cancellationToken);

            // AWS SDK v4 returns null (not an empty list) when there are no sessions.
            var match = (response.GameSessions ?? []).FirstOrDefault(s =>
                s.Name == worldId &&
                (s.Status == GameSessionStatus.ACTIVE || s.Status == GameSessionStatus.ACTIVATING));
            if (match is not null)
            {
                return ToInfo(match);
            }

            nextToken = response.NextToken;
        } while (!string.IsNullOrEmpty(nextToken));

        return null;
    }

    public async Task<GameSessionInfo> CreateWorldSessionAsync(WorldDefinition world, CancellationToken cancellationToken)
    {
        var response = await gameLift.CreateGameSessionAsync(new CreateGameSessionRequest
        {
            FleetId = GameLift.FleetId,
            AliasId = GameLift.AliasId,
            Location = NullIfEmpty(GameLift.Location),
            Name = world.Id,
            MaximumPlayerSessionCount = world.MaxPlayers,
            GameProperties =
            [
                new GameProperty { Key = WorldIdProperty, Value = world.Id },
                new GameProperty { Key = MapProperty, Value = world.Map },
            ],
        }, cancellationToken);

        return ToInfo(response.GameSession);
    }

    public async Task<PlayerSessionInfo?> CreatePlayerSessionAsync(string gameSessionId, string playerId, CancellationToken cancellationToken)
    {
        try
        {
            var response = await gameLift.CreatePlayerSessionAsync(new CreatePlayerSessionRequest
            {
                GameSessionId = gameSessionId,
                PlayerId = playerId,
            }, cancellationToken);

            var session = response.PlayerSession;
            return new PlayerSessionInfo(session.PlayerSessionId, session.IpAddress, session.DnsName, session.Port ?? 0);
        }
        catch (GameSessionFullException)
        {
            return null;
        }
    }

    private static GameSessionInfo ToInfo(GameSession session) => new(
        session.GameSessionId,
        session.Status == GameSessionStatus.ACTIVE ? GameSessionState.Active : GameSessionState.Activating,
        session.IpAddress,
        session.DnsName,
        session.Port ?? 0);

    private static string? NullIfEmpty(string? value) => string.IsNullOrWhiteSpace(value) ? null : value;
}
