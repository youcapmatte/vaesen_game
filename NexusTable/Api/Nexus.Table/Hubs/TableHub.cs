using System.Collections.Concurrent;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.SignalR;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Core.Security;

namespace Nexus.Table.Hubs;

/// <summary>
/// Live updates for everyone at the table, at /hubs/table. Server to clients:
///   "societyChanged"(societyId)          members or the Society changed; reload it
///   "characterChanged"(character)        a sheet changed (Conditions, notes, ...), dark secret left out
///   "rollMade"(roll)                     a roll or a push, to animate the dice for everyone
///   "presenceChanged"(presence[])        who is at the table now, and who holds push-to-talk
///   "sessionChanged"(session)            a session started or ended, or a test was asked for or rolled (no play state)
///   "sessionEvent"(event)                a new line in the session log (host-only lines are not sent)
/// Clients to server:
///   SetPushToTalk(bool)                  the player holds (or released) the talk-to-the-Gamemaster key
/// The token goes in the access_token query parameter (browsers can't set headers on WebSockets).
/// </summary>
[Authorize(Policy = Policies.Player)]
public sealed class TableHub(IOptions<NexusOptions> options, IAccountStore accounts, TablePresence presence) : Hub
{
    public const string Path = "/hubs/table";

    public override async Task OnConnectedAsync()
    {
        // The account exists by now: the web app reads /api/me before it connects. Only accounts the host let in get the table.
        var account = await accounts.GetBySubjectAsync(Context.User!.GetSubject(), Context.ConnectionAborted).ConfigureAwait(false);
        if (account is not { IsApproved: true })
        {
            Context.Abort();
            return;
        }

        var group = GroupOf(options.Value.SocietyId);
        await Groups.AddToGroupAsync(Context.ConnectionId, group).ConfigureAwait(false);
        presence.Join(Context.ConnectionId, account.Id);
        await Clients.Group(group).SendAsync("presenceChanged", presence.Snapshot()).ConfigureAwait(false);

        await base.OnConnectedAsync().ConfigureAwait(false);
    }

    public override async Task OnDisconnectedAsync(Exception? exception)
    {
        if (presence.Leave(Context.ConnectionId))
        {
            await Clients.Group(GroupOf(options.Value.SocietyId)).SendAsync("presenceChanged", presence.Snapshot()).ConfigureAwait(false);
        }

        await base.OnDisconnectedAsync(exception).ConfigureAwait(false);
    }

    /// <summary>Who is here right now; a client calls it once after connecting.</summary>
    public IReadOnlyList<PresenceEntry> GetPresence() => presence.Snapshot();

    /// <summary>Shows the table who is talking to the Gamemaster. The audio itself will go through LiveKit.</summary>
    public async Task SetPushToTalk(bool active)
    {
        if (presence.SetTalking(Context.ConnectionId, active))
        {
            await Clients.Group(GroupOf(options.Value.SocietyId)).SendAsync("presenceChanged", presence.Snapshot()).ConfigureAwait(false);
        }
    }

    public static string GroupOf(string societyId) => $"society:{societyId}";
}

/// <summary>One person at the table. A person with several tabs open counts once.</summary>
public sealed class PresenceEntry
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("talkingToGm")]
    public bool TalkingToGm { get; set; }
}

/// <summary>Who is connected to the table hub, in this process. Enough for one API instance; use a backplane for more.</summary>
public sealed class TablePresence
{
    private readonly ConcurrentDictionary<string, (string AccountId, bool Talking)> _connections = new(StringComparer.Ordinal);

    public void Join(string connectionId, string accountId) => _connections[connectionId] = (accountId, false);

    public bool Leave(string connectionId) => _connections.TryRemove(connectionId, out _);

    /// <summary>False when the connection is unknown or nothing changed.</summary>
    public bool SetTalking(string connectionId, bool talking)
    {
        if (!_connections.TryGetValue(connectionId, out var entry) || entry.Talking == talking)
        {
            return false;
        }

        _connections[connectionId] = entry with { Talking = talking };
        return true;
    }

    public IReadOnlyList<PresenceEntry> Snapshot() => _connections.Values
        .GroupBy(c => c.AccountId, StringComparer.Ordinal)
        .Select(g => new PresenceEntry { AccountId = g.Key, TalkingToGm = g.Any(c => c.Talking) })
        .OrderBy(p => p.AccountId, StringComparer.Ordinal)
        .ToList();
}

public sealed class SignalRTableNotifier(IHubContext<TableHub> hub) : ITableNotifier
{
    public Task SocietyChangedAsync(string societyId, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(societyId)).SendAsync("societyChanged", societyId, cancellationToken);

    public Task CharacterChangedAsync(Character character, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(character.SocietyId)).SendAsync("characterChanged", character.VisibleTo(null), cancellationToken);

    public Task RollMadeAsync(Roll roll, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(roll.SocietyId)).SendAsync("rollMade", roll, cancellationToken);

    public Task SessionChangedAsync(PlaySession session, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(session.SocietyId)).SendAsync("sessionChanged", session.ForPlayers(), cancellationToken);

    public Task SessionEventAsync(SessionEvent sessionEvent, CancellationToken cancellationToken = default) =>
        sessionEvent.HostOnly
            ? Task.CompletedTask
            : hub.Clients.Group(TableHub.GroupOf(sessionEvent.SocietyId)).SendAsync("sessionEvent", sessionEvent, cancellationToken);
}
