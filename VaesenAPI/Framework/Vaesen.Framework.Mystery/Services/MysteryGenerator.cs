using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Services;

/// <summary>A request for a new case.</summary>
public sealed class GenerationRequest
{
    public required SocietyHistory History { get; init; }

    /// <summary>Pin the in-world month; null: the Society's calendar moves on to the next month that fits (fixes 4, section 11).</summary>
    public int? Month { get; init; }

    /// <summary>A deliberate return visit to this level: its cooldown is waived and its earlier cases are the backstory.</summary>
    public string? ReturnLevelId { get; init; }

    /// <summary>Fixes the deterministic choices; null: random.</summary>
    public int? Seed { get; init; }

    /// <summary>Force a level (CaseLab), instead of letting the assignment choose.</summary>
    public string? LevelId { get; init; }

    /// <summary>Force a creature (CaseLab), instead of letting the assignment choose.</summary>
    public string? VaesenId { get; init; }

    /// <summary>Pin the creature's origin (an affliction's), to regenerate a case from the same brief.</summary>
    public string? OriginId { get; init; }

    /// <summary>Store the case when it passes (off for dry runs).</summary>
    public bool Store { get; init; } = true;

    /// <summary>
    /// Keep a level's new roster in the Society's world when the stored case passes (fixes 4, section 12.1). Off for
    /// throwaway experiments, so they do not fix a town's people for good.
    /// </summary>
    public bool KeepRoster { get; init; } = true;
}

public sealed class GenerationResult
{
    public required StoredCase Case { get; init; }

    public bool Passed => Case.Packet.Validation.Passed;
}

/// <summary>Progress for a host waiting on Start: which step of which attempt is running.</summary>
public sealed record GenerationProgress(int CaseAttempt, string Step, string Message);

public interface IMysteryGenerator
{
    /// <summary>Runs assignment, the generation steps, the validator and the critic, and stores the case packet.</summary>
    Task<ResultBase<GenerationResult>> GenerateAsync(GenerationRequest request, IProgress<GenerationProgress>? progress = null, CancellationToken cancellationToken = default);

    /// <summary>Only the deterministic assignment, for previews and tests.</summary>
    Task<ResultBase<AssignmentBrief>> AssignAsync(GenerationRequest request, CancellationToken cancellationToken = default);
}

/// <summary>
/// Spec section 7: assignment (C#), then conflict, escalation, humans and the clues as four sub-steps (facts, minds
/// and channels, arc, hook and scenes), each validated before the next; then the whole-case checks and the critic.
/// A failing step retries with its issues, then starts again on its own; steps that passed are kept. Whole-case and
/// critic issues send back only the sub-step they point at, and an earlier step is re-run only when an issue points at
/// it. A case is re-rolled from a new brief only when nothing plausible fits. Every attempt is in the log.
/// </summary>
public sealed class MysteryGenerator(
    IContentStore content,
    ICasePacketStore cases,
    IMysteryModel model,
    ICoherenceValidator validator,
    IWorldService worlds,
    IOptions<MysteryOptions> options,
    ILogger<MysteryGenerator> logger) : IMysteryGenerator
{
    public const string GenerationFailed = "generation_failed";

    private MysteryOptions Options => options.Value;

    public async Task<ResultBase<AssignmentBrief>> AssignAsync(GenerationRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<AssignmentBrief>();
        var pack = await LoadPackAsync(request, cancellationToken);
        if (!pack.Success)
        {
            return ret.Fail(pack.ErrorMessage, pack.ErrorCode);
        }

        try
        {
            return ret.Ok(Assignment.Assign(pack.Result!, request.History, request.Month, request.Seed ?? Random.Shared.Next(), Options, request.ReturnLevelId));
        }
        catch (InvalidOperationException ex)
        {
            return ret.Fail(ex.Message, ErrorCodes.Invalid);
        }
    }

    public async Task<ResultBase<GenerationResult>> GenerateAsync(
        GenerationRequest request, IProgress<GenerationProgress>? progress = null, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<GenerationResult>();
        var packResult = await LoadPackAsync(request, cancellationToken);
        if (!packResult.Success)
        {
            return ret.Fail(packResult.ErrorMessage, packResult.ErrorCode);
        }

        var pack = packResult.Result!;
        SocietyWorld? world = null;
        if (Options.SeedWorld)
        {
            var seeded = await worlds.EnsureSeededAsync(request.History, pack, progress, cancellationToken);
            if (!seeded.Success)
            {
                return ret.Fail($"The Society's world could not be prepared. {seeded.ErrorMessage}", seeded.ErrorCode);
            }

            world = seeded.Result;
        }

        var baseSeed = request.Seed ?? Random.Shared.Next();
        var log = new List<GenerationLogEntry>();

        // Rosters written during this generation, by level: a re-rolled brief on the same level reuses its roster.
        var newRosters = new Dictionary<string, (LevelRoster Roster, List<CharacterInstance> People)>(StringComparer.Ordinal);
        StoredCase? last = null;
        string lastError = "";

        for (var caseAttempt = 0; caseAttempt <= Options.MaxCaseRetries; caseAttempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            AssignmentBrief brief;
            try
            {
                brief = Assignment.Assign(pack, request.History, request.Month, unchecked(baseSeed + caseAttempt * 7919), Options, request.ReturnLevelId);
            }
            catch (InvalidOperationException ex)
            {
                return ret.Fail(ex.Message, ErrorCodes.Invalid);
            }

            if (world is not null)
            {
                brief.PersistentCastIds = PersistentCast(world);
            }

            var rosterLog = new List<GenerationLogEntry>();
            var caseWorld = world;
            if (world is not null && world.RosterFor(brief.LevelId) is null)
            {
                if (!newRosters.ContainsKey(brief.LevelId))
                {
                    var level = pack.FindLevel(brief.LevelId)!.ForMonth(brief.Month);
                    progress?.Report(new GenerationProgress(caseAttempt, RosterBuilder.StepName, "Meeting the people of the place."));
                    var built = await new RosterBuilder(model, Options).BuildAsync(pack, level, world, rosterLog, caseAttempt, cancellationToken);
                    if (!built.Success)
                    {
                        log.AddRange(rosterLog);
                        lastError = built.ErrorMessage;
                        break;
                    }

                    newRosters[brief.LevelId] = built.Result;
                }

                caseWorld = WithRoster(world, newRosters[brief.LevelId]);
            }

            var draft = NewDraft(pack, request.History, brief, caseWorld);
            draft.Log.AddRange(rosterLog);
            draft.SettledPublicFacts = await SettledPublicFactsAsync(request.History, brief.LevelId, world, cancellationToken);
            draft.EarlierPlots = await EarlierPlotsAsync(draft, request.History, brief, caseAttempt, cancellationToken);
            progress?.Report(new GenerationProgress(caseAttempt, "assignment",
                $"{draft.Vaesen.Name} at {draft.Level.Name}, {new InWorldDate(brief.Year, brief.Month)}, focus {brief.Focus}{(brief.HiddenHand ? ", with a hidden hand" : "")}{(brief.ReturnVisit ? ", a return visit" : "")}."));

            var outcome = await RunStepsAsync(draft, caseAttempt, progress, cancellationToken);
            log.AddRange(draft.Log);
            last = new StoredCase
            {
                Packet = draft.Packet,
                Brief = brief,
                Log = log.ToList(),
                Prompts = new CasePrompts { System = draft.SystemPrompt, Context = draft.CaseContext },
            };

            if (outcome.Success)
            {
                Finish(draft, passed: true);
                last.Status = draft.Packet.Validation.NeedsReview ? CaseStatuses.NeedsReview : CaseStatuses.Generated;
                if (request.Store)
                {
                    var saved = await cases.SaveAsync(last, cancellationToken);
                    if (!saved.Success)
                    {
                        return ret.Fail(saved.ErrorMessage, saved.ErrorCode);
                    }

                    if (world is not null && request.KeepRoster)
                    {
                        var roster = newRosters.TryGetValue(brief.LevelId, out var made) ? made : ((LevelRoster, List<CharacterInstance>)?)null;
                        var kept = await worlds.RecordRosterAsync(world.SocietyId, brief.LevelId, draft.Packet.MysteryId, roster, cancellationToken);
                        if (!kept.Success)
                        {
                            logger.LogWarning("The roster of {LevelId} was not kept: {Error}", brief.LevelId, kept.ErrorMessage);
                        }
                    }
                }

                logger.LogInformation("Generated mystery {MysteryId} '{Title}' for {SocietyId} after {Attempts} case attempt(s)",
                    draft.Packet.MysteryId, draft.Packet.Title, draft.Packet.SocietyId, caseAttempt + 1);
                return ret.Ok(new GenerationResult { Case = last });
            }

            lastError = outcome.ErrorMessage;
            logger.LogWarning("Mystery case attempt {Attempt} failed at {Step}: {Error}", caseAttempt + 1, draft.FailedStep, outcome.ErrorMessage);
            Finish(draft, passed: false);
            last.FailedStep = draft.FailedStep;
            if (outcome.ErrorCode is not StepErrorCodes.Reroll)
            {
                // A step that gave up already started again on its own, and a model or network failure is not a bad
                // brief: only "nothing plausible fits" draws a new one.
                break;
            }
        }

        // Fail visibly, but hand back the last attempt and the whole log so it can be inspected.
        ret.Result = last is null ? null : new GenerationResult { Case = last };
        return ret.Fail($"No valid case could be generated. {lastError}", GenerationFailed);
    }

    private async Task<ResultBase<ContentPack>> LoadPackAsync(GenerationRequest request, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<ContentPack>();
        var loaded = await content.GetPackAsync(Options.SettingId, cancellationToken);
        if (!loaded.Success)
        {
            return ret.Fail(loaded.ErrorMessage, loaded.ErrorCode);
        }

        var pack = loaded.Result!.Usable(Options.UseDraftContent);
        var errors = ContentValidator.Validate(pack).Where(i => i.IsError).ToList();
        if (errors.Count > 0)
        {
            return ret.Fail("The content pack has errors: " + string.Join(" ", errors.Take(5)), ErrorCodes.Invalid);
        }

        if (request.VaesenId is not null)
        {
            pack.Vaesen = pack.Vaesen.Where(v => v.Id == request.VaesenId).ToList();
        }

        if (request.LevelId is not null)
        {
            pack.Levels = pack.Levels.Where(l => l.Id == request.LevelId).ToList();
        }

        if (request.OriginId is not null)
        {
            // A copy of each creature that has the origin, narrowed to it: the reason is then an affliction from it.
            pack.Vaesen = pack.Vaesen.Where(v => v.OriginSet.Exists(o => o.Id == request.OriginId)).Select(v =>
            {
                var copy = Newtonsoft.Json.JsonConvert.DeserializeObject<VaesenDefinition>(Newtonsoft.Json.JsonConvert.SerializeObject(v))!;
                copy.OriginSet = copy.OriginSet.Where(o => o.Id == request.OriginId).ToList();
                copy.ReasonKinds = [ReasonKinds.Affliction];
                return copy;
            }).ToList();
        }

        if (pack.Vaesen.Count == 0 || pack.Levels.Count == 0)
        {
            return ret.Fail(Options.UseDraftContent
                ? "No usable creature or level (check the requested ids)."
                : "No approved creature or level yet. Approve content with the ContentTool, or set Mystery:UseDraftContent.", ErrorCodes.Invalid);
        }

        return ret.Ok(pack);
    }

    /// <summary>
    /// The level's public facts the Society's cases already explain (fixes 4, section 12.2): from the overlay for cases
    /// played, and from the stored packets of cases queued on the level.
    /// </summary>
    private async Task<List<SettledPublicFact>> SettledPublicFactsAsync(SocietyHistory history, string levelId, SocietyWorld? world, CancellationToken ct)
    {
        var settled = (world?.Overlay ?? [])
            .Where(o => o.LevelId == levelId && o.PublicFactId is not null)
            .Select(o => new SettledPublicFact(o.PublicFactId!, "", o.Text, o.MysteryId ?? "", Played: true))
            .ToList();
        foreach (var queued in history.Mysteries.Where(m => m.LevelId == levelId && !m.Finished))
        {
            var stored = await cases.GetAsync(history.SocietyId, queued.MysteryId, ct);
            foreach (var e in stored.Result?.Packet.PublicFactsExplained ?? [])
            {
                if (!settled.Exists(s => s.PublicFactId == e.PublicFactId))
                {
                    settled.Add(new SettledPublicFact(e.PublicFactId, e.Fact, e.Explanation, queued.MysteryId, Played: false));
                }
            }
        }

        return settled;
    }

    /// <summary>How many earlier cases the plot memory holds: the latest with the same creature or at the same level.</summary>
    public const int EarlierPlotCount = 6;

    /// <summary>
    /// The plot memory (review of case 09, problem 5; fixes 7, section 4): the Society's latest cases with this creature
    /// or at this level, from its history and from every case stored for it (queued, in play, finished, or made in the
    /// case lab), each as its title, the first sentences of its truth and its signature. A case stored without a
    /// signature is read into one now and saved with it.
    /// </summary>
    private async Task<List<EarlierPlot>> EarlierPlotsAsync(MysteryDraft draft, SocietyHistory history, AssignmentBrief brief, int caseAttempt, CancellationToken ct)
    {
        var finished = history.Mysteries.Where(m => m.Finished).Select(m => m.MysteryId).ToHashSet(StringComparer.Ordinal);
        var listed = (await cases.ListAsync(history.SocietyId, ct)).Result ?? [];
        var candidates = history.Mysteries.Select(m => (m.MysteryId, m.VaesenIds, m.LevelId, Order: m.Year * 12 + m.Month))
            .Concat(listed.Where(s => s.Status != CaseStatuses.Rejected).Select(s => (s.MysteryId, s.VaesenIds, s.LevelId, Order: s.Year * 12 + s.Month)))
            .Where(c => c.VaesenIds.Contains(brief.VaesenId) || c.LevelId == brief.LevelId)
            .DistinctBy(c => c.MysteryId)
            .OrderByDescending(c => c.Order)
            .Take(EarlierPlotCount);

        var plots = new List<EarlierPlot>();
        foreach (var past in candidates)
        {
            var stored = await cases.GetAsync(history.SocietyId, past.MysteryId, ct);
            if (stored.Result?.Packet is not { } packet)
            {
                continue;
            }

            if (packet.PlotSignature is null && Options.UseCritic)
            {
                var signed = await new NoveltyCheck(model, Options).SignAsync(packet, past.VaesenIds.FirstOrDefault() ?? "", draft.Log, caseAttempt, draft.SystemPrompt, ct);
                if (signed.Success)
                {
                    packet.PlotSignature = signed.Result;
                    if (!(await cases.SaveAsync(stored.Result, ct)).Success)
                    {
                        logger.LogWarning("The plot signature of {MysteryId} was not kept", past.MysteryId);
                    }
                }
            }

            var summary = packet.Truth.Summary.Length <= 600 ? packet.Truth.Summary : packet.Truth.Summary[..600].TrimEnd() + "…";
            plots.Add(new EarlierPlot(past.MysteryId, packet.Title, past.VaesenIds.FirstOrDefault() ?? "", past.LevelId, packet.Brief.Year, packet.Brief.Month,
                packet.Brief.ConflictArchetypeId, summary, finished.Contains(past.MysteryId), packet.PlotSignature));
        }

        return plots;
    }

    /// <summary>A copy of the world with a level's new roster in it, for a case to draw on before the roster is kept.</summary>
    private static SocietyWorld WithRoster(SocietyWorld world, (LevelRoster Roster, List<CharacterInstance> People) roster)
    {
        var copy = Newtonsoft.Json.JsonConvert.DeserializeObject<SocietyWorld>(Newtonsoft.Json.JsonConvert.SerializeObject(world))!;
        copy.LevelRosters.Add(roster.Roster);
        copy.Characters.AddRange(roster.People);
        return copy;
    }

    /// <summary>
    /// The persistent characters a case is offered: the living, canonical and core first, then by importance. A level's
    /// own people come in their own section (fixes 4, section 12.1), and other levels' people stay home.
    /// </summary>
    private List<string> PersistentCast(SocietyWorld world) => world.Available
        .Where(c => c.Tier != WorldTiers.LocationResident)
        .OrderBy(c => c.Tier switch { WorldTiers.Canonical => 0, WorldTiers.Core => 1, WorldTiers.Background => 2, _ => 3 })
        .ThenByDescending(c => c.Importance)
        .ThenBy(c => c.Id, StringComparer.Ordinal)
        .Take(Options.PersistentCastOffered)
        .Select(c => c.Id)
        .ToList();

    private MysteryDraft NewDraft(ContentPack pack, SocietyHistory history, AssignmentBrief brief, SocietyWorld? world)
    {
        // The case's own copy, as the level is in the case's month (fixes 4, section 12.4): it may add homes for its
        // people (fixes 3, section 6) without touching the content pack.
        var level = pack.FindLevel(brief.LevelId)!.ForMonth(brief.Month);
        var vaesen = pack.FindVaesen(brief.VaesenId)!;
        var draft = new MysteryDraft
        {
            Brief = brief,
            Pack = pack,
            History = history,
            Level = level,
            Vaesen = vaesen,
            Conflicts = brief.ConflictArchetypeIds.Select(pack.FindConflict).OfType<ConflictArchetype>().ToList(),
            World = world,
        };

        draft.Packet = new CasePacket
        {
            MysteryId = ObjectCore.CreateRandomId("mystery"),
            SocietyId = history.SocietyId,
            Models = new CaseModels { Creative = Options.CreativeModel, Critic = Options.UseCritic ? Options.CriticModel : "" },
            Brief = new CaseBrief
            {
                LevelId = level.Id,
                VaesenIds = [vaesen.Id],
                Month = brief.Month,
                Year = brief.Year,
                ReturnVisit = brief.ReturnVisit,
                PreviousMysteryIds = brief.PreviousMysteryIds,
                HookChannelId = brief.HookChannelId,
                HookGiverType = brief.HookGiverType,
                Seed = brief.Seed,
            },
            Truth = new CaseTruth
            {
                ReasonKind = brief.ReasonKind,
                Origin = brief.OriginId,
                PlotCentre = new PlotCentre { Kind = brief.PlotCentreKind },
                Focus = brief.Focus,
            },
            Level = new CaseLevel { LevelId = level.Id },
        };
        draft.SystemPrompt = PromptBuilder.System(pack.Setting);
        draft.CaseContext = PromptBuilder.CaseContext(draft);
        return draft;
    }

    /// <summary>The steps before the clues, in order; the clues are the last stage.</summary>
    private static readonly string[] Stages = [ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, "clues"];

    /// <summary>
    /// The steps in their fixed order, each on the stage the one before produced. When a stage gives up and its issues
    /// point at an earlier step, that step runs once more with them, followed by everything after it.
    /// </summary>
    private async Task<ResultBase> RunStepsAsync(MysteryDraft draft, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var ret = new ResultBase();
        var from = 0;
        IReadOnlyList<CoherenceIssue> feedback = [];
        var reworkedEarlier = false;
        var triedFoci = new List<string>();
        while (true)
        {
            ResultBase failed = ret.Ok();
            for (var stage = from; stage < Stages.Length; stage++)
            {
                var result = await RunStageAsync(draft, stage, stage == from ? feedback : [], caseAttempt, progress, ct);
                if (!result.Success)
                {
                    draft.FailedStep ??= Stages[stage];
                    failed = result;
                    break;
                }

                draft.FailedStep = null;
            }

            if (failed.Success)
            {
                return failed;
            }

            // The conflict step said the focus does not suit the creature here: change only the focus (fixes, section 8).
            if (failed.ErrorCode == StepErrorCodes.RerollFocus)
            {
                triedFoci.Add(draft.Brief.Focus);
                var next = Assignment.SupportedFocus(draft.Vaesen, draft.Brief.Focus, draft.Brief.HiddenHand, triedFoci);
                if (triedFoci.Contains(next))
                {
                    return ret.Fail($"No focus suits {draft.Vaesen.Name} here: {failed.ErrorMessage}", StepErrorCodes.Reroll);
                }

                logger.LogInformation("The focus {Focus} does not suit {Vaesen}; trying {Next}", draft.Brief.Focus, draft.Vaesen.Id, next);
                draft.Brief.Focus = next;
                draft.Packet.Truth.Focus = next;
                draft.CaseContext = PromptBuilder.CaseContext(draft);
                draft.FailedStep = null;
                from = 0;
                feedback = [];
                continue;
            }

            if (failed.ErrorCode != StepErrorCodes.StepFailed || reworkedEarlier)
            {
                return failed;
            }

            // Re-run an earlier step only when an issue is attributed to it (fixes, section 2.3).
            var failedAt = Array.IndexOf(Stages, StageOf(draft.FailedStep));
            var issues = draft.ReworkFeedback.Count > 0 ? draft.ReworkFeedback : LastIssues(draft, draft.FailedStep);
            var earlier = issues.Select(EarlierStage).Where(i => i >= 0 && i < failedAt).DefaultIfEmpty(-1).Min();
            if (earlier < 0)
            {
                return failed;
            }

            logger.LogInformation("Step {Step} gave up on issues that point at {Earlier}; running {Earlier} again", draft.FailedStep, Stages[earlier], Stages[earlier]);
            reworkedEarlier = true;
            from = earlier;
            feedback = issues.Where(i => EarlierStage(i) == earlier).ToList();
            draft.ReworkFeedback = [];
            draft.FailedStep = null;
        }
    }

    private async Task<ResultBase> RunStageAsync(MysteryDraft draft, int stage, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var o = Options;
        return stage switch
        {
            0 => await RunConflictAsync(draft, feedback, caseAttempt, progress, ct),
            1 => await RunStepAsync(new EscalationStep(model, validator, o), new ConflictStage(draft), feedback, caseAttempt, "Writing what has happened.", progress, ct),
            2 => await RunPeopleAsync(draft, feedback, caseAttempt, progress, ct),
            _ => await RunCluesAsync(draft, feedback, caseAttempt, progress, ct),
        };
    }

    /// <summary>How many times the conflict is re-rolled when its plot repeats an earlier one.</summary>
    public const int NoveltyRerolls = 1;

    /// <summary>
    /// The conflict, then the structural novelty check on it (fixes 7, section 4): its plot is read into a signature and
    /// compared with the Society's earlier plots. A repeat re-rolls the conflict once, with the overlap named. A repeat
    /// that survives is left for the critic's novelty check, which then blocks.
    /// </summary>
    private async Task<ResultBase> RunConflictAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var o = Options;
        var conflict = new ConflictStep(model, validator, o);
        var result = await RunStepAsync(conflict, new AssignedStage(draft), feedback, caseAttempt, "Building the conflict.", progress, ct);
        for (var reroll = 0; result.Success && o.UseCritic; reroll++)
        {
            progress?.Report(new GenerationProgress(caseAttempt, NoveltyCheck.StepName, "Checking the plot against the Society's earlier cases."));
            var signed = await new NoveltyCheck(model, o).SignAsync(draft.Packet, draft.Vaesen.Id, draft.Log, caseAttempt, draft.SystemPrompt, ct);
            if (!signed.Success)
            {
                logger.LogWarning("The plot could not be read into a signature: {Error}", signed.ErrorMessage);
                break;
            }

            draft.Packet.PlotSignature = signed.Result;
            var repeats = NoveltyCheck.Overlaps(signed.Result!, draft.EarlierPlots);
            draft.Log[^1].Issues = repeats;
            draft.Log[^1].Passed = repeats.Count == 0;
            if (repeats.Count == 0 || reroll >= NoveltyRerolls)
            {
                break;
            }

            logger.LogInformation("The conflict repeats an earlier plot; re-rolling it: {Repeats}", string.Join(" ", repeats));
            result = await RunStepAsync(conflict, new AssignedStage(draft), [.. feedback, .. repeats], caseAttempt, "Building another conflict.", progress, ct);
        }

        return result;
    }

    /// <summary>
    /// One step, and if it gives up, the same step once more from a fresh start: steps that already passed are kept
    /// (fixes, section 2.3). Marks the draft with the step that failed.
    /// </summary>
    private async Task<ResultBase> RunStepAsync<TIn, TOut>(
        IMysteryGenerationStep<TIn, TOut> step, TIn input, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, string message,
        IProgress<GenerationProgress>? progress, CancellationToken ct)
        where TIn : DraftStage
        where TOut : DraftStage
    {
        var ret = new ResultBase();
        progress?.Report(new GenerationProgress(caseAttempt, step.Name, message));
        var result = await step.RunAsync(input, feedback, caseAttempt, ct);
        if (!result.Success && result.ErrorCode == StepErrorCodes.StepFailed)
        {
            logger.LogInformation("Step {Step} gave up; starting it again on its own", step.Name);
            progress?.Report(new GenerationProgress(caseAttempt, step.Name, "Starting this step afresh."));
            result = await step.RunAsync(input, [], caseAttempt, ct, fresh: true);
        }

        if (result.Success)
        {
            return ret.Ok();
        }

        input.Draft.FailedStep = step.Name;
        return ret.Fail(result.ErrorMessage, result.ErrorCode);
    }

    /// <summary>The clue sub-steps in order, then the whole-case checks and the critic, reworking what they point at.</summary>
    /// <summary>The case's people, then the place's residents on the small model (fixes 4, section 10).</summary>
    private async Task<ResultBase> RunPeopleAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var o = Options;
        var people = await RunStepAsync(new HumansStep(model, validator, o), new EscalationStage(draft), feedback, caseAttempt, "Casting the people.", progress, ct);
        if (!people.Success)
        {
            return people;
        }

        // The place's people are the Society's roster: the residents are taken from it, without a model call.
        if (ResidentsStep.FromRoster(draft, o.RosterResidents))
        {
            progress?.Report(new GenerationProgress(caseAttempt, ResidentsStep.StepName, "Adding residents from the place's people."));
            return people;
        }

        return await RunStepAsync(new ResidentsStep(model, validator, o), new CastStage(draft), [], caseAttempt, "Adding the residents.", progress, ct);
    }

    private async Task<ResultBase> RunCluesAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var ret = new ResultBase();
        var o = Options;
        var pending = new SortedDictionary<int, IReadOnlyList<CoherenceIssue>> { [0] = feedback, [1] = [], [2] = [], [3] = [], [4] = [] };
        var criticReworks = 0;
        List<CoherenceIssue>? sentBack = null;
        for (var round = 0; ; round++)
        {
            foreach (var (sub, issues) in pending)
            {
                var result = sub switch
                {
                    0 => await RunStepAsync(new FactsStep(model, validator, o), new CastStage(draft), issues, caseAttempt, "Deciding the facts.", progress, ct),
                    1 => await RunStepAsync(new MindsStep(model, validator, o), new FactsStage(draft), issues, caseAttempt, "Placing who knows what, and every way in.", progress, ct),
                    2 => await RunStepAsync(new ArcStep(model, validator, o), new MindsStage(draft), issues, caseAttempt, "Shaping the beats and the endings.", progress, ct),
                    3 when !CreatureStep.Needed(draft) => new ResultBase().Ok(),
                    3 => await RunStepAsync(new CreatureStep(model, validator, o), new ArcStage(draft), issues, caseAttempt, "Giving the creature a mind.", progress, ct),
                    _ => await RunStepAsync(new HookStep(model, validator, o), new ArcStage(draft), issues, caseAttempt, "Writing the hook and the scenes.", progress, ct),
                };
                if (!result.Success)
                {
                    return result;
                }
            }

            pending = [];
            var whole = CheckWhole(draft, caseAttempt, round, progress);
            var errors = whole.Where(i => i.IsError).ToList();

            if (errors.Count == 0 && o.UseCritic)
            {
                progress?.Report(new GenerationProgress(caseAttempt, CriticStep.Name, "Reviewing the case."));
                var review = await new CriticStep(model, o).RunAsync(draft, caseAttempt, ct, sentBack);
                if (!review.Success)
                {
                    draft.FailedStep = CriticStep.Name;
                    return ret.Fail(review.ErrorMessage, review.ErrorCode);
                }

                var critique = review.Result!;
                draft.Packet.Validation.Issues.RemoveAll(i => i.Source == IssueSources.Critic);
                // A retold plot alone is not worth a round and a second look: no patch can change it.
                if (critique.Exists(i => i.IsError && i.Code != "critic." + CriticChecks.Novelty) && criticReworks < o.MaxCriticReworks)
                {
                    // The critic's objections, and its warnings with concrete fixes, go to the steps that own them and
                    // are fixed in place, in pipeline order; then the whole case is checked and reviewed once more.
                    criticReworks++;
                    sentBack = critique.Where(i => i.IsError || Fixable(i)).ToList();
                    var toFix = sentBack.Concat(TargetedWarnings(draft)).ToList();
                    progress?.Report(new GenerationProgress(caseAttempt, CheckStepName, "Fixing what the critic found, step by step."));
                    var before = Snapshot(draft);
                    await FixRoundAsync(draft, toFix, caseAttempt, light: false, ct);
                    if (StillPasses(draft, caseAttempt))
                    {
                        continue;
                    }

                    // The case passed before: fixes that break it are not kept, and the objections stay as warnings.
                    logger.LogInformation("The critic's fixes broke the case's checks; keeping it as it was");
                    Restore(draft, before);
                }

                // Past its rework, the critic's objections are warnings on a case that passes. Those with concrete
                // fixes are applied in one cheap round, kept only if the case still passes its checks.
                critique = critique.Select(i => i.IsError ? AsWarning(i) : i).ToList();
                draft.Packet.Validation.Issues.AddRange(critique);
                // Every critic issue it answered goes in, so each ends with a label: nothing to route is "not fixed" (fixes 5).
                var last = critique.Where(i => i.Location != "critic").Concat(TargetedWarnings(draft)).ToList();
                if (last.Count > 0)
                {
                    progress?.Report(new GenerationProgress(caseAttempt, CheckStepName, "Applying the critic's last suggestions, and checking each took."));
                    var unresolved = await ApplyWarningsAsync(draft, last, caseAttempt, ct);

                    // The whole-case warnings as the case now stands, not as it was before the fixes.
                    draft.Packet.Validation.Issues.RemoveAll(i => i.Source == IssueSources.Structural);
                    whole = validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Whole);
                    // A fix that did not hold is a warning marked unresolved, never "applied"; only one that breaks the table
                    // (or any, with UnresolvedFixIsError) fails the case.
                    if (unresolved.Any(u => o.UnresolvedFixIsError || BreaksTheTable(u)))
                    {
                        draft.FailedStep = CheckStepName;
                        draft.Packet.Validation.Issues.AddRange(whole);
                        return ret.Fail("Fixes that did not hold after a retry: " + string.Join(" ", unresolved.Select(u => $"{u.Code} at {string.Join(", ", u.Targets ?? [])}: {u.Unresolved}")),
                            StepErrorCodes.StepFailed);
                    }
                }

                errors = [];
            }

            if (errors.Count == 0)
            {
                draft.Completed |= CheckScope.Whole;
                draft.Packet.Validation.Issues.AddRange(whole);
                return ret.Ok();
            }

            if (round >= o.MaxReworkRounds)
            {
                draft.FailedStep = CheckStepName;
                draft.ReworkFeedback = errors;
                return ret.Fail($"The finished case still had problems after {o.MaxReworkRounds} reworks: " + string.Join(" ", errors.Take(5)), StepErrorCodes.StepFailed);
            }

            // Too few ways in is fixed by a patch of minds and channels (a channel added, a teller who knows it first-hand),
            // not by writing every mind and channel again.
            if (errors.TrueForAll(e => e.Code is IssueCodes.Redundancy or IssueCodes.SinglePointOfFailure) && draft.Outputs.ContainsKey(MindsStep.StepName))
            {
                progress?.Report(new GenerationProgress(caseAttempt, CheckStepName, "Adding ways in."));
                var patch = errors.SelectMany(e => WaysIn(draft.Packet, e)).ToList();
                if ((await new MindsStep(model, validator, o).FixAsync(draft, patch, caseAttempt, light: false, ct)).Success)
                {
                    pending = [];
                    continue;
                }
            }

            // Send each structural issue back to the sub-step it points at; new facts mean everything built on them is redone.
            pending = new SortedDictionary<int, IReadOnlyList<CoherenceIssue>>(errors
                .GroupBy(ClueSubStep)
                .ToDictionary(g => g.Key, g => (IReadOnlyList<CoherenceIssue>)g.ToList()));
            if (pending.ContainsKey(0))
            {
                foreach (var later in new[] { 1, 2, 3, 4 })
                {
                    pending.TryAdd(later, []);
                }
            }

            progress?.Report(new GenerationProgress(caseAttempt, CheckStepName,
                $"Reworking {string.Join(", ", pending.Keys.Select(k => ClueStepNames[k]))}."));
        }
    }

    /// <summary>The tension weights (they depend on the whole case), then the whole-case checks, logged as a check entry.</summary>
    private List<CoherenceIssue> CheckWhole(MysteryDraft draft, int caseAttempt, int round, IProgress<GenerationProgress>? progress)
    {
        progress?.Report(new GenerationProgress(caseAttempt, CheckStepName, "Checking the whole case."));
        var whole = Derived.Tension(draft.Packet);
        whole.AddRange(validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Whole));
        draft.Log.Add(new GenerationLogEntry
        {
            CaseAttempt = caseAttempt,
            Step = CheckStepName,
            Attempt = round,
            Kind = LogEntryKinds.Check,
            Passed = !whole.Exists(i => i.IsError),
            Issues = whole,
        });
        draft.Packet.Validation.Runs++;
        return whole;
    }

    /// <summary>
    /// The structural checks' warnings that name their items (an actor's action, a home trace, a reused name): they go
    /// to their owning steps with the critic's issues (fixes 3, sections 4, 6 and 7).
    /// </summary>
    private List<CoherenceIssue> TargetedWarnings(MysteryDraft draft) =>
        validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Full)
            .Where(i => !i.IsError && i.Source == IssueSources.Structural && Fixable(i))
            .ToList();

    /// <summary>A critic issue that names its items and says how to fix them.</summary>
    private static bool Fixable(CoherenceIssue issue) =>
        issue.Targets is { Count: > 0 } && !string.IsNullOrWhiteSpace(issue.SuggestedFix)
        || issue.Replacements is { Count: > 0 } || issue.SetFields is { Count: > 0 } || issue.ListEdits is { Count: > 0 } || issue.Patches is { Count: > 0 };

    private static CoherenceIssue AsWarning(CoherenceIssue issue) => new()
    {
        Code = issue.Code,
        Severity = IssueSeverities.Warning,
        Location = issue.Location,
        Explanation = issue.Explanation,
        SuggestedFix = issue.SuggestedFix,
        Source = issue.Source,
        Targets = issue.Targets,
        Replacements = issue.Replacements,
        Everywhere = issue.Everywhere,
        Blocking = issue.Blocking,
        SetFields = issue.SetFields,
        ListEdits = issue.ListEdits,
        Patches = issue.Patches,
        ImpactReason = issue.ImpactReason,
    };

    /// <summary>What a round of fixes did: the issues every part of which was done (each issue's parts carry their status).</summary>
    private sealed record FixRound(List<CoherenceIssue> Applied);

    /// <summary>
    /// The parts of an issue's fix, each with its owner (fixes 5, section 3): every replacement pair and field set is
    /// code's, every patch the step's that owns its target. An issue that names targets and a fix but no parts is one
    /// patch per target. A target the case does not have, or an issue with nothing to route, is unrouted; in a full
    /// round (not the light last one) a critic issue with no targets revises its step as a whole.
    /// </summary>
    internal static List<FixPart> PartsOf(CasePacket packet, CoherenceIssue issue, bool light)
    {
        var parts = new List<FixPart>();
        if (issue.Code == "critic." + CriticChecks.Novelty)
        {
            parts.Add(new FixPart { Kind = FixPartKinds.Patch, Status = FixPartStatuses.Unrouted, Note = "A retold plot is the whole design: it is for a person to judge, not a patch." });
            return parts;
        }

        var scope = string.Join(", ", issue.Everywhere ? ["everywhere"] : issue.Targets ?? []);
        parts.AddRange((issue.Replacements ?? []).Select(r => new FixPart { Kind = FixPartKinds.Replace, Owner = "code", Target = scope, Detail = $"\"{r.From}\" → \"{r.To}\"" }));
        parts.AddRange((issue.SetFields ?? []).Select(f => new FixPart { Kind = FixPartKinds.SetField, Owner = "code", Target = f.Path, Detail = f.Value }));
        parts.AddRange((issue.ListEdits ?? []).Select(e => new FixPart
        {
            Kind = FixPartKinds.ListEdit, Owner = "code", Target = e.Target, Detail = $"{e.Op} {e.Value}{(e.Source.Length > 0 ? $" ({e.Source})" : "")}",
        }));
        var asks = issue.Patches is { Count: > 0 } patches
            ? patches.Select(p => (p.Target, Fix: string.IsNullOrWhiteSpace(p.Fix) ? issue.SuggestedFix ?? "" : p.Fix, p.Change)).ToList()
            : parts.Count == 0 ? (issue.Targets ?? []).Select(t => (Target: t, Fix: issue.SuggestedFix ?? "", Change: PatchChanges.Reword)).ToList() : [];
        var tree = CaseItems.Tree(packet);
        foreach (var (target, fix, asked) in asks)
        {
            var change = ChangeOf(packet, issue, target.Trim(), asked);
            var (step, location) = change switch
            {
                PatchChanges.AddChannel => (MindsStep.StepName, Patch.NewItem + "channel"),
                PatchChanges.AddPerson => (HumansStep.StepName, Patch.NewItem + "cast"),
                _ => CriticRouting.Owner(target, packet),
            };
            var known = location.StartsWith(Patch.NewItem, StringComparison.Ordinal) || CaseItems.Json(tree, target) is not null;
            parts.Add(new FixPart
            {
                Kind = FixPartKinds.Patch,
                Owner = known ? step : "",
                Target = target,
                Change = change == PatchChanges.Reword ? null : change,
                Detail = fix,
                Status = known ? FixPartStatuses.Pending : FixPartStatuses.Unrouted,
                Note = known ? null : $"The case has no item {target}.",
            });
        }

        if (parts.Count == 0)
        {
            parts.Add(!light && issue.Source == IssueSources.Critic
                ? new FixPart { Kind = FixPartKinds.Patch, Owner = CriticRouting.DefaultStep(issue.Code), Target = "", Detail = issue.SuggestedFix ?? issue.Explanation }
                : new FixPart { Kind = FixPartKinds.Patch, Status = FixPartStatuses.Unrouted, Note = "Nothing to route: no target and no operation." });
        }

        return parts;
    }

    /// <summary>The item a field path is on: "channels[id=c44]" for "channels[id=c44].placeId".</summary>
    private static string ItemOf(string path)
    {
        var dot = path.LastIndexOf('.');
        return dot > 0 ? path[..dot] : path;
    }

    /// <summary>
    /// What a patch changes (review of case 09, problem 2), as the critic said or as its target shows: a fair-clues
    /// patch on a fact asks for a way in, which only a new channel gives; a person the case does not have yet is a new
    /// person. A reword goes to the step that owns the target.
    /// </summary>
    internal static string ChangeOf(CasePacket packet, CoherenceIssue issue, string target, string? asked)
    {
        if (asked is PatchChanges.AddChannel or PatchChanges.AddPerson)
        {
            return asked;
        }

        var id = target.Split('.')[0];
        if (issue.Code == "critic." + CriticChecks.FairClues && packet.FindFact(id) is not null)
        {
            return PatchChanges.AddChannel;
        }

        return id.StartsWith("npc-", StringComparison.Ordinal) && packet.FindCast(id) is null ? PatchChanges.AddPerson : PatchChanges.Reword;
    }

    /// <summary>
    /// Fixes the issues' parts (fixes 2, section 1; review 3, item 1; fixes 5, sections 1 to 3). Code parts first, one at
    /// a time; then each step's patches, in pipeline order. Every part is checked on its own: one that adds a structural
    /// error the case did not have gets one repair from the steps that own the broken checks, and if that does not hold,
    /// it alone is undone. A part is done only when its post-condition holds: a pair's text is gone where it was to be
    /// replaced, a field reads back as set, a patched item is not as it was.
    /// </summary>
    private async Task<FixRound> FixRoundAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, int caseAttempt, bool light, CancellationToken ct)
    {
        var o = Options;
        var steps = new Dictionary<string, IFixableStep>(StringComparer.Ordinal)
        {
            [ConflictStep.StepName] = new ConflictStep(model, validator, o),
            [EscalationStep.StepName] = new EscalationStep(model, validator, o),
            [HumansStep.StepName] = new HumansStep(model, validator, o),
            [ResidentsStep.StepName] = new ResidentsStep(model, validator, o),
            [FactsStep.StepName] = new FactsStep(model, validator, o),
            [MindsStep.StepName] = new MindsStep(model, validator, o),
            [ArcStep.StepName] = new ArcStep(model, validator, o),
            [CreatureStep.StepName] = new CreatureStep(model, validator, o),
            [HookStep.StepName] = new HookStep(model, validator, o),
        };

        foreach (var issue in issues)
        {
            issue.Parts = PartsOf(draft.Packet, issue, light);
        }

        // What the case already fails is not any fix's fault: only new errors count against a fix.
        var baseline = ErrorKeys(Errors(draft));

        // The items as they were when the round began: a patch is judged against these, so an item a code part of the
        // same issue already changed (a rank set, then the identity patched) counts as changed.
        var roundStart = CaseItems.Tree(draft.Packet);

        // Code parts: each pair on its own, so one that matched nothing or broke a check costs only itself; the fields
        // of one item together (review of case 09, problem 6): a channel's placeId and affordanceId each break the
        // affordance check alone, and set one at a time both were undone.
        foreach (var issue in issues)
        {
            var codeParts = issue.Parts!.Where(p => p.Owner == "code").ToList();
            var pairs = issue.Replacements ?? [];
            var fields = issue.SetFields ?? [];
            var edits = issue.ListEdits ?? [];
            var units = codeParts.Select((part, i) => (Part: part, Index: i))
                .GroupBy(x => x.Part.Kind switch
                {
                    FixPartKinds.Replace => $"pair:{x.Index}",
                    FixPartKinds.ListEdit => $"edit:{x.Index}",
                    _ => "field:" + ItemOf(x.Part.Target),
                })
                .Select(g => g.ToList());
            foreach (var unit in units)
            {
                var before = Snapshot(draft);
                var failed = false;
                foreach (var (part, i) in unit)
                {
                    var result = part.Kind switch
                    {
                        FixPartKinds.Replace => TextReplacements.Apply(draft, new CoherenceIssue { Targets = issue.Targets, Everywhere = issue.Everywhere, Replacements = [pairs[i]] })[0],
                        FixPartKinds.SetField => TextReplacements.SetField(draft, fields[i - pairs.Count]),
                        _ => TextReplacements.Edit(draft, edits[i - pairs.Count - fields.Count]),
                    };
                    draft.Log.Add(new GenerationLogEntry
                    {
                        CaseAttempt = caseAttempt,
                        Step = CheckStepName,
                        Kind = LogEntryKinds.Replace,
                        Passed = result.Ok,
                        Instruction = $"{issue.Code}: {result.Op} {result.What} in {part.Target}",
                        Output = result.ToString(),
                    });
                    part.Note = result.ToString();
                    part.Status = result.Ok ? FixPartStatuses.Done : FixPartStatuses.Failed;
                    failed |= !result.Ok;
                }

                if (failed)
                {
                    // One field of an item that did not take leaves the item as it was: half a move is no move.
                    Restore(draft, before);
                    foreach (var (part, _) in unit.Where(u => u.Part.Status == FixPartStatuses.Done))
                    {
                        part.Status = FixPartStatuses.Failed;
                        part.Note = "Undone with the other fields of its item, one of which did not take.";
                    }

                    continue;
                }

                if (NewErrors(draft, baseline) is { Count: > 0 } broke)
                {
                    Restore(draft, before);
                    foreach (var (part, _) in unit)
                    {
                        part.Status = FixPartStatuses.Undone;
                        part.Note = $"It broke {Describe(broke)} and was undone.";
                    }

                    LogUndo(draft, caseAttempt, string.Join("; ", unit.Select(u => $"{u.Part.Kind} {u.Part.Target}")), broke);
                }
            }
        }

        // Model parts, grouped by the step that owns each; a resident's record is the residents step's, not humans'.
        var residents = draft.Outputs.TryGetValue(ResidentsStep.StepName, out var kept) && kept is ResidentsOutput r
            ? r.Residents.Select(c => $"cast:{c.CharacterId}").ToHashSet(StringComparer.Ordinal)
            : [];
        var routed = new Dictionary<string, List<(FixPart Part, CoherenceIssue Ask)>>(StringComparer.Ordinal);
        foreach (var issue in issues)
        {
            foreach (var part in issue.Parts!.Where(p => p.Kind == FixPartKinds.Patch && p.Status == FixPartStatuses.Pending))
            {
                var location = part.Change switch
                {
                    PatchChanges.AddChannel => Patch.NewItem + "channel",
                    PatchChanges.AddPerson => Patch.NewItem + "cast",
                    _ => part.Target.Length == 0 ? issue.Location : CriticRouting.Owner(part.Target, draft.Packet).Location,
                };
                var step = part.Owner == HumansStep.StepName && residents.Contains(location) ? ResidentsStep.StepName : part.Owner;
                var ask = new CoherenceIssue
                {
                    Code = issue.Code, Severity = issue.Severity, Location = location, Explanation = issue.Explanation,
                    SuggestedFix = part.Change switch
                    {
                        PatchChanges.AddChannel => $"Add a new channel for {part.Target.Split('.')[0]} (another person, or a trace or document at another spot): {part.Detail}",
                        PatchChanges.AddPerson => $"Add {part.Target.Split('.')[0]} to the cast as a new person: {part.Detail}",
                        _ => part.Detail,
                    },
                    Source = issue.Source, Targets = part.Target.Length == 0 ? issue.Targets : [part.Target],
                };
                if (!routed.TryGetValue(step, out var list))
                {
                    routed[step] = list = [];
                }

                list.Add((part, ask));
            }
        }

        Reroute(draft, routed, steps, light);
        foreach (var step in CriticRouting.PipelineOrder)
        {
            if (!routed.TryGetValue(step, out var asks) || asks.Count == 0)
            {
                continue;
            }

            var before = Snapshot(draft);
            var beforeTree = CaseItems.Tree(before.Packet);
            var beforeOutput = Newtonsoft.Json.JsonConvert.SerializeObject(before.Outputs.GetValueOrDefault(step));
            var result = await steps[step].FixAsync(draft, asks.Select(a => a.Ask).ToList(), caseAttempt, light, ct);

            // A new person is not done until minds give them what they know (review of case 09, problem 2): ask in
            // the same unit, before checking, or the person with no mind would count as a fix that broke the case.
            var added = draft.Packet.Cast.Where(c => before.Packet.FindCast(c.CharacterId) is null).ToList();
            if (result.Success && added.Count > 0 && step != MindsStep.StepName && asks.Exists(a => a.Part.Change == PatchChanges.AddPerson))
            {
                var minds = added.Select(c => new CoherenceIssue
                {
                    Code = IssueCodes.Consistency, Severity = IssueSeverities.Error, Location = Patch.NewItem + "cast", Source = IssueSources.Structural,
                    Explanation = $"{c.Name} ({c.CharacterId}) has just joined the case and has no mind yet.",
                    SuggestedFix = $"Add {c.CharacterId}'s mind (what they know, think and lie about) and any channel they are, for this: "
                        + string.Join(" ", asks.Where(a => a.Part.Change == PatchChanges.AddPerson).Select(a => a.Part.Detail)),
                }).ToList();
                result = await steps[MindsStep.StepName].FixAsync(draft, minds, caseAttempt, light, ct);
            }

            if (!result.Success)
            {
                var skipped = result.ErrorCode == StepErrorCodes.SkippedBudget;
                foreach (var (part, _) in asks.Where(a => a.Part.Status == FixPartStatuses.Pending))
                {
                    part.Status = skipped ? FixPartStatuses.Skipped : FixPartStatuses.Failed;
                    part.Note = skipped ? result.ErrorMessage : $"The {step} patch did not pass that step's checks: {result.ErrorMessage}";
                }

                logger.LogWarning("The fix for {Step} did not pass its checks and was not applied: {Error}", step, result.ErrorMessage);
                continue;
            }

            if (NewErrors(draft, baseline) is { Count: > 0 } broke)
            {
                // One repair, asked of the steps that own what broke, with what broke: never the same fix sent again.
                logger.LogInformation("The fix for {Step} broke {Broke}; asking for a repair", step, Describe(broke));
                var repaired = await RepairAsync(draft, broke, steps, caseAttempt, light, ct) && NewErrors(draft, baseline).Count == 0;
                if (!repaired)
                {
                    Restore(draft, before);
                    foreach (var (part, _) in asks)
                    {
                        part.Status = FixPartStatuses.Undone;
                        part.Note = $"The {step} patch broke {Describe(broke)}; one repair did not hold, so it alone was undone.";
                    }

                    LogUndo(draft, caseAttempt, step, broke);
                    continue;
                }
            }

            // The post-condition of a patch: the item it names is not as it was ("the call succeeded" is not one).
            var afterTree = CaseItems.Tree(draft.Packet);
            var afterOutput = Newtonsoft.Json.JsonConvert.SerializeObject(draft.Outputs.GetValueOrDefault(step));
            // A new channel's: the fact has more ways in than it had; a new person's: the cast has someone it did not.
            foreach (var (part, _) in asks)
            {
                var id = part.Target.Split('.')[0];
                var changed = part.Change switch
                {
                    PatchChanges.AddChannel => draft.Packet.Channels.Count(c => c.FactId == id) > before.Packet.Channels.Count(c => c.FactId == id),
                    PatchChanges.AddPerson => draft.Packet.Cast.Exists(c => before.Packet.FindCast(c.CharacterId) is null),
                    _ when part.Target.Length == 0 => beforeOutput != afterOutput,
                    _ => CaseItems.Json(beforeTree, part.Target) != CaseItems.Json(afterTree, part.Target)
                        || CaseItems.Json(roundStart, part.Target) != CaseItems.Json(afterTree, part.Target),
                };
                part.Status = changed ? FixPartStatuses.Done : FixPartStatuses.Failed;
                part.Note = changed ? null : part.Change switch
                {
                    PatchChanges.AddChannel => $"The {step} patch added no channel for {id}.",
                    PatchChanges.AddPerson => $"The {step} patch added no one to the cast.",
                    _ => $"The {step} patch left {(part.Target.Length == 0 ? "the step" : part.Target)} as it was.",
                };
            }

            // What this fix changed reaches the later steps whose items are built on it.
            foreach (var (later, follow) in CriticRouting.Dependents(before.Packet, draft.Packet))
            {
                if (!routed.TryGetValue(later, out var list))
                {
                    routed[later] = list = [];
                }

                list.Add((new FixPart { Kind = FixPartKinds.Patch, Owner = later, Target = "", Detail = follow.SuggestedFix ?? "" }, follow));
            }
        }

        return new FixRound(issues.Where(i => i.Code != IssueCodes.Consistency && i.Parts!.TrueForAll(p => p.Status == FixPartStatuses.Done)).ToList());
    }

    /// <summary>
    /// Each part goes to the step whose answer holds its item (review of case 10, problem 1): one no step holds fails
    /// alone, with why, and never sinks the parts sent with it. A part that names no item asks for its step to be
    /// revised whole, which the last round has no budget for: it is skipped there, and stays open for review.
    /// </summary>
    private static void Reroute(MysteryDraft draft, Dictionary<string, List<(FixPart Part, CoherenceIssue Ask)>> routed, Dictionary<string, IFixableStep> steps, bool light)
    {
        foreach (var (step, asks) in routed.ToList())
        {
            foreach (var ask in asks.ToList())
            {
                if (ask.Part.Target.Length == 0)
                {
                    if (light && !steps[step].Holds(draft, ask.Ask.Location))
                    {
                        asks.Remove(ask);
                        ask.Part.Status = FixPartStatuses.Skipped;
                        ask.Part.Note = $"Skipped (budget): it names no item, so it needs the whole {step} step revised, and the last round only patches items.";
                    }

                    continue;
                }

                if (steps[step].Holds(draft, ask.Ask.Location))
                {
                    continue;
                }

                asks.Remove(ask);
                var holder = CriticRouting.PipelineOrder.FirstOrDefault(s => steps[s].Holds(draft, ask.Ask.Location));
                if (holder is null)
                {
                    ask.Part.Status = FixPartStatuses.Failed;
                    ask.Part.Note = $"No step's answer holds {ask.Ask.Location} ({ask.Part.Target}).";
                    continue;
                }

                ask.Part.Owner = holder;
                if (!routed.TryGetValue(holder, out var list))
                {
                    routed[holder] = list = [];
                }

                list.Add(ask);
            }
        }
    }

    /// <summary>
    /// One repair of the checks a fix broke (review 3, item 2): too few ways in becomes a new channel or a first-hand
    /// teller from minds and channels; anything else goes, as an error, to the step that owns the item it names.
    /// </summary>
    private static async Task<bool> RepairAsync(MysteryDraft draft, List<CoherenceIssue> broke, Dictionary<string, IFixableStep> steps, int caseAttempt, bool light, CancellationToken ct)
    {
        static bool WaysInIssue(CoherenceIssue e) => e.Code is IssueCodes.Redundancy or IssueCodes.SinglePointOfFailure;
        var asks = broke.Where(WaysInIssue)
            .SelectMany(e => WaysIn(draft.Packet, e))
            .Select(e => (Step: MindsStep.StepName, Issue: e))
            .Concat(broke.Where(e => !WaysInIssue(e)).Select(e => (Step: RepairOwner(e), Issue: e)))
            .GroupBy(a => a.Step)
            .OrderBy(g => CriticRouting.PipelineOrder.ToList().IndexOf(g.Key));
        foreach (var ask in asks)
        {
            if (!steps.TryGetValue(ask.Key, out var step) || !(await step.FixAsync(draft, ask.Select(a => a.Issue).ToList(), caseAttempt, light, ct)).Success)
            {
                return false;
            }
        }

        return true;
    }

    /// <summary>The step that owns a structural issue's item: what a person knows is minds and channels', the rest by its id.</summary>
    internal static string RepairOwner(CoherenceIssue issue)
    {
        var colon = issue.Location.IndexOf(':');
        var id = colon >= 0 ? issue.Location[(colon + 1)..] : issue.Location;
        if (issue.Location.StartsWith("cast:", StringComparison.Ordinal))
        {
            return issue.Code == IssueCodes.Provenance ? MindsStep.StepName : CriticRouting.Owner(id).Step;
        }

        if (colon < 0 || issue.Location.StartsWith("scene", StringComparison.Ordinal))
        {
            return ClueStepNames[ClueSubStep(issue)];
        }

        return CriticRouting.Owner(id).Step;
    }

    /// <summary>Every structural error the case has now, the tension's included.</summary>
    private List<CoherenceIssue> Errors(MysteryDraft draft)
    {
        var check = Derived.Tension(draft.Packet);
        check.AddRange(validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Full));
        return check.Where(i => i.IsError).ToList();
    }

    /// <summary>The errors the case has now that it did not have at <paramref name="baseline"/>.</summary>
    private List<CoherenceIssue> NewErrors(MysteryDraft draft, HashSet<string> baseline) =>
        Errors(draft).Where(e => !baseline.Contains(ErrorKey(e))).ToList();

    private static HashSet<string> ErrorKeys(IEnumerable<CoherenceIssue> errors) => errors.Select(ErrorKey).ToHashSet(StringComparer.Ordinal);

    private static string ErrorKey(CoherenceIssue e) => $"{e.Code}|{e.Location}";

    private static string Describe(List<CoherenceIssue> broke) =>
        string.Join("; ", broke.Take(3).Select(b => $"{b.Code} at {b.Location}")) + (broke.Count > 3 ? $" and {broke.Count - 3} more" : "");

    private static void LogUndo(MysteryDraft draft, int caseAttempt, string what, List<CoherenceIssue> broke) => draft.Log.Add(new GenerationLogEntry
    {
        CaseAttempt = caseAttempt,
        Step = CheckStepName,
        Kind = LogEntryKinds.Undo,
        Passed = false,
        Instruction = $"Undid the {what} fix alone: it broke {Describe(broke)}.",
        Issues = broke,
    });

    /// <summary>
    /// The last round (fixes 2, section 2; review 3, items 1 and 2; fixes 5, sections 3 and 4): every remaining critic
    /// issue and every targeted structural warning, split into parts and fixed by code and patches, never whole-step
    /// rewrites. Then the labels: an issue whose parts all held goes to a scoped re-check on the small model, which sees
    /// only the items it names, before and after; a structural warning is re-checked by its check. What the re-check
    /// finds still standing is tried once more with what it found; an undone part and an instruction already sent are
    /// not. Returns the issues not fixed.
    /// </summary>
    private async Task<List<CoherenceIssue>> ApplyWarningsAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> warnings, int caseAttempt, CancellationToken ct)
    {
        var originals = warnings.Distinct().ToList();
        var reasons = new Dictionary<CoherenceIssue, string?>();
        var rechecked = new HashSet<CoherenceIssue>();
        var sent = new HashSet<string>(StringComparer.Ordinal);
        var startPacket = draft.ClonePacket();

        // What is asked this time, by the original issue it stands for.
        var asked = originals.ToDictionary(i => i, i => i);
        for (var attempt = 0; attempt <= Options.FixRetries && asked.Count > 0; attempt++)
        {
            foreach (var ask in asked.Values)
            {
                sent.Add(Signature(ask));
            }

            var before = Snapshot(draft);
            await FixRoundAsync(draft, asked.Values.ToList(), caseAttempt, light: true, ct);
            if (!StillPasses(draft, caseAttempt))
            {
                // Each part was checked alone, so this is parts that clash: keep the case as it was.
                logger.LogInformation("The last round's fixes broke the case's checks together; keeping it as it was");
                Restore(draft, before);
                foreach (var (original, ask) in asked)
                {
                    foreach (var part in ask.Parts ?? [])
                    {
                        part.Status = FixPartStatuses.Undone;
                        part.Note = "It clashed with another fix in the same round and was undone.";
                    }

                    original.Parts = Merge(original.Parts, ask.Parts);
                }

                break;
            }

            foreach (var (original, ask) in asked)
            {
                original.Parts = Merge(original.Parts, ask.Parts);
            }

            // The scoped re-check, for issues whose every part held.
            var whole = asked.Keys.Where(i => i.Parts!.TrueForAll(p => p.Status == FixPartStatuses.Done)).ToList();
            var now = validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Full);
            var critic = whole.Where(i => i.Source == IssueSources.Critic).ToList();
            var looked = critic.Count == 0 ? null : (await new FixVerifier(model, Options).VerifyAsync(draft, critic, startPacket, caseAttempt, ct)).Result;
            var again = new Dictionary<CoherenceIssue, CoherenceIssue>();
            foreach (var issue in whole)
            {
                string? still;
                if (issue.Source == IssueSources.Structural)
                {
                    still = now.Exists(n => n.Code == issue.Code && n.Location == issue.Location) ? "The check still finds it." : null;
                }
                else if (looked is null)
                {
                    // No re-check ran: fixed, but not verified.
                    reasons.Remove(issue);
                    rechecked.Remove(issue);
                    continue;
                }
                else
                {
                    still = looked[critic.IndexOf(issue)];
                }

                rechecked.Add(issue);
                reasons[issue] = still;
                if (still is null)
                {
                    continue;
                }

                // Tried again only with what the re-check found, as patches of the same items; never the same words twice.
                var retry = Retry(issue, still);
                if (!sent.Contains(Signature(retry)))
                {
                    again[issue] = retry;
                }
            }

            asked = again;
        }

        foreach (var issue in originals)
        {
            var parts = issue.Parts ?? [];
            (issue.Label, issue.Unresolved) = parts switch
            {
                _ when parts.Exists(p => p.Status == FixPartStatuses.Undone) => (FixLabels.DidNotHold, parts.First(p => p.Status == FixPartStatuses.Undone).Note),
                _ when parts.Exists(p => p.Status is FixPartStatuses.Failed or FixPartStatuses.Unrouted or FixPartStatuses.Pending) =>
                    (FixLabels.NotFixed, string.Join(" ", parts.Where(p => p.Status != FixPartStatuses.Done).Select(p => $"{(p.Target.Length > 0 ? p.Target : p.Kind)}: {p.Note ?? p.Status}."))),
                _ when !rechecked.Contains(issue) => (FixLabels.Unverified, null),
                _ when reasons.GetValueOrDefault(issue) is { } still => (FixLabels.NotFixed, still),
                _ => (FixLabels.Verified, null),
            };
            issue.Applied = FixLabels.IsFixed(issue.Label);
        }

        // The packet's critic issues are these same objects when the critique was passed in; copy across in case not.
        foreach (var issue in draft.Packet.Validation.Issues.Where(i => i.Source == IssueSources.Critic))
        {
            if (originals.Find(o => o.Code == issue.Code && o.Explanation == issue.Explanation) is { } done && !ReferenceEquals(done, issue))
            {
                (issue.Label, issue.Unresolved, issue.Applied, issue.Parts) = (done.Label, done.Unresolved, done.Applied, done.Parts);
            }
        }

        logger.LogInformation("Last round: {Fixed} fixed, {Open} not", originals.Count(i => i.Applied), originals.Count(i => !i.Applied));

        // The structural ones the checks still find (a second hook-giver left standing) count as much as the critic's.
        var structural = originals.Where(i => i.Source == IssueSources.Structural && !i.Applied).ToList();
        return draft.Packet.Validation.Issues.Where(i => i.Source == IssueSources.Critic && i.Label is not null && !FixLabels.IsFixed(i.Label)).Concat(structural).ToList();
    }

    /// <summary>The parts as they now stand: a retry's parts replace the parts they redo, done parts stay.</summary>
    private static List<FixPart> Merge(List<FixPart>? had, List<FixPart>? now)
    {
        if (had is null || ReferenceEquals(had, now))
        {
            return now ?? [];
        }

        var redone = (now ?? []).Select(p => p.Target).ToHashSet(StringComparer.Ordinal);
        return had.Where(p => p.Status == FixPartStatuses.Done && p.Kind != FixPartKinds.Patch || !redone.Contains(p.Target)).Concat(now ?? []).ToList();
    }

    /// <summary>The issue again as patches of the items it names, with what the re-check found still standing.</summary>
    private static CoherenceIssue Retry(CoherenceIssue issue, string still)
    {
        var patched = (issue.Parts ?? []).Where(p => p.Kind == FixPartKinds.Patch && p.Target.Length > 0).Select(p => p.Target).ToList();
        var targets = patched.Count > 0 ? patched : issue.Targets ?? [];
        return new CoherenceIssue
        {
            Code = issue.Code,
            Severity = issue.Severity,
            Location = issue.Location,
            Explanation = issue.Explanation,
            SuggestedFix = $"{issue.SuggestedFix} Still not done: {still}",
            Source = issue.Source,
            Targets = issue.Targets,
            Patches = targets.Select(t => new PatchAsk { Target = t, Fix = $"{issue.SuggestedFix} Still not done: {still}" }).ToList(),
            Blocking = issue.Blocking,
        };
    }

    /// <summary>What an instruction asks, to never send the same one twice.</summary>
    private static string Signature(CoherenceIssue issue) =>
        $"{issue.Code}|{issue.Location}|{string.Join(",", issue.Targets ?? [])}|{issue.Explanation}|{issue.SuggestedFix}|{string.Join(",", (issue.Patches ?? []).Select(p => p.Target + p.Fix))}";

    /// <summary>A fix that must hold for the case to be played: two people claiming to have fetched the Society.</summary>
    private static bool BreaksTheTable(CoherenceIssue issue) =>
        issue.Source == IssueSources.Structural && issue.Code == IssueCodes.Opening && issue.Location.StartsWith("cast:", StringComparison.Ordinal);

    /// <summary>
    /// A redundancy or single-point issue as a patch of minds and channels: a new channel, and the minds of the people
    /// whose testimony is only hearsay, so one of them can be given the fact first-hand.
    /// </summary>
    private static IEnumerable<CoherenceIssue> WaysIn(CasePacket packet, CoherenceIssue issue)
    {
        yield return new CoherenceIssue
        {
            Code = issue.Code, Severity = IssueSeverities.Error, Location = Patch.NewItem + "channel", Explanation = issue.Explanation,
            SuggestedFix = (issue.SuggestedFix ?? "") + " Add an independent channel (another person, or a trace or document at another spot), or give a teller first-hand knowledge.",
        };

        var factId = issue.Location.StartsWith("fact:", StringComparison.Ordinal) ? issue.Location[5..] : null;
        var graph = new FactGraph(packet);
        foreach (var teller in packet.Channels.Where(c => c.FactId == factId && graph.IsHearsay(c)).Select(c => c.SourceCharacterId).OfType<string>().Distinct())
        {
            yield return new CoherenceIssue
            {
                Code = issue.Code, Severity = IssueSeverities.Error, Location = $"cast:{teller}",
                Explanation = $"Knows {factId} only from a bystander, so their testimony does not count toward its two ways in.",
                SuggestedFix = $"If it fits, let them know {factId} first-hand (witnessed, self, lore, or told by a cast member who knows it).",
            };
        }
    }

    /// <summary>The packet and the steps' outputs, to go back to when fixes break a case that passed.</summary>
    private static (CasePacket Packet, Dictionary<string, object> Outputs) Snapshot(MysteryDraft draft) =>
        (draft.ClonePacket(), draft.Outputs.ToDictionary(p => p.Key, p => p.Value, StringComparer.Ordinal));

    private static void Restore(MysteryDraft draft, (CasePacket Packet, Dictionary<string, object> Outputs) snapshot)
    {
        draft.Packet = snapshot.Packet;
        draft.Outputs.Clear();
        foreach (var (step, output) in snapshot.Outputs)
        {
            draft.Outputs[step] = output;
        }
    }

    /// <summary>Every structural check on the whole case, after a round of fixes; logged as a check entry.</summary>
    private bool StillPasses(MysteryDraft draft, int caseAttempt)
    {
        var check = Derived.Tension(draft.Packet);
        check.AddRange(validator.Validate(draft.Packet, draft.ValidationContext, CheckScope.Full));
        var passed = !check.Exists(i => i.IsError);
        draft.Log.Add(new GenerationLogEntry { CaseAttempt = caseAttempt, Step = CheckStepName, Kind = LogEntryKinds.Check, Passed = passed, Issues = check });
        return passed;
    }

    public const string CheckStepName = "check";

    private static readonly string[] ClueStepNames = [FactsStep.StepName, MindsStep.StepName, ArcStep.StepName, CreatureStep.StepName, HookStep.StepName];

    /// <summary>Which clue sub-step a whole-case or critic issue is for: 0 facts, 1 minds and channels, 2 arc, 3 the creature's mind, 4 hook.</summary>
    internal static int ClueSubStep(CoherenceIssue issue) => issue.Code switch
    {
        IssueCodes.Opening => 4,
        "critic." + CriticChecks.Hook => 4,
        "critic." + CriticChecks.Scenes => 4,
        "critic." + CriticChecks.Solves => 2,
        "critic." + CriticChecks.DeepLoreTrail => 0,
        _ when issue.Location.StartsWith("hook", StringComparison.Ordinal) || issue.Location.StartsWith("scene", StringComparison.Ordinal) => 4,
        _ when issue.Location.StartsWith("creatureMinds", StringComparison.Ordinal) => 3,
        _ when issue.Location == "facts" && issue.Code is IssueCodes.Schema or IssueCodes.Giveaway => 0,
        _ when issue.Location.StartsWith("beat", StringComparison.Ordinal) || issue.Location == "climax" => 2,
        _ => 1,
    };

    /// <summary>
    /// The earlier stage a structural issue points at, or -1: the conflict's truth, an escalation event, or a missing
    /// person. The critic's issues never send work back past the clues: a location it mentions is not a fault there.
    /// </summary>
    private static int EarlierStage(CoherenceIssue issue) => issue switch
    {
        { Source: not IssueSources.Structural } => -1,
        { Code: IssueCodes.UnknownReference } when issue.Explanation.StartsWith("Unknown cast member", StringComparison.Ordinal)
            || issue.Explanation.StartsWith("Mind for", StringComparison.Ordinal) => 2,
        { Code: IssueCodes.UnknownReference } when issue.Explanation.StartsWith("Unknown escalation event", StringComparison.Ordinal)
            || issue.Explanation.StartsWith("Unknown opening event", StringComparison.Ordinal) => 1,
        _ when issue.Location.StartsWith("truth.escalation", StringComparison.Ordinal) => 1,
        _ when issue.Location.StartsWith("truth.", StringComparison.Ordinal) || issue.Location == "brief" => 0,
        _ => -1,
    };

    private static string StageOf(string? step) => step switch
    {
        ConflictStep.StepName or EscalationStep.StepName or HumansStep.StepName => step,
        _ => "clues",
    };

    private static List<CoherenceIssue> LastIssues(MysteryDraft draft, string? step) =>
        draft.Log.LastOrDefault(e => e.Step == step)?.Issues.Where(i => i.IsError).ToList() ?? [];

    /// <summary>
    /// Stamps the packet and replaces the accumulated issues with one final structural run plus the critic's. A case
    /// that failed is checked only on the steps it completed, and says once which step did not (fixes, section 6.2).
    /// </summary>
    private void Finish(MysteryDraft draft, bool passed)
    {
        var packet = draft.Packet;
        packet.GeneratedAt = DateTime.UtcNow;
        var critic = packet.Validation.Issues.Where(i => i.Source == IssueSources.Critic).ToList();
        var finishedButFailed = draft.FailedStep is CheckStepName or CriticStep.Name;
        var scope = passed ? CheckScope.Full : draft.Completed | (finishedButFailed ? CheckScope.Whole : 0);
        var structural = validator.Validate(packet, draft.ValidationContext, scope);
        if (!passed && !finishedButFailed)
        {
            structural.Insert(0, CoherenceIssue.Error(IssueCodes.Incomplete, draft.FailedStep ?? "case",
                $"Step {draft.FailedStep ?? "unknown"} did not complete, so the case after it is missing."));
        }

        // A repeat the conflict's re-roll did not shake off blocks on its own (fixes 7, section 4); the critic's novelty
        // check then blocks only when this one found nothing and the critic still sees a repeat.
        var repeats = packet.PlotSignature is { } signature
            ? NoveltyCheck.Overlaps(signature, draft.EarlierPlots).Select(r => new CoherenceIssue
            {
                Code = r.Code, Severity = IssueSeverities.Warning, Location = r.Location, Explanation = r.Explanation, SuggestedFix = r.SuggestedFix,
                Source = IssueSources.Structural, Blocking = true,
            }).ToList()
            : [];
        foreach (var novelty in critic.Where(i => i.Code == "critic." + CriticChecks.Novelty && repeats.Count > 0))
        {
            novelty.Blocking = false;
        }

        packet.Validation.Issues = structural.Concat(repeats).Concat(critic).ToList();
        packet.Validation.Passed = passed && !structural.Exists(i => i.IsError);

        // A blocking issue not fixed (review 3, item 3; fixes 5, section 5): the case is kept, but needs review.
        packet.Validation.NeedsReview = packet.Validation.Passed && packet.Validation.Issues.Exists(i => i.Blocking && !FixLabels.IsFixed(i.Label));
        packet.Validation.Runs++;
    }
}
