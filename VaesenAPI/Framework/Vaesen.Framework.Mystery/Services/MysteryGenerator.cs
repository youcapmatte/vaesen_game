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
            0 => await RunStepAsync(new ConflictStep(model, validator, o), new AssignedStage(draft), feedback, caseAttempt, "Building the conflict.", progress, ct),
            1 => await RunStepAsync(new EscalationStep(model, validator, o), new ConflictStage(draft), feedback, caseAttempt, "Writing what has happened.", progress, ct),
            2 => await RunPeopleAsync(draft, feedback, caseAttempt, progress, ct),
            _ => await RunCluesAsync(draft, feedback, caseAttempt, progress, ct),
        };
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
                if (critique.Exists(i => i.IsError) && criticReworks < o.MaxCriticReworks)
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
                var last = critique.Where(Fixable).Concat(TargetedWarnings(draft)).ToList();
                if (last.Count > 0)
                {
                    progress?.Report(new GenerationProgress(caseAttempt, CheckStepName, "Applying the critic's last suggestions."));
                    await ApplyWarningsAsync(draft, last, caseAttempt, ct);
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
    private static bool Fixable(CoherenceIssue issue) => issue.Targets is { Count: > 0 } && !string.IsNullOrWhiteSpace(issue.SuggestedFix);

    private static CoherenceIssue AsWarning(CoherenceIssue issue) => new()
    {
        Code = issue.Code,
        Severity = IssueSeverities.Warning,
        Location = issue.Location,
        Explanation = issue.Explanation,
        SuggestedFix = issue.SuggestedFix,
        Source = issue.Source,
        Targets = issue.Targets,
    };

    /// <summary>
    /// Routes the issues to their owning steps (fixes 2, section 1) and fixes each step in place, in pipeline order:
    /// a patch of the items named, or a revised answer. Returns the issues whose step was fixed.
    /// </summary>
    private async Task<List<CoherenceIssue>> FixRoundAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, int caseAttempt, bool light, CancellationToken ct)
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

        var applied = new List<CoherenceIssue>();

        // An issue split over several steps is applied only if every part was: one failed part means it was not.
        var failed = new HashSet<CoherenceIssue>();
        var routed = CriticRouting.Route(issues).ToDictionary(r => r.Step, r => r.Issues, StringComparer.Ordinal);

        // A resident's record is the residents step's, not humans'.
        if (draft.Outputs.TryGetValue(ResidentsStep.StepName, out var kept) && kept is ResidentsOutput residents
            && routed.TryGetValue(HumansStep.StepName, out var people))
        {
            var theirs = residents.Residents.Select(c => $"cast:{c.CharacterId}").ToHashSet(StringComparer.Ordinal);
            var moved = people.Where(p => theirs.Contains(p.Part.Location)).ToList();
            if (moved.Count > 0)
            {
                people.RemoveAll(moved.Contains);
                routed[ResidentsStep.StepName] = moved;
                if (people.Count == 0)
                {
                    routed.Remove(HumansStep.StepName);
                }
            }
        }
        foreach (var step in CriticRouting.PipelineOrder)
        {
            if (!routed.TryGetValue(step, out var parts))
            {
                continue;
            }

            var before = draft.Packet;
            var result = await steps[step].FixAsync(draft, parts.Select(p => p.Part).ToList(), caseAttempt, light, ct);
            if (!result.Success)
            {
                failed.UnionWith(parts.Select(p => p.Original));
                logger.LogWarning("The fix for {Step} did not pass its checks and was not applied: {Error}", step, result.ErrorMessage);
                continue;
            }

            applied.AddRange(parts.Select(p => p.Original));

            // What this fix changed reaches the later steps whose items are built on it.
            foreach (var (later, follow) in CriticRouting.Dependents(before, draft.Packet))
            {
                if (!routed.TryGetValue(later, out var list))
                {
                    routed[later] = list = [];
                }

                list.Add((follow, follow));
            }
        }

        return applied.Where(i => i.Code != IssueCodes.Consistency && !failed.Contains(i)).Distinct().ToList();
    }

    /// <summary>
    /// The last round (fixes 2, section 2): the critic's remaining warnings with concrete fixes, applied with the light
    /// model, then the structural checks again. No third critic. If the case no longer passes, it goes back to how it was.
    /// </summary>
    private async Task ApplyWarningsAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> warnings, int caseAttempt, CancellationToken ct)
    {
        var before = Snapshot(draft);
        var applied = await FixRoundAsync(draft, warnings, caseAttempt, light: true, ct);
        if (applied.Count == 0)
        {
            return;
        }

        if (!StillPasses(draft, caseAttempt))
        {
            logger.LogInformation("Applying the critic's warnings broke the case's checks; keeping it as it was");
            Restore(draft, before);
            return;
        }

        foreach (var issue in draft.Packet.Validation.Issues.Where(i => i.Source == IssueSources.Critic))
        {
            issue.Applied = applied.Exists(a => a.Code == issue.Code && a.Explanation == issue.Explanation);
        }

        logger.LogInformation("Applied {Count} warnings in the last round: {Codes}", applied.Count, string.Join(", ", applied.Select(a => a.Code)));
    }

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
        "critic." + CriticChecks.Solves => 2,
        "critic." + CriticChecks.DeepLoreTrail => 0,
        _ when issue.Location.StartsWith("hook", StringComparison.Ordinal) => 4,
        _ when issue.Location == "creatureMinds" => 3,
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

        packet.Validation.Issues = structural.Concat(critic).ToList();
        packet.Validation.Passed = passed && !structural.Exists(i => i.IsError);
        packet.Validation.Runs++;
    }
}
