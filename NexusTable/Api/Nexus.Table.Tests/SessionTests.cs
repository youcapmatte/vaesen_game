using System.Net;
using System.Net.Http.Json;
using Microsoft.Extensions.DependencyInjection;
using Nexus.Table.Api;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Models;

namespace Nexus.Table.Tests;

/// <summary>
/// Sessions: a mystery played over several evenings, each resuming from the play state the last one left; the log of
/// everything said, rolled and noted; tests the Gamemaster asks for, with the sixes they need.
/// </summary>
public sealed class SessionTests : IDisposable
{
    private const string SocietyId = "society-00000000-0000-0000-0000-000000000001";

    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    private HttpClient Agent()
    {
        var agent = _factory.CreateClient();
        agent.DefaultRequestHeaders.Add(AgentController.KeyHeader, NexusApiFactory.AgentKey);
        return agent;
    }

    private async Task<HttpClient> HostWithCaseAsync()
    {
        var host = await _factory.ClientForAsync("mattias");
        await _factory.Services.GetRequiredService<ICasePacketStore>().SaveAsync(new StoredCase
        {
            Packet = new CasePacket
            {
                MysteryId = "mystery-bells",
                SocietyId = SocietyId,
                Title = "The Drowned Bells",
                Hook = new CaseHook { Text = "Come at once: the bells of Forsby ring under the lake." },
                Validation = new CaseValidation { Passed = true },
            },
        });
        Assert.Equal(HttpStatusCode.OK, (await host.PostAsync("/api/mysteries/mystery-bells/table", null)).StatusCode);
        return host;
    }

    [Fact]
    public async Task A_mystery_runs_over_sessions_each_resuming_from_the_state_the_last_left_and_the_log_exports()
    {
        var host = await HostWithCaseAsync();
        var ingrid = await _factory.ClientForAsync("ingrid");
        var agent = Agent();

        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsync("/api/sessions/start", null)).StatusCode);
        var first = await NexusApiFactory.ResultAsync<PlaySession>(await host.PostAsync("/api/sessions/start", null));
        Assert.Equal(1, first.Number);

        // The table talks to the Gamemaster, the host leaves a note, and the scribe writes where things stand.
        await agent.PostAsJsonAsync("/api/agent/events", new AgentEventsRequest
        {
            Events =
            [
                new() { Kind = SessionEventKinds.PlayerSpoke, Speaker = "Elin Gregorius (Ingrid)", Text = "I ask the ferryman about the bells." },
                new() { Kind = SessionEventKinds.GmSaid, Speaker = "The Gamemaster", Text = "Ferryman: I hear them every night, miss." },
            ],
        });
        var note = await NexusApiFactory.ResultAsync<SessionEvent>(await host.PostAsJsonAsync("/api/sessions/current/notes", new { text = "We agreed Elin keeps the ferryman's key." }));
        Assert.Equal(note.Text, (await NexusApiFactory.ResultAsync<SessionEvent>(await agent.GetAsync($"/api/agent/events/{note.Id}"))).Text);
        await agent.PutAsJsonAsync($"/api/agent/sessions/{first.Id}/state", new PlayState
        {
            Summary = "The Society reached Forsby and heard of the bells from the ferryman.",
            Scene = "The ferry landing, the first evening",
            FactsFound = [new FoundFact { FactId = "f1", How = "the ferryman told Elin", Session = 1 }],
        });

        // Players see the open session without the play state; the log without the scribe's lines.
        var asPlayer = await NexusApiFactory.ResultAsync<PlaySession>(await ingrid.GetAsync("/api/sessions/current"));
        Assert.Equal("", asPlayer.State.Summary);
        var playerLog = await NexusApiFactory.ResultAsync<List<SessionEvent>>(await ingrid.GetAsync($"/api/sessions/{first.Id}/log"));
        Assert.Contains(playerLog, e => e.Kind == SessionEventKinds.Note);
        Assert.DoesNotContain(playerLog, e => e.HostOnly);

        Assert.Equal(HttpStatusCode.OK, (await host.PostAsync($"/api/sessions/{first.Id}/end", null)).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync($"/api/sessions/{first.Id}/log")).StatusCode);

        // Bringing in the Gamemaster opens the next session, which starts from where the last one ended.
        Assert.Equal(HttpStatusCode.OK, (await host.PostAsync("/api/call/gamemaster", null)).StatusCode);
        var table = await NexusApiFactory.ResultAsync<GmTable>(await agent.GetAsync("/api/agent/table"));
        Assert.Equal(2, table.Session!.Number);
        Assert.Equal("The ferry landing, the first evening", table.Session.State.Scene);
        Assert.Contains(table.RecentLog, l => l.Contains("Session 2", StringComparison.Ordinal));

        var export = await NexusApiFactory.ResultAsync<string>(await host.GetAsync("/api/sessions/mystery/mystery-bells/export"));
        Assert.Contains("## Session 1", export, StringComparison.Ordinal);
        Assert.Contains("## Session 2", export, StringComparison.Ordinal);
        Assert.Contains("I ask the ferryman about the bells.", export, StringComparison.Ordinal);
        Assert.Contains("**Note from Mattias (the host):** We agreed Elin keeps the ferryman's key.", export, StringComparison.Ordinal);
        Assert.Contains("f1 (session 1): the ferryman told Elin", export, StringComparison.Ordinal);
        Assert.Equal(2, (await NexusApiFactory.ResultAsync<List<PlaySession>>(await host.GetAsync("/api/sessions/mystery/mystery-bells"))).Count);
    }

    [Fact]
    public async Task A_test_the_gamemaster_asks_for_shows_the_dice_and_the_sixes_it_needs_and_the_roll_answers_it()
    {
        var host = await HostWithCaseAsync();
        var ingrid = await _factory.ClientForAsync("ingrid");
        var elin = await NexusApiFactory.ResultAsync<Character>(await ingrid.PostAsJsonAsync("/api/characters", Requests.Academic()));
        await host.PostAsync("/api/sessions/start", null);

        var test = await NexusApiFactory.ResultAsync<TestRequest>(await Agent().PostAsJsonAsync("/api/agent/tests",
            new TestRequestInput { Character = "Elin", Skill = "Learning", Difficulty = -1, SuccessesNeeded = 2, Reason = "the old parish books" }));

        // Logic 5 + Learning 3 - 1 = 7 dice, two sixes needed.
        Assert.Equal(7, test.PoolSize);
        Assert.Equal(2, test.SuccessesNeeded);
        Assert.Single((await NexusApiFactory.ResultAsync<PlaySession>(await ingrid.GetAsync("/api/sessions/current"))).PendingTests);

        _factory.Dice.Queue(6, 1, 2, 3, 4, 5, 2);
        var outcome = await NexusApiFactory.ResultAsync<RollOutcome>(await ingrid.PostAsJsonAsync("/api/rolls", new { characterId = elin.Id, testId = test.Id }));

        Assert.Equal("learning", outcome.Roll.Skill);
        Assert.Equal(7, outcome.Roll.PoolSize);
        Assert.Equal(1, outcome.Roll.Successes);
        Assert.Equal(2, outcome.Roll.SuccessesNeeded);
        Assert.False(outcome.Roll.Succeeded);
        var session = await NexusApiFactory.ResultAsync<PlaySession>(await host.GetAsync("/api/sessions/current"));
        Assert.Empty(session.PendingTests);
        var log = await NexusApiFactory.ResultAsync<List<SessionEvent>>(await host.GetAsync($"/api/sessions/{session.Id}/log"));
        Assert.Contains(log, e => e.Kind == SessionEventKinds.TestRequested && e.Text.Contains("2 sixes needed", StringComparison.Ordinal));
        Assert.Contains(log, e => e.Kind == SessionEventKinds.Roll && e.RollId == outcome.Roll.Id && e.Text.Contains("1 of 2 sixes needed, failure", StringComparison.Ordinal));

        // A failed roll can still be pushed; the answered test cannot be rolled again.
        Assert.Equal(HttpStatusCode.BadRequest, (await ingrid.PostAsJsonAsync("/api/rolls", new { characterId = elin.Id, testId = test.Id })).StatusCode);
    }

    [Fact]
    public async Task Without_a_case_at_the_table_there_is_no_session_and_notes_say_so()
    {
        var host = await _factory.ClientForAsync("mattias");

        Assert.Equal(HttpStatusCode.BadRequest, (await host.PostAsync("/api/sessions/start", null)).StatusCode);
        Assert.Equal(HttpStatusCode.BadRequest, (await host.PostAsJsonAsync("/api/sessions/current/notes", new { text = "Hello" })).StatusCode);
        Assert.Null(await NexusApiFactory.ResultAsync<PlaySession?>(await host.GetAsync("/api/sessions/current")));
    }
}
