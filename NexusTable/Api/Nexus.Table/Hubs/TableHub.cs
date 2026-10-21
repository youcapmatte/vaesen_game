using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.SignalR;
using Microsoft.Extensions.Options;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Core.Security;

namespace Nexus.Table.Hubs;

/// <summary>
/// Live updates for everyone at the table, at /hubs/table. Clients only listen:
///   "societyChanged"(societyId)   members or the Society changed; reload it
///   "characterChanged"(character) a sheet changed (Conditions, notes, ...)
///   "rollMade"(roll)              a roll or a push, to animate the dice for everyone
/// The token goes in the access_token query parameter (browsers can't set headers on WebSockets).
/// </summary>
[Authorize(Policy = Policies.Player)]
public sealed class TableHub(IOptions<NexusOptions> options) : Hub
{
    public const string Path = "/hubs/table";

    public override async Task OnConnectedAsync()
    {
        await Groups.AddToGroupAsync(Context.ConnectionId, GroupOf(options.Value.SocietyId)).ConfigureAwait(false);
        await base.OnConnectedAsync().ConfigureAwait(false);
    }

    public static string GroupOf(string societyId) => $"society:{societyId}";
}

public sealed class SignalRTableNotifier(IHubContext<TableHub> hub) : ITableNotifier
{
    public Task SocietyChangedAsync(string societyId, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(societyId)).SendAsync("societyChanged", societyId, cancellationToken);

    public Task CharacterChangedAsync(Character character, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(character.SocietyId)).SendAsync("characterChanged", character, cancellationToken);

    public Task RollMadeAsync(Roll roll, CancellationToken cancellationToken = default) =>
        hub.Clients.Group(TableHub.GroupOf(roll.SocietyId)).SendAsync("rollMade", roll, cancellationToken);
}
