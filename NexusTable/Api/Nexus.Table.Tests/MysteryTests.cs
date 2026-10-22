using System.Net;
using System.Net.Http.Json;
using Nexus.Table.Services;
using Vaesen.Framework.Mystery.Data;

namespace Nexus.Table.Tests;

public sealed class MysteryTests : IDisposable
{
    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task Players_cannot_generate_or_read_case_files_or_the_world()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");

        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsync("/api/mysteries/generate", null)).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries/mystery-x")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries/mystery-x/report")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsJsonAsync("/api/mysteries/mystery-x/close", new { outcome = "solved" })).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/world")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsync("/api/world/seed", null)).StatusCode);
    }

    [Fact]
    public async Task The_host_starts_a_generation_and_sees_it_end()
    {
        var host = await _factory.ClientForAsync("mattias");

        Assert.Empty(await NexusApiFactory.ResultAsync<List<CaseSummary>>(await host.GetAsync("/api/mysteries")));
        var job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.PostAsync("/api/mysteries/generate", null));
        Assert.Equal(MysteryJobStatus.Running, job.Status);
        job = await WaitAsync(host, job);

        // The test model is "down": the job fails visibly at its first model call, seeding the Society's world.
        Assert.Equal(MysteryJobStatus.Failed, job.Status);
        Assert.Contains("not available", job.Error, StringComparison.Ordinal);
        Assert.Contains(job.Progress, p => p.StartsWith("world:", StringComparison.Ordinal));
        Assert.Equal(job.JobId, (await NexusApiFactory.ResultAsync<MysteryJob>(await host.GetAsync("/api/mysteries/jobs/current"))).JobId);
    }

    [Fact]
    public async Task Before_it_is_seeded_the_world_is_not_found_and_seeding_reports_its_failure()
    {
        var host = await _factory.ClientForAsync("mattias");

        Assert.Equal(HttpStatusCode.NotFound, (await host.GetAsync("/api/world")).StatusCode);
        var job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.PostAsync("/api/world/seed", null));
        Assert.Equal(MysteryJobKinds.World, job.Kind);
        job = await WaitAsync(host, job);

        Assert.Equal(MysteryJobStatus.Failed, job.Status);
        Assert.Contains("not available", job.Error, StringComparison.Ordinal);
    }

    [Fact]
    public async Task Closing_an_unknown_mystery_is_not_found()
    {
        var host = await _factory.ClientForAsync("mattias");

        var response = await host.PostAsJsonAsync("/api/mysteries/mystery-nope/close", new { outcome = "solved" });

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    private static async Task<MysteryJob> WaitAsync(HttpClient host, MysteryJob job)
    {
        for (var i = 0; i < 100 && job.Status == MysteryJobStatus.Running; i++)
        {
            await Task.Delay(50);
            job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.GetAsync($"/api/mysteries/jobs/{job.JobId}"));
        }

        return job;
    }
}
