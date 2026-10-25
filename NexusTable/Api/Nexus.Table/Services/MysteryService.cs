using System.Collections.Concurrent;
using System.Globalization;
using System.Text;
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

    /// <summary>A lab run: how many cases were asked for.</summary>
    [JsonProperty("total", NullValueHandling = NullValueHandling.Ignore)]
    public int? Total { get; set; }

    /// <summary>A lab run: each case as it ends.</summary>
    [JsonProperty("results")]
    public List<LabResult> Results { get; set; } = [];

    /// <summary>The host asked a lab run to stop after the case it is writing.</summary>
    [JsonProperty("stopRequested")]
    public bool StopRequested { get; set; }
}

public static class MysteryJobKinds
{
    public const string Mystery = "mystery";
    public const string World = "world";

    /// <summary>Cases written one after another for review; kept apart from the Society's own.</summary>
    public const string Lab = "lab";
}

/// <summary>One case of a lab run.</summary>
public sealed class LabResult
{
    [JsonProperty("mysteryId")]
    public string? MysteryId { get; set; }

    [JsonProperty("title")]
    public string? Title { get; set; }

    [JsonProperty("passed")]
    public bool Passed { get; set; }

    [JsonProperty("error")]
    public string? Error { get; set; }
}

public sealed class LabRequest
{
    public const int MaxCount = 10;

    /// <summary>How many cases to write, one after another (1 to <see cref="MaxCount"/>).</summary>
    [JsonProperty("count")]
    public int Count { get; set; } = 1;
}

public static class ExportFormats
{
    public const string Markdown = "markdown";
    public const string Json = "json";
}

public sealed class ExportRequest
{
    [JsonProperty("mysteryIds")]
    public List<string> MysteryIds { get; set; } = [];

    /// <summary>"markdown" (the readable reports, for reviewing) or "json" (the whole stored cases).</summary>
    [JsonProperty("format")]
    public string Format { get; set; } = ExportFormats.Markdown;
}

/// <summary>A file for the host to save.</summary>
public sealed class MysteryExport
{
    [JsonProperty("fileName")]
    public string FileName { get; set; } = "";

    [JsonProperty("contentType")]
    public string ContentType { get; set; } = "";

    [JsonProperty("content")]
    public string Content { get; set; } = "";
}

public static class MysteryJobStatus
{
    public const string Running = "running";
    public const string Succeeded = "succeeded";
    public const string Failed = "failed";
}

/// <summary>
/// The mystery engine at the table: the host generates cases and reads them (spoilers: host only, spec 20), writes
/// batches in the lab to judge the engine, and brings one case to the table, where players see its title and hook and
/// the Gamemaster gets the whole case file.
/// </summary>
public interface IMysteryService
{
    Task<ResultBase<MysteryJob>> StartAsync(Account caller, CancellationToken cancellationToken = default);

    ResultBase<MysteryJob> GetJob(Account caller, string jobId);

    /// <summary>The running generation, or the last one since the API started; null if there has been none.</summary>
    ResultBase<MysteryJob?> GetCurrentJob(Account caller);

    /// <summary>The Society's cases, or with <paramref name="lab"/> the lab's.</summary>
    Task<ResultBase<List<CaseSummary>>> ListAsync(Account caller, bool lab = false, CancellationToken cancellationToken = default);

    /// <summary>Writes <see cref="LabRequest.Count"/> cases one after another, kept apart from the Society's history.</summary>
    Task<ResultBase<MysteryJob>> StartLabAsync(Account caller, LabRequest request, CancellationToken cancellationToken = default);

    /// <summary>Stops a lab run after the case it is writing.</summary>
    ResultBase<MysteryJob> StopJob(Account caller, string jobId);

    /// <summary>The chosen cases as one file: their reports (Markdown) or the stored cases (JSON).</summary>
    Task<ResultBase<MysteryExport>> ExportAsync(Account caller, ExportRequest request, CancellationToken cancellationToken = default);

    /// <summary>Puts a case on the table. A lab case is moved into the Society's own cases first.</summary>
    Task<ResultBase<Society>> BringToTableAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);

    /// <summary>Takes the case off the table without closing it.</summary>
    Task<ResultBase<Society>> ClearTableAsync(Account caller, CancellationToken cancellationToken = default);

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
    ISessionService sessions,
    Microsoft.Extensions.Options.IOptions<Vaesen.Framework.Mystery.MysteryOptions> options,
    Microsoft.Extensions.Options.IOptions<NexusOptions> nexus,
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
        CancellationToken cancellationToken,
        Action<MysteryJob>? setup = null)
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
        setup?.Invoke(job);
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

    public async Task<ResultBase<List<CaseSummary>>> ListAsync(Account caller, bool lab = false, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<List<CaseSummary>>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can see generated mysteries.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        if (societyId is null)
        {
            return ret.Fail("The Society is not set up.", ErrorCodes.NotFound);
        }

        return await cases.ListAsync(lab ? LabSocietyId(societyId) : societyId, cancellationToken).ConfigureAwait(false);
    }

    public Task<ResultBase<MysteryJob>> StartLabAsync(Account caller, LabRequest request, CancellationToken cancellationToken = default)
    {
        if (request.Count is < 1 or > LabRequest.MaxCount)
        {
            return Task.FromResult(new ResultBase<MysteryJob>().Fail($"Write between 1 and {LabRequest.MaxCount} cases at a time.", ErrorCodes.Invalid));
        }

        return StartJobAsync(
            caller,
            MysteryJobKinds.Lab,
            (job, history, progress, stopping) => RunLabAsync(job, request.Count, history, progress, stopping),
            cancellationToken,
            job => job.Total = request.Count);
    }

    public ResultBase<MysteryJob> StopJob(Account caller, string jobId)
    {
        var ret = new ResultBase<MysteryJob>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can stop a generation.", ErrorCodes.Forbidden);
        }

        if (!_jobs.TryGetValue(jobId, out var job))
        {
            return ret.Fail("No such generation.", ErrorCodes.NotFound);
        }

        lock (job)
        {
            job.StopRequested = true;
        }

        return GetJob(caller, jobId);
    }

    public async Task<ResultBase<MysteryExport>> ExportAsync(Account caller, ExportRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<MysteryExport>();
        var ids = request.MysteryIds.Where(id => !string.IsNullOrWhiteSpace(id)).Distinct(StringComparer.Ordinal).ToList();
        if (ids.Count == 0)
        {
            return ret.Fail("Choose at least one case to export.", ErrorCodes.Invalid);
        }

        if (request.Format is not (ExportFormats.Markdown or ExportFormats.Json))
        {
            return ret.Fail("The format is markdown or json.", ErrorCodes.Invalid);
        }

        var stored = new List<StoredCase>();
        foreach (var id in ids)
        {
            var one = await GetAsync(caller, id, cancellationToken).ConfigureAwait(false);
            if (!one.Success)
            {
                return ret.Fail($"{id}: {one.ErrorMessage}", one.ErrorCode);
            }

            stored.Add(one.Result!);
        }

        var stamp = DateTime.UtcNow.ToString("yyyyMMdd-HHmm", CultureInfo.InvariantCulture);
        if (request.Format == ExportFormats.Json)
        {
            return ret.Ok(new MysteryExport
            {
                FileName = $"nexus-mysteries-{stamp}.json",
                ContentType = "application/json",
                Content = JsonConvert.SerializeObject(stored, Formatting.Indented),
            });
        }

        // One document for a reviewer: a contents list, then each case's full report.
        var s = new StringBuilder();
        s.AppendLine("# Nexus Table: generated mysteries for review");
        s.AppendLine();
        s.AppendLine(CultureInfo.InvariantCulture, $"Exported {DateTime.UtcNow:yyyy-MM-dd HH:mm} UTC. {stored.Count} case(s), each with its truth, cast, clues, validation and generation log.");
        s.AppendLine();
        for (var i = 0; i < stored.Count; i++)
        {
            var packet = stored[i].Packet;
            s.AppendLine(CultureInfo.InvariantCulture, $"{i + 1}. {packet.Title} ({packet.MysteryId}): {(packet.Validation.Passed ? "passed" : "failed")} validation");
        }

        foreach (var one in stored)
        {
            s.AppendLine();
            s.AppendLine("---");
            s.AppendLine();
            s.Append(CaseReport.ToMarkdown(one));
        }

        return ret.Ok(new MysteryExport { FileName = $"nexus-mysteries-{stamp}.md", ContentType = "text/markdown", Content = s.ToString() });
    }

    public async Task<ResultBase<Society>> BringToTableAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Society>();
        var stored = await GetAsync(caller, mysteryId, cancellationToken).ConfigureAwait(false);
        if (!stored.Success)
        {
            return ret.Fail(stored.ErrorMessage, stored.ErrorCode);
        }

        var packet = stored.Result!.Packet;
        if (!packet.Validation.Passed || string.IsNullOrWhiteSpace(packet.Hook.Text))
        {
            return ret.Fail("Only a case that passed its checks, with a hook, can be played.", ErrorCodes.Invalid);
        }

        if (stored.Result.Status is CaseStatuses.Solved or CaseStatuses.Failed)
        {
            return ret.Fail("This case has already been played to its end.", ErrorCodes.Invalid);
        }

        // A case from the lab becomes one of the Society's own: it is played, closed and remembered like the rest.
        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        if (societyId is not null && packet.SocietyId != societyId)
        {
            packet.SocietyId = societyId;
            var saved = await cases.SaveAsync(stored.Result, cancellationToken).ConfigureAwait(false);
            if (!saved.Success)
            {
                return ret.Fail(saved.ErrorMessage, saved.ErrorCode);
            }
        }

        // Another case coming to the table ends the session of the one there.
        var there = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (there.Result?.AtTable is { } before && before.MysteryId != packet.MysteryId)
        {
            await sessions.EndOpenAsync(caller.Id, cancellationToken).ConfigureAwait(false);
        }

        return await society.SetAtTableAsync(caller, new MysteryAtTable
        {
            MysteryId = packet.MysteryId,
            Title = packet.Title,
            HookText = packet.Hook.Text,
            BroughtAt = DateTime.UtcNow,
        }, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<Society>> ClearTableAsync(Account caller, CancellationToken cancellationToken = default)
    {
        if (caller.Role == AccountRole.Host)
        {
            await sessions.EndOpenAsync(caller.Id, cancellationToken).ConfigureAwait(false);
        }

        return await society.SetAtTableAsync(caller, null, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<StoredCase>> GetAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can read a case file.", ErrorCodes.Forbidden);
        }

        var societyId = await SocietyIdAsync(cancellationToken).ConfigureAwait(false);
        if (societyId is null)
        {
            return ret.Fail("The Society is not set up.", ErrorCodes.NotFound);
        }

        // The Society's own cases first, then the lab's.
        var found = await cases.GetAsync(societyId, mysteryId, cancellationToken).ConfigureAwait(false);
        return found.Success ? found : await cases.GetAsync(LabSocietyId(societyId), mysteryId, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<string>> GetReportAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<string>();
        var stored = await GetAsync(caller, mysteryId, cancellationToken).ConfigureAwait(false);
        return stored.Success
            ? ret.Ok(CaseReport.ToMarkdown(stored.Result!))
            : ret.Fail(stored.ErrorMessage, stored.ErrorCode);
    }

    public async Task<ResultBase<StoredCase>> CloseAsync(Account caller, string mysteryId, CloseMysteryRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can close a mystery.", ErrorCodes.Forbidden);
        }

        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!current.Success)
        {
            return ret.Fail("The Society is not set up.", ErrorCodes.NotFound);
        }

        var closed = await worlds.CloseMysteryAsync(current.Result!.Id, mysteryId, request, cancellationToken).ConfigureAwait(false);
        if (closed.Success && current.Result.AtTable?.MysteryId == mysteryId)
        {
            // A closed case leaves the table, its session ended; the next one is chosen fresh.
            await sessions.EndOpenAsync(caller.Id, cancellationToken).ConfigureAwait(false);
            await society.SetAtTableAsync(caller, null, cancellationToken).ConfigureAwait(false);
        }

        if (closed.Success && nexus.Value.PreGenerateNextCase)
        {
            // Write the next case now, in the background, so one is ready when the Society next sits down.
            var next = await StartAsync(caller, CancellationToken.None).ConfigureAwait(false);
            if (!next.Success)
            {
                logger.LogInformation("The next case was not started after closing {MysteryId}: {Error}", mysteryId, next.ErrorMessage);
            }
        }

        return closed;
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

    /// <summary>
    /// Cases one after another, each from the Society's real history, so every run samples "the next mystery". They are
    /// stored under the lab's id and never become history themselves, so the runs don't steer one another.
    /// </summary>
    private async Task RunLabAsync(MysteryJob job, int count, SocietyHistory history, IProgress<GenerationProgress> progress, CancellationToken cancellationToken)
    {
        var labId = LabSocietyId(history.SocietyId);
        try
        {
            for (var run = 1; run <= count; run++)
            {
                lock (job)
                {
                    if (job.StopRequested)
                    {
                        job.Progress.Add($"lab: Stopped after {run - 1} of {count}.");
                        break;
                    }

                    job.Progress.Add($"lab: Case {run} of {count}.");
                }

                var result = await generator.GenerateAsync(new GenerationRequest { History = history, Store = false }, progress, cancellationToken).ConfigureAwait(false);
                var written = result.Result?.Case;
                var labResult = new LabResult { Passed = result.Success, Error = result.Success ? null : result.ErrorMessage, Title = written?.Packet.Title };
                if (written is not null && written.Packet.Title is { Length: > 0 })
                {
                    written.Packet.SocietyId = labId;
                    if (!result.Success)
                    {
                        written.Status = CaseStatuses.Rejected;
                    }

                    var saved = await cases.SaveAsync(written, cancellationToken).ConfigureAwait(false);
                    labResult.MysteryId = saved.Success ? written.Packet.MysteryId : null;
                }

                lock (job)
                {
                    job.Results.Add(labResult);
                }
            }

            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.Status = MysteryJobStatus.Succeeded;
                job.Title = $"{job.Results.Count(r => r.Passed)} of {job.Results.Count} passed";
            }
        }
        catch (Exception ex) when (ex is not OutOfMemoryException)
        {
            logger.LogError(ex, "Lab run {JobId} crashed", job.JobId);
            lock (job)
            {
                job.EndedAt = DateTime.UtcNow;
                job.Status = MysteryJobStatus.Failed;
                job.Error = ex is OperationCanceledException ? "The server stopped." : ex.Message;
            }
        }
    }

    private static string LabSocietyId(string societyId) => societyId + "-lab";

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
            Mysteries = CaseSummary.History(past.Result ?? []),
        });
    }

    private async Task<string?> SocietyIdAsync(CancellationToken cancellationToken)
    {
        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        return current.Success ? current.Result!.Id : null;
    }
}
