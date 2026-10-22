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

    /// <summary>"mystery" (writing a case) or "world" (seeding the Society's world).</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = MysteryJobKinds.Mystery;

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

public static class MysteryJobKinds
{
    public const string Mystery = "mystery";
    public const string World = "world";
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

    /// <summary>The running generation, or the last one since the API started; null if there has been none.</summary>
    ResultBase<MysteryJob?> GetCurrentJob(Account caller);

    Task<ResultBase<List<CaseSummary>>> ListAsync(Account caller, CancellationToken cancellationToken = default);

    Task<ResultBase<StoredCase>> GetAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);

    Task<ResultBase<string>> GetReportAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);

    /// <summary>Records how the mystery ended and writes it back to the Society's world.</summary>
    Task<ResultBase<StoredCase>> CloseAsync(Account caller, string mysteryId, CloseMysteryRequest request, CancellationToken cancellationToken = default);

    /// <summary>The Society's persistent world (host only: it holds the cast's secrets).</summary>
    Task<ResultBase<SocietyWorld>> GetWorldAsync(Account caller, CancellationToken cancellationToken = default);

    /// <summary>Seeds the Society's world now, in the background, instead of before the first mystery.</summary>
    Task<ResultBase<MysteryJob>> StartSeedAsync(Account caller, CancellationToken cancellationToken = default);
}

public sealed class MysteryService(
    IMysteryGenerator generator,
    ICasePacketStore cases,
    ISocietyService society,
    ICharacterService characters,
    IWorldService worlds,
    IContentStore content,
    Microsoft.Extensions.Options.IOptions<Vaesen.Framework.Mystery.MysteryOptions> options,
    IHostApplicationLifetime lifetime,
    ILogger<MysteryService> logger) : IMysteryService
{
    private readonly ConcurrentDictionary<string, MysteryJob> _jobs = new();

    public Task<ResultBase<MysteryJob>> StartAsync(Account caller, CancellationToken cancellationToken = default) =>
        StartJobAsync(caller, MysteryJobKinds.Mystery, RunAsync, cancellationToken);

    public Task<ResultBase<MysteryJob>> StartSeedAsync(Account caller, CancellationToken cancellationToken = default) =>
        StartJobAsync(caller, MysteryJobKinds.World, RunSeedAsync, cancellationToken);

    private async Task<ResultBase<MysteryJob>> StartJobAsync(
        Account caller,
        string kind,
        Func<MysteryJob, SocietyHistory, IProgress<GenerationProgress>, CancellationToken, Task> run,
        CancellationToken cancellationToken)
    {
        var ret = new ResultBase<MysteryJob>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can start a mystery.", ErrorCodes.Forbidden);
        }

        if (_jobs.Values.Any(j => j.Status == MysteryJobStatus.Running))
        {
            return ret.Fail("The engine is already at work; wait for it to finish.", ErrorCodes.Conflict);
        }

        var history = await HistoryAsync(cancellationToken).ConfigureAwait(false);
        if (!history.Success)
        {
            return ret.Fail(history.ErrorMessage, history.ErrorCode);
        }

        var job = new MysteryJob { JobId = Guid.NewGuid().ToString("N"), Kind = kind, StartedAt = DateTime.UtcNow };
        _jobs[job.JobId] = job;
        var progress = new Progress<GenerationProgress>(p =>
        {
            lock (job)
            {
                // The assignment names the creature; the host may be playing, so the page only says it is done.
                job.Progress.Add(p.Step == "assignment" ? "assignment: A place and a creature are chosen." : $"{p.Step}: {p.Message}");
            }
        });

        // Generation takes minutes; it outlives the request and stops only with the app.
        _ = Task.Run(() => run(job, history.Result!, progress, lifetime.ApplicationStopping), CancellationToken.None);
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

    public ResultBase<MysteryJob?> GetCurrentJob(Account caller)
    {
        var ret = new ResultBase<MysteryJob?>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can see mystery generation.", ErrorCodes.Forbidden);
        }

        var job = _jobs.Values
            .OrderByDescending(j => j.Status == MysteryJobStatus.Running)
            .ThenByDescending(j => j.StartedAt)
            .FirstOrDefault();
        return job is null ? ret.Ok(null) : ret.Ok(GetJob(caller, job.JobId).Result);
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

    public async Task<ResultBase<StoredCase>> CloseAsync(Account caller, string mysteryId, CloseMysteryRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can close a mystery.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        return societyId is null
            ? ret.Fail("The Society is not set up.", ErrorCodes.NotFound)
            : await worlds.CloseMysteryAsync(societyId, mysteryId, request, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<SocietyWorld>> GetWorldAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyWorld>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can see the Society's world.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        return societyId is null
            ? ret.Fail("The Society is not set up.", ErrorCodes.NotFound)
            : await worlds.GetAsync(societyId, cancellationToken).ConfigureAwait(false);
    }

    private async Task RunSeedAsync(MysteryJob job, SocietyHistory history, IProgress<GenerationProgress> progress, CancellationToken cancellationToken)
    {
        try
        {
            var pack = await content.GetPackAsync(options.Value.SettingId, cancellationToken).ConfigureAwait(false);
            var result = pack.Success
                ? await worlds.EnsureSeededAsync(history, pack.Result!.Usable(options.Value.UseDraftContent), progress, cancellationToken).ConfigureAwait(false)
                : new ResultBase<SocietyWorld>().Fail(pack.ErrorMessage, pack.ErrorCode);
            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.Status = result.Success ? MysteryJobStatus.Succeeded : MysteryJobStatus.Failed;
                job.Error = result.Success ? null : result.ErrorMessage;
                job.Title = result.Success ? $"{result.Result!.Characters.Count} people" : null;
            }
        }
        catch (Exception ex) when (ex is not OutOfMemoryException)
        {
            logger.LogError(ex, "World seeding {JobId} crashed", job.JobId);
            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.Status = MysteryJobStatus.Failed;
                job.Error = ex is OperationCanceledException ? "The server stopped." : ex.Message;
            }
        }
    }

    private async Task RunAsync(MysteryJob job, SocietyHistory history, IProgress<GenerationProgress> progress, CancellationToken cancellationToken)
    {
        try
        {
            var result = await generator.GenerateAsync(new GenerationRequest { History = history }, progress, cancellationToken).ConfigureAwait(false);
            // A failed case that got somewhere is kept so the host can see why it failed; the history ignores it.
            var kept = result.Success || result.Result?.Case.Packet.Title is { Length: > 0 };
            if (!result.Success && kept)
            {
                await cases.SaveAsync(result.Result!.Case, cancellationToken).ConfigureAwait(false);
            }

            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.MysteryId = kept ? result.Result?.Case.Packet.MysteryId : null;
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
                .Where(s => s.Passed && s.Status != CaseStatuses.Rejected)
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
