using System.Collections;
using System.Text;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>One stage of generation (spec 6.3): it takes the stage before it and produces the next.</summary>
public interface IMysteryGenerationStep<in TIn, TOut>
    where TIn : DraftStage
    where TOut : DraftStage
{
    string Name { get; }

    /// <param name="fresh">A restart after the step gave up: logged as such (fixes, section 2.3).</param>
    Task<ResultBase<TOut>> RunAsync(TIn input, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, CancellationToken cancellationToken, bool fresh = false);
}

public static class StepErrorCodes
{
    /// <summary>The step could not produce a valid result within its retries.</summary>
    public const string StepFailed = "step_failed";

    /// <summary>The model said nothing plausible fits; the whole case should be re-rolled.</summary>
    public const string Reroll = "reroll";

    /// <summary>The model said the focus does not suit this creature here; only the focus is re-rolled.</summary>
    public const string RerollFocus = "reroll_focus";
}

/// <summary>
/// A list of items in a step's output that a patch call can correct one by one, by id ("lever" items are found at
/// issue locations "lever:{id}").
/// </summary>
public sealed record PatchList(string Prefix, IList Items, Func<object, string> Id);

/// <summary>A step whose passed output can be corrected in place: the critic's fixes go to the step that owns them.</summary>
public interface IFixableStep
{
    string Name { get; }

    /// <summary>
    /// Corrects this step's passed output for <paramref name="issues"/>: a patch of the items they name, or a revised
    /// answer that changes only what they ask. Nothing downstream is regenerated. Fails, leaving the draft as it was,
    /// if the corrected output does not pass the step's checks.
    /// </summary>
    /// <param name="light">The cheap round for the critic's last warnings: the light model at low effort.</param>
    Task<ResultBase> FixAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, int caseAttempt, bool light, CancellationToken cancellationToken);
}

/// <summary>
/// A creative step: ask the model for <typeparamref name="TOutput"/>, repair near-miss values, fold it into a copy of
/// the packet, run the structural checks for what exists so far, and try again until clean or out of retries. When
/// the only errors are field-level ones on a few items, only those items are asked for again (a patch) and merged
/// back; anything structural gets a full retry with the issues fed back. A passed output is kept on the draft, so the
/// critic's fixes can later patch or revise it in place (<see cref="FixAsync"/>).
/// </summary>
public abstract class ModelStep<TIn, TOut, TOutput>(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : IMysteryGenerationStep<TIn, TOut>, IFixableStep
    where TIn : DraftStage
    where TOut : DraftStage
    where TOutput : class
{
    private enum Mode
    {
        Full,
        Fresh,
        Fix,
        FixLight,
    }

    public abstract string Name { get; }

    /// <summary>What the validator checks after this step.</summary>
    protected abstract CheckScope Scope { get; }

    protected abstract string Instruction(MysteryDraft draft);

    /// <summary>Folds the output into <paramref name="packet"/>; returns issues the output itself has (or a re-roll signal).</summary>
    protected abstract List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, TOutput output);

    protected abstract TOut Next(MysteryDraft draft);

    /// <summary>The model of the first try: <see cref="MysteryOptions.CreativeModel"/> by default.</summary>
    protected virtual string FirstTryModel(MysteryOptions o) => o.CreativeModel;

    /// <summary>The model of patches and fixes: <see cref="MysteryOptions.PatchModel"/> by default.</summary>
    protected virtual string FixModel(MysteryOptions o) => o.PatchModel;

    /// <summary>How hard the first try thinks (fixes, section 9): <see cref="MysteryOptions.StandardEffort"/> by default.</summary>
    protected virtual string FirstTryEffort(MysteryOptions o) => o.StandardEffort;

    /// <summary>The lists of items a patch can correct; none means every retry is a full one.</summary>
    protected virtual IReadOnlyList<PatchList> PatchLists(TOutput output) => [];

    public async Task<ResultBase<TOut>> RunAsync(TIn input, IReadOnlyList<CoherenceIssue> feedback, int caseAttempt, CancellationToken cancellationToken, bool fresh = false)
    {
        var ret = new ResultBase<TOut>();
        var result = await LoopAsync(input.Draft, feedback.ToList(), null, caseAttempt, fresh ? Mode.Fresh : Mode.Full, options.MaxStepRetries, cancellationToken);
        return result.Success ? ret.Ok(Next(input.Draft)) : ret.Fail(result.ErrorMessage, result.ErrorCode);
    }

    public async Task<ResultBase> FixAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, int caseAttempt, bool light, CancellationToken cancellationToken)
    {
        if (!draft.Outputs.TryGetValue(Name, out var kept) || kept is not TOutput passed)
        {
            return new ResultBase().Ok();
        }

        // Work on a copy: a fix that does not pass leaves the passed output untouched.
        var copy = Newtonsoft.Json.JsonConvert.DeserializeObject<TOutput>(Newtonsoft.Json.JsonConvert.SerializeObject(passed))!;
        return await LoopAsync(draft, issues.ToList(), copy, caseAttempt, light ? Mode.FixLight : Mode.Fix, 1, cancellationToken);
    }

    private async Task<ResultBase> LoopAsync(
        MysteryDraft draft, List<CoherenceIssue> issues, TOutput? last, int caseAttempt, Mode mode, int retries, CancellationToken cancellationToken)
    {
        var ret = new ResultBase();
        var fixing = mode is Mode.Fix or Mode.FixLight;
        var previous = last is null ? null : Newtonsoft.Json.JsonConvert.SerializeObject(last);

        // A fix keeps every id it was given: a renamed person keeps their id, or every channel naming them breaks.
        var keptIds = fixing && last is not null ? Patch.AllIds(PatchLists(last)) : null;
        // A fix names its items, so it is always a patch when it can be: answering a few dozen items beats rewriting all.
        var patchErrors = last is not null && Patch.Patchable(PatchLists(last), issues, fixing ? 1.0 : options.PatchShare, anyCode: fixing) ? issues : [];

        // The last round never rewrites a whole step: what cannot be a patch is left as it is, and stays a warning.
        if (mode == Mode.FixLight && patchErrors.Count == 0 && last is not null && PatchLists(last).Count > 0)
        {
            return ret.Fail($"Not a patch of {Name}'s items; left for the game master.", StepErrorCodes.StepFailed);
        }

        for (var attempt = 0; attempt <= retries; attempt++)
        {
            var patching = last is not null && patchErrors.Count > 0;
            var entry = new GenerationLogEntry
            {
                CaseAttempt = caseAttempt,
                Step = Name,
                Attempt = attempt,
                Kind = patching ? LogEntryKinds.Patch : fixing ? LogEntryKinds.Fix : mode == Mode.Fresh && attempt == 0 ? LogEntryKinds.Fresh : LogEntryKinds.Full,
            };
            draft.Log.Add(entry);

            var view = CaseView.For(Name, draft.Packet);
            entry.Instruction = patching
                ? PromptBuilder.WithCase(Patch.Instruction(Name, Patch.Items(PatchLists(last!), patchErrors), patchErrors, Patch.OtherIds(PatchLists(last!), patchErrors)), view)
                : fixing && attempt == 0
                    ? PromptBuilder.WithCaseAndFeedback(PromptBuilder.Revise(Name), view, issues, previous)
                    : PromptBuilder.WithCaseAndFeedback(Instruction(draft), view, issues, previous);
            var (modelName, effort) = (patching, fixing, attempt) switch
            {
                // Judgement fixes go to the patch model as patches, never to the small model (review 2, item 2).
                (_, true, _) when mode == Mode.FixLight => (Or(FixModel(options), options.CreativeModel), options.PatchEffort),
                (true, _, _) or (_, true, _) => (Or(FixModel(options), options.CreativeModel), options.PatchEffort),
                (_, _, 0) => (Or(FirstTryModel(options), options.CreativeModel), FirstTryEffort(options)),
                _ => (Or(options.RetryModel, options.CreativeModel), options.LightEffort),
            };
            var reply = await model.GenerateAsync<TOutput>(new ModelRequest
            {
                Step = Name,
                Model = modelName,
                Effort = effort,
                MaxTokens = patching ? options.PatchMaxOutputTokens : options.MaxOutputTokens,
                System = draft.SystemPrompt,
                Context = draft.ContextFor(Name),
                Instruction = entry.Instruction,

                // Only a repeat of this step's schema can read the cache, so a first call does not pay to write it.
                Cache = attempt > 0,
                Ids = CaseIds.For(draft, Name),
            }, cancellationToken);

            if (!reply.Success)
            {
                entry.Error = reply.ErrorMessage;
                if (reply.ErrorCode is ModelErrorCodes.BadOutput or ModelErrorCodes.Truncated)
                {
                    issues = [CoherenceIssue.Error(IssueCodes.Schema, Name, reply.ErrorMessage, "Answer with complete JSON that fits the schema; be more concise if needed.")];
                    patchErrors = [];
                    continue;
                }

                return ret.Fail(reply.ErrorMessage, reply.ErrorCode);
            }

            var answer = reply.Result!;
            entry.Model = answer.Model;
            entry.InputTokens = answer.InputTokens;
            entry.OutputTokens = answer.OutputTokens;
            entry.CacheReadTokens = answer.CacheReadTokens;
            entry.CacheWriteTokens = answer.CacheWriteTokens;
            entry.LatencyMs = answer.LatencyMs;
            entry.PrefixHash = answer.PrefixHash;
            entry.CostUsd = ModelPrices.Cost(answer.Model, answer.InputTokens, answer.CacheWriteTokens, answer.CacheReadTokens, answer.OutputTokens);
            entry.Output = answer.RawJson;

            // A patch answers only the corrected items: merge them into the last full answer.
            var output = answer.Value;
            if (patching)
            {
                // New items are welcome (a decoy thread a fact points to, a channel asked for), but not new people:
                // a person who comes back from a patch with another id is the same person renamed.
                var asked = patchErrors.Select(e => e.Location).Where(l => l.StartsWith(Patch.NewItem, StringComparison.Ordinal)).ToHashSet(StringComparer.Ordinal);
                Patch.Merge(PatchLists(last!), PatchLists(output), allowNew: prefix => prefix != "cast" || asked.Contains(Patch.NewItem + prefix));
                output = last!;
            }

            var repairs = EnumRepair.Normalize(output);
            var packet = draft.ClonePacket();
            var applyIssues = Apply(draft, packet, output);
            if (applyIssues.Find(i => i.Code is StepErrorCodes.Reroll or StepErrorCodes.RerollFocus) is { } reroll)
            {
                entry.Issues = applyIssues;
                return fixing ? ret.Fail(reroll.Explanation, StepErrorCodes.StepFailed) : ret.Fail(reroll.Explanation, reroll.Code);
            }

            var context = draft.ValidationContext;
            var checkContext = new ValidationContext
            {
                Pack = context.Pack,
                Level = context.Level,
                Vaesen = context.Vaesen,
                Conflict = draft.Pack.FindConflict(packet.Brief.ConflictArchetypeId),
                UnavailableCharacterIds = context.UnavailableCharacterIds,
                SocietyCharacterIds = context.SocietyCharacterIds,
                WorldNames = context.WorldNames,
                Holders = context.Holders,
                SettledPublicFacts = context.SettledPublicFacts,
                ParentChild = context.ParentChild,
                MaxLeverFacts = context.MaxLeverFacts,
            };
            var found = repairs.Concat(applyIssues).Concat(validator.Validate(packet, checkContext, Scope)).ToList();
            if (keptIds is not null)
            {
                var now = Patch.AllIds(PatchLists(output));
                found.AddRange(keptIds.Where(id => !now.Contains(id)).Select(id => CoherenceIssue.Error(IssueCodes.Schema, id,
                    $"A fix keeps every item: {id} is gone. Change what the problems ask, and keep every id (rename a person by their name, never their id).")));
            }
            entry.Issues = found;
            packet.Validation.Runs++;

            if (!found.Exists(i => i.IsError))
            {
                entry.Passed = true;
                packet.Validation.Issues.AddRange(found);
                var before = draft.Packet;
                draft.Packet = packet;
                draft.Completed |= Scope;
                draft.Outputs[Name] = output;
                OnPassed(draft, before, caseAttempt);
                return ret.Ok();
            }

            issues = found.Where(i => i.IsError).ToList();
            last = output;
            previous = Newtonsoft.Json.JsonConvert.SerializeObject(output);
            patchErrors = Patch.Patchable(PatchLists(output), issues, options.PatchShare) ? issues : [];
        }

        return ret.Fail($"The {Name} step still had problems after {retries + 1} attempts: "
            + string.Join(" ", issues.Take(5).Select(i => i.ToString())), StepErrorCodes.StepFailed);
    }

    /// <summary>After a passed answer is on the draft: <paramref name="before"/> is the packet as it was.</summary>
    protected virtual void OnPassed(MysteryDraft draft, CasePacket before, int caseAttempt)
    {
    }

    private static string Or(string value, string fallback) => string.IsNullOrWhiteSpace(value) ? fallback : value;
}

/// <summary>Step 3: the conflict skeleton.</summary>
public sealed class ConflictStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<AssignedStage, ConflictStage, ConflictOutput>(model, validator, options)
{
    public const string StepName = "conflict";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Conflict;

    protected override string FirstTryEffort(MysteryOptions o) => o.CreativeEffort;

    protected override string Instruction(MysteryDraft draft)
    {
        var origin = draft.Vaesen.OriginSet.Find(o => o.Id == draft.Brief.OriginId);
        return PromptBuilder.Fill(PromptBuilder.Template("conflict"), new Dictionary<string, string>
        {
            ["originLine"] = origin is null ? "" : $", and the origin of the affliction ({origin.Description})",
            ["handLine"] = PromptBuilder.HandLineForConflict(draft.Brief),
        });
    }

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, ConflictOutput output)
    {
        if (output.Impossible)
        {
            return [CoherenceIssue.Error(StepErrorCodes.Reroll, "conflict", output.ImpossibleReason ?? "Nothing plausible fits.")];
        }

        if (output.FocusUnsuitable)
        {
            return [CoherenceIssue.Error(StepErrorCodes.RerollFocus, "truth.focus", output.FocusUnsuitableReason ?? $"The focus {draft.Brief.Focus} does not suit this creature here.")];
        }

        packet.Title = output.Title;
        packet.Brief.ConflictArchetypeId = output.ConflictArchetypeId;
        var truth = packet.Truth;
        truth.Summary = output.TruthSummary;
        truth.Provocation = output.Provocation;
        truth.ProvocationPlaceId = output.ProvocationPlaceId;
        truth.ProvocationAffordanceTag = output.ProvocationAffordanceTag;
        truth.HiddenHandMotive = draft.Brief.HiddenHand ? output.HiddenHandMotive : null;
        truth.DecoyReading = output.DecoyReading;
        var publicFacts = draft.Level.PublicFactsWithIds.ToDictionary(f => f.Id, f => f.Text, StringComparer.Ordinal);
        packet.PublicFactsExplained = output.PublicFactsExplained
            .Where(e => !string.IsNullOrWhiteSpace(e.Explanation))
            .Select(e => new PublicFactExplanation { PublicFactId = e.PublicFactId, Fact = publicFacts.GetValueOrDefault(e.PublicFactId, ""), Explanation = e.Explanation })
            .ToList();

        return draft.Brief.ConflictArchetypeIds.Contains(output.ConflictArchetypeId)
            ? []
            : [CoherenceIssue.Error(IssueCodes.UnknownReference, "conflictArchetypeId",
                $"'{output.ConflictArchetypeId}' is not one of the offered archetypes ({string.Join(", ", draft.Brief.ConflictArchetypeIds)}).")];
    }

    protected override ConflictStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Step 4: what has concretely happened.</summary>
public sealed class EscalationStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<ConflictStage, EscalationStage, EscalationOutput>(model, validator, options)
{
    public const string StepName = "escalation";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Escalation;

    protected override IReadOnlyList<PatchList> PatchLists(EscalationOutput output) => [new("event", output.Events, e => ((EscalationDraft)e).Id)];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("escalation"), new Dictionary<string, string>
        {
            ["openingRule"] = PromptBuilder.OpeningRule(draft.Brief),
            ["giveawayRule"] = PromptBuilder.GiveawayRuleForEscalation(draft.Brief),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, EscalationOutput output)
    {
        // A fix after the facts step keeps which facts each event is.
        var factIds = packet.Truth.Escalation.ToDictionary(e => e.Id, e => e.FactIds, StringComparer.Ordinal);
        packet.Truth.Escalation = output.Events.Select(e => new EscalationEvent
        {
            Id = e.Id,
            Description = e.Description,
            When = e.When,
            PlaceId = string.IsNullOrWhiteSpace(e.PlaceId) ? null : e.PlaceId,
            Register = e.Register,
            Opening = e.Opening,
            FactIds = factIds.TryGetValue(e.Id, out var ids) ? ids : [],
        }).ToList();
        return [];
    }

    protected override EscalationStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Step 5: the people.</summary>
public sealed class HumansStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<EscalationStage, CastStage, HumansOutput>(model, validator, options)
{
    public const string StepName = "humans";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Cast;

    protected override IReadOnlyList<PatchList> PatchLists(HumansOutput output) => [new("cast", output.Cast, c => ((CastDraft)c).CharacterId)];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("humans"), new Dictionary<string, string>
        {
            ["handLine"] = PromptBuilder.HandLineForHumans(draft.Brief),
            ["hookGiverType"] = draft.Pack.Setting.FindHookGiverType(draft.Brief.HookGiverType) is { } giver ? $"{giver.Id}: {giver.Meaning}" : draft.Brief.HookGiverType,
            ["usedNames"] = PromptBuilder.UsedNames(draft.World, draft.Level.Id),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, HumansOutput output)
    {
        // A fix after minds and channels keeps what everyone knows, thinks and lies about.
        // The residents step's people stay when the case's people are revised.
        var residents = draft.Outputs.TryGetValue(ResidentsStep.StepName, out var kept) && kept is ResidentsOutput r
            ? r.Residents.Select(c => c.CharacterId).ToHashSet(StringComparer.Ordinal)
            : [];

        // New people get ids from their role, not their name (review 3, item 5), before anything refers to them.
        var rekeyed = PersonIds.Rekey(output.Cast, packet, draft.ValidationContext.SocietyCharacterIds);
        if (output.HiddenHandCharacterId is { } hand && rekeyed.TryGetValue(hand, out var handId))
        {
            output.HiddenHandCharacterId = handId;
        }

        packet.Cast = ToCast(draft, packet, output.Cast)
            .Concat(packet.Cast.Where(c => residents.Contains(c.CharacterId) && !output.Cast.Exists(o => o.CharacterId == c.CharacterId)))
            .ToList();
        packet.Truth.PlotCentre.CharacterId = draft.Brief.HiddenHand ? output.HiddenHandCharacterId : null;
        packet.OffstagePeople = output.OffstagePeople;
        AddDwellings(draft, packet, output.Cast);
        return [];
    }

    /// <summary>Cast drafts as cast members, keeping what minds and channels already gave anyone who was there before.</summary>
    internal static List<CastMember> ToCast(MysteryDraft draft, CasePacket packet, List<CastDraft> drafts)
    {
        var minds = packet.Cast.ToDictionary(c => c.CharacterId, StringComparer.Ordinal);
        return drafts.Select(c => new CastMember
        {
            CharacterId = c.CharacterId,
            Name = c.Name,
            Handle = string.IsNullOrWhiteSpace(c.Handle) ? null : c.Handle,
            InstanceRef = c.InstanceRef,
            PlotRole = c.PlotRole,
            Importance = c.Importance,
            RoleSlotId = c.RoleSlotId,
            Identity = c.Identity ?? (c.InstanceRef is InstanceRefs.Persistent or InstanceRefs.Promoted
                ? draft.World?.Find(c.CharacterId)?.Identity ?? draft.Pack.FindCharacter(c.CharacterId)?.Identity
                : null),
            HomePlaceId = string.IsNullOrWhiteSpace(c.HomePlaceId) ? null : c.HomePlaceId,
            HomeAffordanceId = string.IsNullOrWhiteSpace(c.HomeAffordanceId) ? null : c.HomeAffordanceId,
            Keeps = string.IsNullOrWhiteSpace(c.Keeps) ? null : c.Keeps,
            Wants = c.Wants,
            Secrets = c.Secrets,
            Routine = c.Routine,
            VoiceNotes = c.VoiceNotes,
            Relationships = c.Relationships,
            Knows = minds.TryGetValue(c.CharacterId, out var mind) ? mind.Knows : [],
            Thinks = mind?.Thinks ?? [],
            Lies = mind?.Lies ?? [],
        }).ToList();
    }

    /// <summary>
    /// Builds a home for each significant person who asked for one (fixes 3, section 6), in their home place of the
    /// draft's own copy of the level, and records it on the packet so the runtime builds it too.
    /// </summary>
    internal static void AddDwellings(MysteryDraft draft, CasePacket packet, List<CastDraft> cast)
    {
        foreach (var asked in cast.Where(c => c.NewDwelling && c.Importance == CastImportance.Significant))
        {
            var member = packet.FindCast(asked.CharacterId);
            if (member?.HomePlaceId is null || draft.Level.FindPlace(member.HomePlaceId) is not { } place)
            {
                continue;
            }

            var home = Dwellings.For(place.Id, member.CharacterId, member.Name);
            if (place.FindAffordance(home.Id) is null)
            {
                place.Affordances.Add(home);
            }

            packet.Level.AddedAffordances.RemoveAll(a => a.Affordance.Id == home.Id);
            packet.Level.AddedAffordances.Add(new AddedAffordance { PlaceId = place.Id, Affordance = home });
            member.HomeAffordanceId = home.Id;
        }
    }

    /// <summary>
    /// A person renamed after names existed (a fix, a name clash) is renamed everywhere: in every other step's text and
    /// in the outputs later fixes patch (fixes 4, section 4). Otherwise the case calls one person by two names.
    /// </summary>
    protected override void OnPassed(MysteryDraft draft, CasePacket before, int caseAttempt) => Renames.AfterPass(draft, before, caseAttempt, Name);

    protected override CastStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>
/// The place's ordinary residents, after the case's people (fixes 4, section 10): short records on the small model at
/// low effort, so the expensive humans call writes only the five to seven people the case turns on.
/// </summary>
public sealed class ResidentsStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<CastStage, CastStage, ResidentsOutput>(model, validator, options)
{
    public const string StepName = "residents";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Cast;

    protected override string FirstTryModel(MysteryOptions o) => o.LightModel;

    protected override string FirstTryEffort(MysteryOptions o) => o.LightEffort;

    /// <summary>A resident's record is short: its patches stay on the small model too (review 3, item 6).</summary>
    protected override string FixModel(MysteryOptions o) => o.LightModel;

    protected override IReadOnlyList<PatchList> PatchLists(ResidentsOutput output) => [new("cast", output.Residents, c => ((CastDraft)c).CharacterId)];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("residents"), new Dictionary<string, string>
        {
            ["names"] = string.Join(", ", draft.Packet.Cast.Where(c => c.Importance == CastImportance.Significant).Select(c => c.Name)),
            ["usedNames"] = PromptBuilder.UsedNames(draft.World, draft.Level.Id),
        });

    /// <summary>
    /// Residents from the place's people (fixes 4, section 12.1): up to <paramref name="count"/> of the level's roster not
    /// already in the case, those with a thread to pull first. False when the Society has no roster for the level.
    /// </summary>
    public static bool FromRoster(MysteryDraft draft, int count)
    {
        if (draft.World?.RosterFor(draft.Level.Id) is null)
        {
            return false;
        }

        var taken = draft.Packet.Cast.Select(c => c.CharacterId).ToHashSet(StringComparer.Ordinal);
        var chosen = draft.World.PeopleOf(draft.Level.Id)
            .Where(c => c.Status == CharacterStatuses.Alive && !taken.Contains(c.Id))
            .OrderByDescending(c => c.LatentHooks.Count > 0)
            .ThenBy(c => RosterRoles.Holds(c.RosterRole ?? "") ? 1 : 0)
            .ThenBy(c => c.Id, StringComparer.Ordinal)
            .Take(count)
            .Select(c => new CastDraft
            {
                CharacterId = c.Id,
                Name = c.Name,
                InstanceRef = InstanceRefs.Persistent,
                PlotRole = $"{c.Identity.Occupation}: {c.Background}",
                Importance = CastImportance.Resident,
                Identity = c.Identity,
                HomePlaceId = c.HomePlaceId,
                Wants = new CastWants { Drives = c.Drives.Select(d => d.Text).ToList(), Fears = c.Fears.ToList() },
                Secrets = c.LatentHooks.Select(h => h.Text).ToList(),
                Relationships = c.Relationships.Select(r => new CastRelationship { CharacterId = r.CharacterId, Nature = r.Nature, Sentiment = r.Sentiment }).ToList(),
                Routine = c.Background,
                VoiceNotes = c.Voice.Register,
            })
            .ToList();
        var output = new ResidentsOutput { Residents = chosen };
        AddResidents(draft, draft.Packet, output);
        draft.Outputs[StepName] = output;
        draft.Log.Add(new GenerationLogEntry
        {
            CaseAttempt = draft.Log.LastOrDefault()?.CaseAttempt ?? 0,
            Step = StepName,
            Kind = LogEntryKinds.Roster,
            Passed = true,
            Instruction = $"No model call: up to {count} residents from the Society's people of {draft.Level.Name}.",
            Output = string.Join(", ", chosen.Select(c => c.Name)),
        });
        return true;
    }

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, ResidentsOutput output)
    {
        AddResidents(draft, packet, output);
        return [];
    }

    private static void AddResidents(MysteryDraft draft, CasePacket packet, ResidentsOutput output)
    {
        PersonIds.Rekey(output.Residents, packet, draft.ValidationContext.SocietyCharacterIds);
        var previous = draft.Outputs.TryGetValue(StepName, out var kept) && kept is ResidentsOutput r
            ? r.Residents.Select(c => c.CharacterId).ToHashSet(StringComparer.Ordinal)
            : [];
        foreach (var resident in output.Residents)
        {
            resident.Importance = CastImportance.Resident;
        }

        var residents = HumansStep.ToCast(draft, packet, output.Residents);
        packet.Cast = packet.Cast.Where(c => !previous.Contains(c.CharacterId) && !residents.Exists(n => n.CharacterId == c.CharacterId)).Concat(residents).ToList();
    }

    protected override void OnPassed(MysteryDraft draft, CasePacket before, int caseAttempt) => Renames.AfterPass(draft, before, caseAttempt, Name);

    protected override CastStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Sub-step 6a: every fact, the decoy threads, and which facts each event is. deepLore and giveaway are derived.</summary>
public sealed class FactsStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<CastStage, FactsStage, FactsOutput>(model, validator, options)
{
    public const string StepName = "facts";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Facts;

    protected override string FirstTryEffort(MysteryOptions o) => o.CreativeEffort;

    protected override IReadOnlyList<PatchList> PatchLists(FactsOutput output) =>
    [
        new("fact", output.Facts, f => ((FactDraft)f).Id),
        new("decoy", output.Decoys, d => ((DecoyThread)d).ThreadId),
    ];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("facts"), new Dictionary<string, string>
        {
            ["redHerringDensity"] = draft.Brief.RedHerringDensity,
            ["decoyRule"] = PromptBuilder.DecoyRule(draft.Brief.RedHerringDensity),
            ["seasonRule"] = PromptBuilder.SeasonRule(draft.Brief),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, FactsOutput output)
    {
        var issues = new List<CoherenceIssue>();
        packet.Facts = output.Facts.Select(f => new CaseFact
        {
            Id = f.Id,
            Statement = f.Statement,
            LoadBearing = f.LoadBearing,
            Kind = f.Kind,
            PointsTo = f.PointsTo,
            LoreIds = f.LoreIds,
        }).ToList();
        Derived.LoreFlags(packet.Facts, draft.Vaesen);
        Derived.LoadBearing(packet);

        foreach (var e in packet.Truth.Escalation)
        {
            e.FactIds = [];
        }

        foreach (var mapping in output.EventFacts)
        {
            var e = packet.Truth.Escalation.Find(x => x.Id == mapping.EventId);
            if (e is null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, "eventFacts", $"Unknown escalation event '{mapping.EventId}'."));
                continue;
            }

            e.FactIds = mapping.FactIds;
        }

        packet.Decoys = output.Decoys;
        packet.SeasonThreads = output.SeasonThreads;
        return issues;
    }

    protected override FactsStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Sub-step 6b: what every mind holds, and every channel into every fact. Checked against each other.</summary>
public sealed class MindsStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<FactsStage, MindsStage, MindsOutput>(model, validator, options)
{
    public const string StepName = "minds";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Minds;

    protected override IReadOnlyList<PatchList> PatchLists(MindsOutput output) =>
    [
        new("cast", output.Minds, m => ((CastMind)m).CharacterId),
        new("channel", output.Channels, c => ((CaseChannel)c).Id),
    ];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("minds"), new Dictionary<string, string>
        {
            ["giveawayRule"] = PromptBuilder.GiveawayRuleForChannels(draft.Brief),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, MindsOutput output)
    {
        var issues = new List<CoherenceIssue>();
        foreach (var member in packet.Cast)
        {
            member.Knows = [];
            member.Thinks = [];
            member.Lies = [];
        }

        foreach (var mind in output.Minds)
        {
            var member = packet.FindCast(mind.CharacterId);
            if (member is null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, "minds", $"Mind for '{mind.CharacterId}', who is not in the cast."));
                continue;
            }

            member.Knows = mind.Knows;
            member.Thinks = mind.Thinks;
            member.Lies = mind.Lies;
        }

        packet.Channels = output.Channels;
        return issues;
    }

    protected override MindsStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Sub-step 6c: beats, the climax and the resolution levers. The engine sets the tension weights.</summary>
public sealed class ArcStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<MindsStage, ArcStage, ArcOutput>(model, validator, options)
{
    public const string StepName = "arc";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Arc;

    protected override IReadOnlyList<PatchList> PatchLists(ArcOutput output) =>
    [
        new("beat", output.Beats, b => ((CaseBeat)b).Id),
        new("lever", output.ResolutionLevers, l => ((ResolutionLever)l).Id),
    ];

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("arc"), new Dictionary<string, string>
        {
            ["climaxRule"] = PromptBuilder.ClimaxRule(draft.Vaesen),
            ["leverRule"] = PromptBuilder.LeverRule(draft.Brief, draft.Vaesen),
            ["mediumFrom"] = DesperationTiers.MediumFrom.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture),
            ["highFrom"] = DesperationTiers.HighFrom.ToString("0.00", System.Globalization.CultureInfo.InvariantCulture),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, ArcOutput output)
    {
        packet.Beats = output.Beats;
        packet.Climax = output.Climax;
        packet.ResolutionLevers = output.ResolutionLevers;
        Derived.LoadBearing(packet);
        // The weights depend on the hook and the channels too: they are set before each whole-case check.
        packet.Tension = new CaseTension();
        return [];
    }

    protected override ArcStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>
/// The creature's mind (fixes 3, section 10), for a creature that can be bargained with: what it wants, its terms in
/// the deep lever's deeds, its red lines, and how it answers each approach.
/// </summary>
public sealed class CreatureStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<ArcStage, ArcStage, CreatureOutput>(model, validator, options)
{
    public const string StepName = "creature";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Creature;

    // One creature per case: its mind is one item a patch can answer (review 2, item 2), never the whole step again.
    protected override IReadOnlyList<PatchList> PatchLists(CreatureOutput output) => [new("creatureMinds", output.CreatureMinds, _ => CriticRouting.CreatureItem)];

    /// <summary>Only a creature the players can bargain with gets a mind.</summary>
    public static bool Needed(MysteryDraft draft) => draft.Vaesen.ResolutionShape == ResolutionShapes.Negotiable;

    protected override string Instruction(MysteryDraft draft) =>
        PromptBuilder.Fill(PromptBuilder.Template("creature"), new Dictionary<string, string>
        {
            ["creatureMindRule"] = PromptBuilder.CreatureMindRule(draft.Vaesen),
        });

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, CreatureOutput output)
    {
        packet.CreatureMinds = output.CreatureMinds;
        return [];
    }

    protected override ArcStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>Sub-step 6d: the hook and the scenes. After it, the whole case is checked.</summary>
public sealed class HookStep(IMysteryModel model, ICoherenceValidator validator, MysteryOptions options)
    : ModelStep<ArcStage, CluesStage, HookOutput>(model, validator, options)
{
    public const string StepName = "hook";

    public override string Name => StepName;

    protected override CheckScope Scope => CheckScope.Hook;

    protected override string FirstTryEffort(MysteryOptions o) => o.LightEffort;

    protected override string Instruction(MysteryDraft draft)
    {
        var hook = draft.Pack.Setting.FindHookChannel(draft.Brief.HookChannelId);
        return PromptBuilder.Fill(PromptBuilder.Template("hook"), new Dictionary<string, string>
        {
            ["hookChannel"] = hook is null ? draft.Brief.HookChannelId : $"{hook.Name.ToLowerInvariant()} ({hook.Description})",
            ["giverRule"] = draft.Pack.Setting.FindHookGiverType(draft.Brief.HookGiverType) is { } giver ? giver.Meaning : "a cast member with a reason to call for help",
            ["giveawayRule"] = PromptBuilder.GiveawayRuleForHook(draft.Brief),
        });
    }

    protected override List<CoherenceIssue> Apply(MysteryDraft draft, CasePacket packet, HookOutput output)
    {
        packet.Hook = new CaseHook
        {
            Channel = draft.Brief.HookChannelId,
            HookGiverId = output.Hook.HookGiverId,
            Text = output.Hook.Text,
            AssertedFactIds = output.Hook.AssertedFactIds,
            OpeningEventIds = output.Hook.OpeningEventIds,
        };
        // The writer of the hook knows what it states (review 3, item 3): a fact the minds step did not give them is
        // something they heard, so the runtime does not have them deny their own letter. Hearsay never counts as a way in.
        if (packet.FindCast(packet.Hook.HookGiverId) is { } giver)
        {
            foreach (var factId in packet.Hook.AssertedFactIds.Where(f => packet.FindFact(f) is not null && !giver.Knows.Exists(k => k.FactId == f)).Distinct())
            {
                giver.Knows.Add(new CastKnowledge { FactId = factId, Source = KnowledgeSources.BystanderPrefix + "parish-talk", When = "before sending for the Society" });
            }
        }

        packet.Level.PlacesUsed = output.PlacesUsed;
        packet.Level.SceneDescriptions = output.SceneDescriptions
            .GroupBy(s => s.PlaceId)
            .ToDictionary(g => g.Key, g => g.First().Text);
        return [];
    }

    protected override CluesStage Next(MysteryDraft draft) => new(draft);
}

/// <summary>The model critic (spec 7.10, judgement checks): turns failed checks into issues.</summary>
public sealed class CriticStep(IMysteryModel model, MysteryOptions options)
{
    public const string Name = "critic";

    /// <param name="earlier">The issues of the critic's last look, when the case was sent back for them.</param>
    public async Task<ResultBase<List<CoherenceIssue>>> RunAsync(MysteryDraft draft, int caseAttempt, CancellationToken cancellationToken, IReadOnlyList<CoherenceIssue>? earlier = null)
    {
        var ret = new ResultBase<List<CoherenceIssue>>();
        var entry = new GenerationLogEntry { CaseAttempt = caseAttempt, Step = Name };
        draft.Log.Add(entry);

        var instruction = PromptBuilder.Fill(PromptBuilder.Template("critic"), new Dictionary<string, string>
        {
            ["focus"] = $"{draft.Brief.Focus}: {PromptBuilder.FocusMeaning(draft.Brief.Focus)}",
            ["focusRule"] = PromptBuilder.FocusRuleForCritic(draft.Brief.Focus),
            ["solveRule"] = PromptBuilder.SolveRuleForCritic(draft.Brief),
            ["usedNames"] = PromptBuilder.UsedNames(draft.World, draft.Level.Id) is { Length: > 0 } used ? " " + used.TrimStart('-', ' ') : "",
        });

        entry.Instruction = PromptBuilder.WithCaseAndFeedback(instruction, draft.Packet, [], null) + Earlier(earlier);
        var reply = await model.GenerateAsync<CriticOutput>(new ModelRequest
        {
            Step = Name,
            Model = options.CriticModel,

            // The second look checks the fixes against its own list: it does not need to think as hard as the first.
            Effort = earlier is null ? options.CriticEffort : options.StandardEffort,
            MaxTokens = Math.Min(options.MaxOutputTokens, 32000),
            System = draft.SystemPrompt,
            Context = draft.ContextFor(Name),
            Instruction = entry.Instruction,

            // The critic usually looks twice with the same schema and context: the second look reads the cache.
            Cache = true,
        }, cancellationToken);

        if (!reply.Success)
        {
            entry.Error = reply.ErrorMessage;
            return ret.Fail(reply.ErrorMessage, reply.ErrorCode);
        }

        var answer = reply.Result!;
        entry.Model = answer.Model;
        entry.InputTokens = answer.InputTokens;
        entry.OutputTokens = answer.OutputTokens;
        entry.CacheReadTokens = answer.CacheReadTokens;
        entry.CacheWriteTokens = answer.CacheWriteTokens;
        entry.LatencyMs = answer.LatencyMs;
        entry.PrefixHash = answer.PrefixHash;
        entry.CostUsd = ModelPrices.Cost(answer.Model, answer.InputTokens, answer.CacheWriteTokens, answer.CacheReadTokens, answer.OutputTokens);
        entry.Output = answer.RawJson;

        var issues = answer.Value.Checks
            .Where(c => !c.Passed)
            .Select(c => new CoherenceIssue
            {
                Code = "critic." + c.Check,
                Severity = c.Severity == IssueSeverities.Warning ? IssueSeverities.Warning : IssueSeverities.Error,
                Location = c.Location,
                Explanation = c.Explanation,
                SuggestedFix = c.SuggestedFix,
                Source = IssueSources.Critic,
                Targets = c.Targets.Where(t => !string.IsNullOrWhiteSpace(t)).Select(t => t.Trim()).ToList(),
                Replacements = c.Replacements.Where(r => r.From.Length > 0 && r.From != r.To).ToList() is { Count: > 0 } replacements ? replacements : null,
                Everywhere = c.Everywhere,
                SetFields = c.SetFields.Where(f => f.Path.Length > 0).ToList() is { Count: > 0 } fields ? fields : null,
                Patches = c.Patches.Where(p => p.Target.Length > 0).ToList() is { Count: > 0 } patches ? patches : null,

                Blocking = BlockingOf(c),
                ImpactReason = string.IsNullOrWhiteSpace(c.ImpactReason) || c.Impact == CriticImpacts.Default ? null : c.ImpactReason,
            })
            .ToList();

        foreach (var missing in CriticChecks.All.Where(name => !answer.Value.Checks.Exists(c => c.Check == name)))
        {
            issues.Add(new CoherenceIssue
            {
                Code = "critic." + missing,
                Severity = IssueSeverities.Warning,
                Location = "critic",
                Explanation = "The critic did not answer this check.",
                Source = IssueSources.Critic,
            });
        }

        entry.Issues = issues;

        // A critic run passes only when it raised nothing blocking (fixes 5, section 5).
        entry.Passed = !issues.Exists(i => i.IsError || i.Blocking);
        draft.Packet.Validation.Runs++;
        return ret.Ok(issues);
    }

    /// <summary>
    /// Whether a failed check blocks: its check's default (fixes 5, section 5), unless the critic moved it and said why.
    /// A move without a reason is the field filled because every field must be, not a judgement (review of case 09).
    /// </summary>
    public static bool BlockingOf(CriticCheck check) =>
        (string.IsNullOrWhiteSpace(check.ImpactReason) ? CriticImpacts.Default : check.Impact) switch
        {
            CriticImpacts.Blocking => true,
            CriticImpacts.Advisory => false,
            _ => CriticChecks.BlockingByDefault.Contains(check.Check),
        };

    /// <summary>
    /// The last look's issues, for the second look: without them it reviews from scratch and asks for a new detail on
    /// a clue it already had fixed, and the same check comes back reworded.
    /// </summary>
    private static string Earlier(IReadOnlyList<CoherenceIssue>? earlier)
    {
        if (earlier is null || earlier.Count == 0)
        {
            return "";
        }

        var text = new StringBuilder();
        text.AppendLine();
        text.AppendLine("# Your last review");
        text.AppendLine("You sent the case back for these, and the steps that own them have been revised. For each, check that it is fixed. Raise it again only if it still fails as you described it; do not ask for more on a point that is now adequate. Raise a new problem only if it would really fail at the table.");
        foreach (var issue in earlier)
        {
            text.AppendLine($"- [{issue.Severity}] {issue.Code} at {string.Join(", ", issue.Targets ?? [])}: {issue.Explanation}{(issue.SuggestedFix is null ? "" : $" Fix: {issue.SuggestedFix}")}");
        }

        return text.ToString();
    }
}
