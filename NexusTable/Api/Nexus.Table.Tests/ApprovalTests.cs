using System.Net;
using System.Net.Http.Json;
using Newtonsoft.Json;
using Nexus.Table.Models;
using Nexus.Table.Services;

namespace Nexus.Table.Tests;

/// <summary>
/// The host lets people in: a new sign-in waits, sees only its own account and never shows at the table, until the
/// host approves it on the Admin page.
/// </summary>
public sealed class ApprovalTests : IDisposable
{
    private readonly NexusApiFactory _factory = new() { PlayersWaitForApproval = true };

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task A_new_sign_in_waits_for_the_host_and_is_let_in_or_turned_away_on_the_admin_page()
    {
        var host = await _factory.ClientForAsync("mattias");
        var ingrid = await _factory.ClientForAsync("ingrid");

        // Waiting: her own account, nothing else, and not in the Society.
        var waiting = await NexusApiFactory.ResultAsync<Account>(await ingrid.GetAsync("/api/me"));
        Assert.False(waiting.IsApproved);
        Assert.True((await NexusApiFactory.ResultAsync<Account>(await host.GetAsync("/api/me"))).IsApproved);
        foreach (var refused in new[]
        {
            await ingrid.GetAsync("/api/society"),
            await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()),
            await ingrid.PostAsync("/api/call/join", null),
            await ingrid.GetAsync("/api/rolls"),
            await ingrid.GetAsync("/api/admin/accounts"),
        })
        {
            Assert.Equal(HttpStatusCode.Forbidden, refused.StatusCode);
        }

        var society = await NexusApiFactory.ResultAsync<SocietyView>(await host.GetAsync("/api/society"));
        Assert.DoesNotContain(society.Members, m => m.AccountId == waiting.Id);

        // The host sees her waiting, first, and lets her in.
        var accounts = await NexusApiFactory.ResultAsync<List<Account>>(await host.GetAsync("/api/admin/accounts"));
        Assert.Equal(waiting.Id, accounts[0].Id);
        Assert.False(accounts[0].IsApproved);
        Assert.True((await NexusApiFactory.ResultAsync<Account>(await host.PutAsJsonAsync($"/api/admin/accounts/{waiting.Id}/approval", new { approved = true }))).IsApproved);

        Assert.Equal(HttpStatusCode.OK, (await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic())).StatusCode);
        society = await NexusApiFactory.ResultAsync<SocietyView>(await host.GetAsync("/api/society"));
        Assert.Contains(society.Members, m => m.AccountId == waiting.Id);

        // Turned away: out of the Society and out of the call. The host can't be, and only the host decides.
        _factory.LiveKit.Requests.Clear();
        Assert.False((await NexusApiFactory.ResultAsync<Account>(await host.PutAsJsonAsync($"/api/admin/accounts/{waiting.Id}/approval", new { approved = false }))).IsApproved);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/society")).StatusCode);
        society = await NexusApiFactory.ResultAsync<SocietyView>(await host.GetAsync("/api/society"));
        Assert.DoesNotContain(society.Members, m => m.AccountId == waiting.Id);
        var removal = Assert.Single(_factory.LiveKit.Changes);
        Assert.EndsWith("/twirp/livekit.RoomService/RemoveParticipant", removal.Uri.AbsolutePath, StringComparison.Ordinal);
        Assert.Contains(waiting.Id, removal.Body, StringComparison.Ordinal);

        var self = await NexusApiFactory.ResultAsync<Account>(await host.GetAsync("/api/me"));
        Assert.Equal(HttpStatusCode.BadRequest, (await host.PutAsJsonAsync($"/api/admin/accounts/{self.Id}/approval", new { approved = false })).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PutAsJsonAsync($"/api/admin/accounts/{waiting.Id}/approval", new { approved = true })).StatusCode);
    }

    [Fact]
    public void An_account_stored_before_approval_existed_is_let_in()
    {
        const string stored = "{\"id\":\"account-00000000-0000-0000-0000-000000000009\",\"subject\":\"oauth2|discord|1\",\"displayName\":\"Old friend\",\"role\":\"Player\"}";
        Assert.True(JsonConvert.DeserializeObject<Account>(stored)!.IsApproved);
    }
}

/// <summary>At most five people on the call; the Gamemaster is the extra seat.</summary>
public sealed class FullTableTests : IDisposable
{
    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task Five_people_fit_on_the_call_and_anyone_already_on_it_gets_back_in()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");
        var me = await NexusApiFactory.ResultAsync<Account>(await ingrid.GetAsync("/api/me"));

        // Four others and the Gamemaster: there is room for her.
        _factory.LiveKit.OnCall.AddRange(Enumerable.Range(1, 4).Select(i => $"account-00000000-0000-0000-0000-00000000000{i}"));
        _factory.LiveKit.OnCall.Add("agent-AJ_gamemaster");
        Assert.Equal(HttpStatusCode.OK, (await ingrid.PostAsync("/api/call/join", null)).StatusCode);

        // Five others: the table is full.
        _factory.LiveKit.OnCall.Add("account-00000000-0000-0000-0000-000000000005");
        var full = await ingrid.PostAsync("/api/call/join", null);
        Assert.Equal(HttpStatusCode.BadRequest, full.StatusCode);
        Assert.Equal(CallService.TableFull, (await NexusApiFactory.ReadAsync(full))["errorCode"]!.ToString());

        // One of the five is her (another tab, a reconnect): she gets back in.
        _factory.LiveKit.OnCall[^1] = me.Id;
        Assert.Equal(HttpStatusCode.OK, (await ingrid.PostAsync("/api/call/join", null)).StatusCode);
    }
}
