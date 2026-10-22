using System.Collections.Concurrent;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;

namespace Nexus.Table.Services;

/// <summary>A generation the host started: it runs in the background and is polled until it ends.</summary>
public sealed class MysteryJob
{
    [JsonProperty("jobId")]
    public string JobId { get; set; } = "";

    /// <summary>"running", "succeeded" or "failed".</summary>
    [JsonProperty("status")]
    public string Status { get; set; } = MysteryJobStatus.Running;

    [JsonProperty("startedAt")]
    public DateTime StartedAt { get; set; }

    [JsonProperty("endedAt")]
    public DateTime? EndedAt { get; set; }

    [JsonProperty("progress")]
    public List<string> Progress { get; set; } = [];

    [JsonProperty("mysteryId")]
    public string? MysteryId { get; set; }

    [JsonProperty("title")]
    public string? Title { get; set; }

    [JsonProperty("error")]
    public string? Error { get; set; }
}

public static class MysteryJobStatus
{
    public const string Running = "running";
    public const string Succeeded = "succeeded";
    public const string Failed = "failed";
}

/// <summary>
/// The mystery engine at the table: the host generates cases and reads them (spoilers: host only, spec 20). Nothing
/// here reaches players or the voice Gamemaster yet; that waits until the owner is happy with generated cases.
/// </summary>
public interface IMysteryService
{
    Task<ResultBase<MysteryJob>> StartAsync(Account caller, CancellationToken cancellationToken = default);

    ResultBase<MysteryJob> GetJob(Account caller, string jobId);

    Task<ResultBase<List<CaseSummary>>> ListAsync(Account caller, CancellationToken cancellationToken = default);

    Task<ResultBase<StoredCase>> GetAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);

    Task<ResultBase<string>> GetReportAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);
}

public sealed class MysteryService(
    IMysteryGenerator generator,
    ICasePacketStore cases,
    ISocietyService society,
    ICharacterService characters,
    IHostApplicationLifetime lifetime,
    ILogger<MysteryService> logger) : IMysteryService
{
    private readonly ConcurrentDictionary<string, MysteryJob> _jobs = new();

    public async Task<ResultBase<MysteryJob>> StartAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<MysteryJob>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can start a mystery.", ErrorCodes.Forbidden);
        }

        if (_jobs.Values.Any(j => j.Status == MysteryJobStatus.Running))
        {
            return ret.Fail("A mystery is already being generated.", ErrorCodes.Conflict);
        }

        var history = await HistoryAsync(cancellationToken).ConfigureAwait(false);
        if (!history.Success)
        {
            return ret.Fail(history.ErrorMessage, history.ErrorCode);
        }

        var job = new MysteryJob { JobId = Guid.NewGuid().ToString("N"), StartedAt = DateTime.UtcNow };
        _jobs[job.JobId] = job;
        var progress = new Progress<GenerationProgress>(p =>
        {
            lock (job)
            {
                job.Progress.Add($"{p.Step}: {p.Message}");
            }
        });

        // Generation takes minutes; it outlives the request and stops only with the app.
        _ = Task.Run(() => RunAsync(job, history.Result!, progress, lifetime.ApplicationStopping), CancellationToken.None);
        return ret.Ok(job);
    }

    public ResultBase<MysteryJob> GetJob(Account caller, string jobId)
    {
        var ret = new ResultBase<MysteryJob>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can see mystery generation.", ErrorCodes.Forbidden);
        }

        if (!_jobs.TryGetValue(jobId, out var job))
        {
            return ret.Fail("No such generation.", ErrorCodes.NotFound);
        }

        lock (job)
        {
            return ret.Ok(JsonConvert.DeserializeObject<MysteryJob>(JsonConvert.SerializeObject(job))!);
        }
    }

    public async Task<ResultBase<List<CaseSummary>>> ListAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<List<CaseSummary>>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can see generated mysteries.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        return societyId is null ? ret.Fail("The Society is not set up.", ErrorCodes.NotFound) : await cases.ListAsync(societyId, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<StoredCase>> GetAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can read a case file.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        return societyId is null ? ret.Fail("The Society is not set up.", ErrorCodes.NotFound) : await cases.GetAsync(societyId, mysteryId, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<string>> GetReportAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<string>();
        var stored = await GetAsync(caller, mysteryId, cancellationToken).ConfigureAwait(false);
        return stored.Success
            ? ret.Ok(CaseReport.ToMarkdown(stored.Result!.Packet, stored.Result.Brief, stored.Result.Log))
            : ret.Fail(stored.ErrorMessage, stored.ErrorCode);
    }

    private async Task RunAsync(MysteryJob job, SocietyHistory history, IProgress<GenerationProgress> progress, CancellationToken cancellationToken)
    {
        try
        {
            var result = await generator.GenerateAsync(new GenerationRequest { History = history }, progress, cancellationToken).ConfigureAwait(false);
            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.MysteryId = result.Result?.Case.Packet.MysteryId;
                job.Title = result.Result?.Case.Packet.Title;
                job.Status = result.Success ? MysteryJobStatus.Succeeded : MysteryJobStatus.Failed;
                job.Error = result.Success ? null : result.ErrorMessage;
            }
        }
        catch (Exception ex) when (ex is not OutOfMemoryException)
        {
            logger.LogError(ex, "Mystery generation {JobId} crashed", job.JobId);
            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.Status = MysteryJobStatus.Failed;
                job.Error = ex is OperationCanceledException ? "The server stopped." : ex.Message;
            }
        }
    }

    /// <summary>What the assignment needs: the Society, its investigators and the creatures and hooks it has met.</summary>
    private async Task<ResultBase<SocietyHistory>> HistoryAsync(CancellationToken cancellationToken)
    {
        var ret = new ResultBase<SocietyHistory>();
        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!current.Success)
        {
            return ret.Fail(current.ErrorMessage, current.ErrorCode);
        }

        var roster = await characters.ListAsync(cancellationToken).ConfigureAwait(false);
        var past = await cases.ListAsync(current.Result!.Id, cancellationToken).ConfigureAwait(false);
        return ret.Ok(new SocietyHistory
        {
            SocietyId = current.Result.Id,
            SocietyName = current.Result.Name,
            Investigators = (roster.Result ?? []).Select(c => $"{c.Name} ({c.Archetype}, {c.Age})").ToList(),
            Mysteries = (past.Result ?? [])
                .Where(s => s.Passed)
                .OrderBy(s => s.GeneratedAt)
                .Select(s => new PastMystery { MysteryId = s.MysteryId, LevelId = s.LevelId, VaesenIds = s.VaesenIds, HookChannelId = s.HookChannelId, HookGiverType = s.HookGiverType })
                .ToList(),
        });
    }

    private async Task<string?> SocietyIdAsync(CancellationToken cancellationToken)
    {
        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        return current.Success ? current.Result!.Id : null;
    }
}
