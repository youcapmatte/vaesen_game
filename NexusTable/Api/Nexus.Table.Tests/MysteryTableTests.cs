using System.Net;
using System.Net.Http.Json;
using Microsoft.Extensions.DependencyInjection;
using Newtonsoft.Json;
using Nexus.Table.Api;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Models;

namespace Nexus.Table.Tests;

/// <summary>The lab (cases written for review), exporting them, and bringing a case to the table.</summary>
public sealed class MysteryTableTests : IDisposable
{
    private const string SocietyId = "society-00000000-0000-0000-0000-000000000001";
    private const string LabId = SocietyId + "-lab";

    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task Players_cannot_use_the_lab_export_or_choose_the_case_at_the_table()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");

        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries?lab=true")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsJsonAsync("/api/mysteries/lab", new { count = 1 })).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsJsonAsync("/api/mysteries/export", new { mysteryIds = new[] { "mystery-x" } })).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsync("/api/mysteries/mystery-x/table", null)).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.DeleteAsync("/api/mysteries/table")).StatusCode);
    }

    [Fact]
    public async Task A_lab_case_brought_to_the_table_shows_players_only_its_title_and_hook_and_gives_the_gm_the_case()
    {
        var host = await _factory.ClientForAsync("mattias");
        var ingrid = await _factory.ClientForAsync("ingrid");
        await SaveAsync(Case("mystery-lab-1", LabId, "The Drowned Bells"));

        Assert.Single(await NexusApiFactory.ResultAsync<List<CaseSummary>>(await host.GetAsync("/api/mysteries?lab=true")));
        Assert.Empty(await NexusApiFactory.ResultAsync<List<CaseSummary>>(await host.GetAsync("/api/mysteries")));

        var brought = await host.PostAsync("/api/mysteries/mystery-lab-1/table", null);

        Assert.Equal(HttpStatusCode.OK, brought.StatusCode);
        var view = await NexusApiFactory.ResultAsync<SocietyView>(await ingrid.GetAsync("/api/society"));
        Assert.Equal("The Drowned Bells", view.Society.AtTable!.Title);
        Assert.Equal("Come at once: the bells of Forsby ring under the lake.", view.Society.AtTable.HookText);
        var asPlayerSees = await (await ingrid.GetAsync("/api/society")).Content.ReadAsStringAsync();
        Assert.DoesNotContain("The miller drowned them", asPlayerSees, StringComparison.Ordinal);

        // Played cases are the Society's own: the case moved out of the lab's shelf into the Society's.
        Assert.Contains("mystery-lab-1", view.Society.MysteryIds);
        Assert.Single(await NexusApiFactory.ResultAsync<List<CaseSummary>>(await host.GetAsync("/api/mysteries")));

        var agent = _factory.CreateClient();
        agent.DefaultRequestHeaders.Add(AgentController.KeyHeader, NexusApiFactory.AgentKey);
        var table = await NexusApiFactory.ResultAsync<GmTable>(await agent.GetAsync("/api/agent/table"));
        Assert.Equal("The Drowned Bells", table.Mystery!.Title);
        Assert.Contains("The miller drowned them", table.Mystery.CaseFile, StringComparison.Ordinal);

        Assert.Equal(HttpStatusCode.OK, (await host.DeleteAsync("/api/mysteries/table")).StatusCode);
        Assert.Null((await NexusApiFactory.ResultAsync<SocietyView>(await ingrid.GetAsync("/api/society"))).Society.AtTable);
        Assert.Null((await NexusApiFactory.ResultAsync<GmTable>(await agent.GetAsync("/api/agent/table"))).Mystery);
    }

    [Fact]
    public async Task A_case_that_failed_its_checks_cannot_be_played()
    {
        var host = await _factory.ClientForAsync("mattias");
        var failed = Case("mystery-failed", LabId, "Half a Case");
        failed.Packet.Validation.Passed = false;
        await SaveAsync(failed);

        var response = await host.PostAsync("/api/mysteries/mystery-failed/table", null);

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    [Fact]
    public async Task Cases_export_as_one_review_document_or_as_json()
    {
        var host = await _factory.ClientForAsync("mattias");
        await SaveAsync(Case("mystery-a", LabId, "The Drowned Bells"));
        await SaveAsync(Case("mystery-b", SocietyId, "The Salt Wife"));

        var markdown = await NexusApiFactory.ResultAsync<MysteryExport>(
            await host.PostAsJsonAsync("/api/mysteries/export", new { mysteryIds = new[] { "mystery-a", "mystery-b" }, format = "markdown" }));
        var json = await NexusApiFactory.ResultAsync<MysteryExport>(
            await host.PostAsJsonAsync("/api/mysteries/export", new { mysteryIds = new[] { "mystery-a", "mystery-b" }, format = "json" }));

        Assert.EndsWith(".md", markdown.FileName, StringComparison.Ordinal);
        Assert.Contains("1. The Drowned Bells (mystery-a): passed validation", markdown.Content, StringComparison.Ordinal);
        Assert.Contains("# The Salt Wife", markdown.Content, StringComparison.Ordinal);
        Assert.Equal(2, JsonConvert.DeserializeObject<List<StoredCase>>(json.Content)!.Count);
        Assert.Equal(HttpStatusCode.BadRequest, (await host.PostAsJsonAsync("/api/mysteries/export", new { mysteryIds = Array.Empty<string>() })).StatusCode);
    }

    [Fact]
    public async Task A_lab_run_writes_each_case_in_turn_and_reports_each_one()
    {
        var host = await _factory.ClientForAsync("mattias");

        Assert.Equal(HttpStatusCode.BadRequest, (await host.PostAsJsonAsync("/api/mysteries/lab", new { count = 0 })).StatusCode);
        var job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.PostAsJsonAsync("/api/mysteries/lab", new { count = 2 }));
        Assert.Equal(MysteryJobKinds.Lab, job.Kind);
        Assert.Equal(2, job.Total);
        for (var i = 0; i < 100 && job.Status == MysteryJobStatus.Running; i++)
        {
            await Task.Delay(50);
            job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.GetAsync($"/api/mysteries/jobs/{job.JobId}"));
        }

        // The test model is "down", so both cases fail; the run itself ends and says so per case.
        Assert.Equal(MysteryJobStatus.Succeeded, job.Status);
        Assert.Equal(2, job.Results.Count);
        Assert.All(job.Results, r => Assert.False(r.Passed));
        Assert.Equal("0 of 2 passed", job.Title);
    }

    private Task SaveAsync(StoredCase stored) => _factory.Services.GetRequiredService<ICasePacketStore>().SaveAsync(stored);

    private static StoredCase Case(string mysteryId, string societyId, string title) => new()
    {
        Packet = new CasePacket
        {
            MysteryId = mysteryId,
            SocietyId = societyId,
            Title = title,
            Hook = new CaseHook { Text = "Come at once: the bells of Forsby ring under the lake." },
            Validation = new CaseValidation { Passed = true },
            Facts = [new CaseFact { Id = "fact-truth", Statement = "The miller drowned them", LoadBearing = true }],
        },
    };
}
