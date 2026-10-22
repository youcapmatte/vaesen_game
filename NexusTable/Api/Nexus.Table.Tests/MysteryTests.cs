using System.Net;
using Nexus.Table.Services;
using Vaesen.Framework.Mystery.Data;

namespace Nexus.Table.Tests;

public sealed class MysteryTests : IDisposable
{
    private readonly NexusApiFactory _factory = new();

    public void Dispose() => _factory.Dispose();

    [Fact]
    public async Task Players_cannot_generate_or_read_case_files()
    {
        var ingrid = await _factory.ClientForAsync("ingrid");

        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.PostAsync("/api/mysteries/generate", null)).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries/mystery-x")).StatusCode);
        Assert.Equal(HttpStatusCode.Forbidden, (await ingrid.GetAsync("/api/mysteries/mystery-x/report")).StatusCode);
    }

    [Fact]
    public async Task The_host_starts_a_generation_and_sees_it_end()
    {
        var host = await _factory.ClientForAsync("mattias");

        Assert.Empty(await NexusApiFactory.ResultAsync<List<CaseSummary>>(await host.GetAsync("/api/mysteries")));
        var job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.PostAsync("/api/mysteries/generate", null));
        Assert.Equal(MysteryJobStatus.Running, job.Status);

        for (var i = 0; i < 100 && job.Status == MysteryJobStatus.Running; i++)
        {
            await Task.Delay(50);
            job = await NexusApiFactory.ResultAsync<MysteryJob>(await host.GetAsync($"/api/mysteries/jobs/{job.JobId}"));
        }

        // The test model is "down": the job fails visibly, after the assignment chose a case.
        Assert.Equal(MysteryJobStatus.Failed, job.Status);
        Assert.Contains("not available", job.Error, StringComparison.Ordinal);
        Assert.Contains(job.Progress, p => p.StartsWith("assignment:", StringComparison.Ordinal));
    }
}
