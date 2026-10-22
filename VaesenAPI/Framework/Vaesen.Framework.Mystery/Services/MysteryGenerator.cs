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

    /// <summary>The in-world month; null: <see cref="MysteryOptions.DefaultMonth"/>.</summary>
    public int? Month { get; init; }

    /// <summary>Fixes the deterministic choices; null: random.</summary>
    public int? Seed { get; init; }

    /// <summary>Force a level (CaseLab), instead of letting the assignment choose.</summary>
    public string? LevelId { get; init; }

    /// <summary>Force a creature (CaseLab), instead of letting the assignment choose.</summary>
    public string? VaesenId { get; init; }

    /// <summary>Store the case when it passes (off for dry runs).</summary>
    public bool Store { get; init; } = true;
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
/// Spec section 7: assignment (C#), then conflict, escalation, humans and clues (model, each validated before the next),
/// then the critic. A failing step re-rolls with its issues; then the whole case re-rolls; then generation fails
/// visibly, with every attempt in the log.
/// </summary>
public sealed class MysteryGenerator(
    IContentStore content,
    ICasePacketStore cases,
    IMysteryModel model,
    ICoherenceValidator validator,
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
            return ret.Ok(Assignment.Assign(pack.Result!, request.History, request.Month ?? Options.DefaultMonth, request.Seed ?? Random.Shared.Next(), Options));
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
        var month = request.Month ?? Options.DefaultMonth;
        var baseSeed = request.Seed ?? Random.Shared.Next();
        var log = new List<GenerationLogEntry>();
        StoredCase? last = null;
        string lastError = "";

        for (var caseAttempt = 0; caseAttempt <= Options.MaxCaseRetries; caseAttempt++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            AssignmentBrief brief;
            try
            {
                brief = Assignment.Assign(pack, request.History, month, unchecked(baseSeed + caseAttempt * 7919), Options);
            }
            catch (InvalidOperationException ex)
            {
                return ret.Fail(ex.Message, ErrorCodes.Invalid);
            }

            var draft = NewDraft(pack, request.History, brief);
            progress?.Report(new GenerationProgress(caseAttempt, "assignment",
                $"{draft.Vaesen.Name} at {draft.Level.Name}, focus {brief.Focus}{(brief.HiddenHand ? ", with a hidden hand" : "")}."));

            var outcome = await RunStepsAsync(draft, caseAttempt, progress, cancellationToken);
            log.AddRange(draft.Log);
            last = new StoredCase { Packet = draft.Packet, Brief = brief, Log = log.ToList() };

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
                }

                logger.LogInformation("Generated mystery {MysteryId} '{Title}' for {SocietyId} after {Attempts} case attempt(s)",
                    draft.Packet.MysteryId, draft.Packet.Title, draft.Packet.SocietyId, caseAttempt + 1);
                return ret.Ok(new GenerationResult { Case = last });
            }

            lastError = outcome.ErrorMessage;
            logger.LogWarning("Mystery case attempt {Attempt} failed: {Error}", caseAttempt + 1, outcome.ErrorMessage);
            if (outcome.ErrorCode is not (StepErrorCodes.StepFailed or StepErrorCodes.Reroll))
            {
                // A model or network failure, not a bad case: re-rolling will not help.
                break;
            }

            Finish(draft, passed: false);
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

        if (pack.Vaesen.Count == 0 || pack.Levels.Count == 0)
        {
            return ret.Fail(Options.UseDraftContent
                ? "No usable creature or level (check the requested ids)."
                : "No approved creature or level yet. Approve content with the ContentTool, or set Mystery:UseDraftContent.", ErrorCodes.Invalid);
        }

        return ret.Ok(pack);
    }

    private MysteryDraft NewDraft(ContentPack pack, SocietyHistory history, AssignmentBrief brief)
    {
        var level = pack.FindLevel(brief.LevelId)!;
        var vaesen = pack.FindVaesen(brief.VaesenId)!;
        var draft = new MysteryDraft
        {
            Brief = brief,
            Pack = pack,
            History = history,
            Level = level,
            Vaesen = vaesen,
            Conflicts = brief.ConflictArchetypeIds.Select(pack.FindConflict).OfType<ConflictArchetype>().ToList(),
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

    /// <summary>The steps in their fixed order, each on the stage the one before produced.</summary>
    private async Task<ResultBase> RunStepsAsync(MysteryDraft draft, int caseAttempt, IProgress<GenerationProgress>? progress, CancellationToken ct)
    {
        var ret = new ResultBase();
        var o = Options;

        progress?.Report(new GenerationProgress(caseAttempt, "conflict", "Building the conflict."));
        var conflict = await new ConflictStep(model, validator, o).RunAsync(new AssignedStage(draft), [], caseAttempt, ct);
        if (!conflict.Success)
        {
            return ret.Fail(conflict.ErrorMessage, conflict.ErrorCode);
        }

        progress?.Report(new GenerationProgress(caseAttempt, "escalation", "Writing what has happened."));
        var escalation = await new EscalationStep(model, validator, o).RunAsync(conflict.Result!, [], caseAttempt, ct);
        if (!escalation.Success)
        {
            return ret.Fail(escalation.ErrorMessage, escalation.ErrorCode);
        }

        progress?.Report(new GenerationProgress(caseAttempt, "humans", "Casting the people."));
        var cast = await new HumansStep(model, validator, o).RunAsync(escalation.Result!, [], caseAttempt, ct);
        if (!cast.Success)
        {
            return ret.Fail(cast.ErrorMessage, cast.ErrorCode);
        }

        // The clue step and the critic go round together: critic errors are fed back into another clue attempt.
        var cluesStep = new CluesStep(model, validator, o);
        IReadOnlyList<CoherenceIssue> feedback = [];
        for (var round = 0; round <= o.MaxStepRetries; round++)
        {
            progress?.Report(new GenerationProgress(caseAttempt, "clues", round == 0 ? "Placing the clues." : "Reworking the clues after review."));
            var clues = await cluesStep.RunAsync(cast.Result!, feedback, caseAttempt, ct);
            if (!clues.Success)
            {
                return ret.Fail(clues.ErrorMessage, clues.ErrorCode);
            }

            if (!o.UseCritic)
            {
                return ret.Ok();
            }

            progress?.Report(new GenerationProgress(caseAttempt, "critic", "Reviewing the case."));
            var review = await new CriticStep(model, o).RunAsync(draft, caseAttempt, ct);
            if (!review.Success)
            {
                return ret.Fail(review.ErrorMessage, review.ErrorCode);
            }

            draft.Packet.Validation.Issues.RemoveAll(i => i.Source == IssueSources.Critic);
            draft.Packet.Validation.Issues.AddRange(review.Result!);
            if (!review.Result!.Exists(i => i.IsError))
            {
                return ret.Ok();
            }

            feedback = review.Result!.Where(i => i.IsError).ToList();
        }

        return ret.Fail("The critic still found problems after every rework: " + string.Join(" ", feedback.Take(5)), StepErrorCodes.StepFailed);
    }

    /// <summary>Stamps the packet and replaces the accumulated issues with one final structural run plus the critic's.</summary>
    private void Finish(MysteryDraft draft, bool passed)
    {
        var packet = draft.Packet;
        packet.GeneratedAt = DateTime.UtcNow;
        var critic = packet.Validation.Issues.Where(i => i.Source == IssueSources.Critic).ToList();
        var structural = validator.Validate(packet, draft.ValidationContext);
        packet.Validation.Issues = structural.Concat(critic).ToList();
        packet.Validation.Passed = passed && !structural.Exists(i => i.IsError);
        packet.Validation.Runs++;
    }
}
