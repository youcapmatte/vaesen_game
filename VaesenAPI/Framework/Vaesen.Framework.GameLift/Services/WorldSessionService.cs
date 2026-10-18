using System.Collections.Concurrent;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.GameLift.Models;

namespace Vaesen.Framework.GameLift.Services;

/// <summary>
/// Gets characters into world instances: finds (or starts) the instance's game session and reserves a player session
/// on it. Play puts a character back where it just was (if that instance still runs and it left recently), else in its
/// guild's hub. Travelling between worlds is just entering another one; the client then connects to the returned address.
/// </summary>
public sealed class WorldSessionService(
    IGameLiftGateway gameLift,
    ISubscriptionService subscriptions,
    ICharacterStore characters,
    IGuildStore guilds,
    IGuildService guildService,
    TimeProvider timeProvider,
    IOptions<SessionsOptions> options)
{
    // Serialises find-or-create per instance so concurrent joins don't start duplicate game sessions.
    // Only covers a single API instance; multiple instances will need a distributed lock.
    private static readonly ConcurrentDictionary<string, SemaphoreSlim> InstanceLocks = new();

    public IReadOnlyList<WorldDefinition> Worlds => options.Value.Worlds;

    private DateTime Now => timeProvider.GetUtcNow().UtcDateTime;

    /// <summary>Joins a shared world by id (one session for everyone).</summary>
    public async Task<JoinWorldResult> JoinAsync(string worldId, string playerId, CancellationToken cancellationToken)
    {
        var world = FindWorld(worldId);
        if (world is null || world.IsGuildHub)
        {
            return JoinWorldResult.UnknownWorld();
        }

        if (!await subscriptions.HasActiveSubscriptionAsync(playerId, cancellationToken))
        {
            return JoinWorldResult.NoSubscription();
        }

        return await JoinInstanceAsync(world, world.GetSessionName(null), guildId: null, playerId, cancellationToken).ConfigureAwait(false);
    }

    /// <summary>
    /// Puts <paramref name="characterId"/>, played by <paramref name="accountId"/>, into the world: back into the
    /// instance it just left if that's still running and it left within <see cref="SessionsOptions.ReturnWindow"/>,
    /// otherwise its guild's hub. Renews the account's claim on the character and records where it's going.
    /// </summary>
    public async Task<JoinWorldResult> EnterWorldAsync(string accountId, string characterId, CancellationToken cancellationToken)
    {
        var character = Character.IsValidId<Character>(characterId)
            ? await characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false)
            : null;
        if (character is null)
        {
            return JoinWorldResult.CharacterNotFound();
        }

        var guild = await guilds.GetAsync(character.GuildId, cancellationToken).ConfigureAwait(false);
        if (guild is null || !string.Equals(guild.GetPlayerOf(character.Id), accountId, StringComparison.Ordinal))
        {
            return JoinWorldResult.NotYourCharacter();
        }

        if (!await subscriptions.HasActiveSubscriptionAsync(accountId, cancellationToken))
        {
            return JoinWorldResult.NoSubscription();
        }

        // Where to: the instance it just left, if it may return there, else the guild's hub.
        var (world, sessionName) = await GetReturnInstanceAsync(character, cancellationToken).ConfigureAwait(false);
        if (world is null)
        {
            world = Worlds.FirstOrDefault(w => w.IsGuildHub);
            if (world is null)
            {
                return JoinWorldResult.NoHub();
            }

            sessionName = world.GetSessionName(guild.Id);
        }

        // Playing keeps the claim on the character fresh.
        await guildService.AssignPlayerAsync(guild.Id, character.Id, new AssignPlayerRequest { AccountId = accountId }, cancellationToken).ConfigureAwait(false);

        var result = await JoinInstanceAsync(world, sessionName!, guild.Id, character.Id, cancellationToken).ConfigureAwait(false);
        if (result.Status == JoinWorldStatus.Ready)
        {
            await SetLocationAsync(character, new CharacterLocation { WorldId = world.Id, SessionName = sessionName!, EnteredAt = Now }, cancellationToken)
                .ConfigureAwait(false);
        }

        return result with { SessionName = sessionName };
    }

    /// <summary>A game server reports that a character left its world (logged out or lost the connection).</summary>
    public async Task<bool> CharacterLeftAsync(string characterId, CancellationToken cancellationToken)
    {
        var character = await characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false);
        if (character is null)
        {
            return false;
        }

        if (character.LastLocation is { LeftAt: null } location)
        {
            location.LeftAt = Now;
            await SetLocationAsync(character, location, cancellationToken).ConfigureAwait(false);
        }

        // Free the character for the guild's other members; Play claims it again when its player comes back.
        await guildService.ReleasePlayerAsync(character.GuildId, character.Id, cancellationToken).ConfigureAwait(false);
        return true;
    }

    /// <summary>A game server reports the characters that are in its world: their players' claims stay fresh.</summary>
    public async Task HeartbeatAsync(IReadOnlyCollection<string> characterIds, CancellationToken cancellationToken)
    {
        foreach (var character in await characters.GetManyAsync(characterIds, cancellationToken).ConfigureAwait(false))
        {
            var guild = await guilds.GetAsync(character.GuildId, cancellationToken).ConfigureAwait(false);
            var player = guild?.GetPlayerOf(character.Id);
            if (player is not null)
            {
                await guildService.AssignPlayerAsync(guild!.Id, character.Id, new AssignPlayerRequest { AccountId = player }, cancellationToken).ConfigureAwait(false);
            }
        }
    }

    private async Task<(WorldDefinition? World, string? SessionName)> GetReturnInstanceAsync(Character character, CancellationToken cancellationToken)
    {
        var location = character.LastLocation;
        if (location is null || (location.LeftAt is { } leftAt && Now - leftAt > options.Value.ReturnWindow))
        {
            return (null, null);
        }

        var world = FindWorld(location.WorldId);
        if (world is null || !world.OwnsSession(location.SessionName))
        {
            return (null, null);
        }

        // Only while it's still running: a closed instance (a finished dungeon) can't be returned to.
        var session = await gameLift.FindWorldSessionAsync(location.SessionName, cancellationToken).ConfigureAwait(false);
        return session is null ? (null, null) : (world, location.SessionName);
    }

    private async Task<JoinWorldResult> JoinInstanceAsync(WorldDefinition world, string sessionName, string? guildId, string playerId, CancellationToken cancellationToken)
    {
        var instanceLock = InstanceLocks.GetOrAdd(sessionName, _ => new SemaphoreSlim(1, 1));
        GameSessionInfo session;
        await instanceLock.WaitAsync(cancellationToken);
        try
        {
            session = await gameLift.FindWorldSessionAsync(sessionName, cancellationToken)
                ?? await gameLift.CreateWorldSessionAsync(world, sessionName, guildId, cancellationToken);
        }
        finally
        {
            instanceLock.Release();
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

    private async Task SetLocationAsync(Character character, CharacterLocation location, CancellationToken cancellationToken)
    {
        character.LastLocation = location;
        character.UpdatedAt = Now;
        await characters.UpdateAsync(character, character.Name, cancellationToken).ConfigureAwait(false);
    }

    private WorldDefinition? FindWorld(string worldId) =>
        Worlds.FirstOrDefault(w => string.Equals(w.Id, worldId, StringComparison.OrdinalIgnoreCase));
}

public enum JoinWorldStatus
{
    Ready,
    Starting,
    Full,
    UnknownWorld,
    NoSubscription,
    CharacterNotFound,
    NotYourCharacter,
    NoHub,
}

public sealed record JoinWorldResult(JoinWorldStatus Status, WorldDefinition? World, PlayerSessionInfo? PlayerSession)
{
    /// <summary>The instance (game session name) entered, for EnterWorld.</summary>
    public string? SessionName { get; init; }

    public static JoinWorldResult Ready(WorldDefinition world, PlayerSessionInfo session) => new(JoinWorldStatus.Ready, world, session);
    public static JoinWorldResult Starting(WorldDefinition world) => new(JoinWorldStatus.Starting, world, null);
    public static JoinWorldResult Full(WorldDefinition world) => new(JoinWorldStatus.Full, world, null);
    public static JoinWorldResult UnknownWorld() => new(JoinWorldStatus.UnknownWorld, null, null);
    public static JoinWorldResult NoSubscription() => new(JoinWorldStatus.NoSubscription, null, null);
    public static JoinWorldResult CharacterNotFound() => new(JoinWorldStatus.CharacterNotFound, null, null);
    public static JoinWorldResult NotYourCharacter() => new(JoinWorldStatus.NotYourCharacter, null, null);
    public static JoinWorldResult NoHub() => new(JoinWorldStatus.NoHub, null, null);
}
