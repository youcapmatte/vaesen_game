using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>The small model's reading of a plot: one value per axis, and the plot in one line.</summary>
public sealed class PlotSignatureOutput
{
    [OneOf(typeof(PlotProvocations))]
    [JsonProperty("provocation")]
    public string Provocation { get; set; } = PlotProvocations.Other;

    [OneOf(typeof(PlotBonds))]
    [JsonProperty("bond")]
    public string Bond { get; set; } = PlotBonds.None;

    [OneOf(typeof(PlotVictims))]
    [JsonProperty("victim")]
    public string Victim { get; set; } = PlotVictims.Other;

    [OneOf(typeof(PlotCures))]
    [JsonProperty("cure")]
    public string Cure { get; set; } = PlotCures.Other;

    /// <summary>The plot in one line, with no names: what the creature does, to whom and why.</summary>
    [JsonProperty("oneLine")]
    public string OneLine { get; set; } = "";
}

/// <summary>
/// The structural novelty check (fixes 7, section 4): right after the conflict step, the small model reads the plot
/// into a <see cref="PlotSignature"/>, and code compares it with the signatures of the Society's earlier plots. A
/// repeat is an error the conflict is re-rolled on, with the overlap named, for the price of one conflict call rather
/// than a whole case. The same reading backfills the signature of an older case that has none.
/// </summary>
public sealed class NoveltyCheck(IMysteryModel model, MysteryOptions options)
{
    public const string StepName = "novelty";

    /// <summary>The issue code of a plot that repeats an earlier one.</summary>
    public const string Code = "novelty";

    /// <summary>Reads the plot of <paramref name="packet"/> into its signature; logs the call on <paramref name="log"/>.</summary>
    public async Task<ResultBase<PlotSignature>> SignAsync(CasePacket packet, string vaesenId, List<GenerationLogEntry> log, int caseAttempt, string system, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<PlotSignature>();
        var truth = packet.Truth;
        var entry = new GenerationLogEntry { CaseAttempt = caseAttempt, Step = StepName };
        log.Add(entry);
        entry.Instruction = PromptBuilder.Fill(PromptBuilder.Template("novelty"), new Dictionary<string, string>
        {
            ["plot"] = string.Join("\n", new[]
            {
                $"Title: {packet.Title}",
                $"Truth: {truth.Summary}",
                $"Provocation: {truth.Provocation}",
                truth.HiddenHandMotive is { Length: > 0 } hand ? $"Hidden hand: {hand}" : "",
                truth.DecoyReading is { Length: > 0 } decoy ? $"What people think: {decoy}" : "",
            }.Where(l => l.Length > 0)),
        });

        var reply = await model.GenerateAsync<PlotSignatureOutput>(new ModelRequest
        {
            Step = StepName,
            Model = options.LightModel,
            Effort = options.LightEffort,
            MaxTokens = 1000,
            System = system,
            Context = "One case's plot, to be read into a signature and compared with the plots the Society has already had.",
            Instruction = entry.Instruction,
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
        entry.LatencyMs = answer.LatencyMs;
        entry.CostUsd = ModelPrices.Cost(answer.Model, answer.InputTokens, answer.CacheWriteTokens, answer.CacheReadTokens, answer.OutputTokens);
        entry.Output = answer.RawJson;
        entry.Passed = true;
        var value = answer.Value;
        return ret.Ok(new PlotSignature
        {
            VaesenId = vaesenId,
            ReasonKind = truth.ReasonKind,
            ConflictArchetypeId = packet.Brief.ConflictArchetypeId,
            Provocation = value.Provocation,
            Bond = value.Bond,
            Victim = value.Victim,
            Cure = value.Cure,
            OneLine = value.OneLine,
        });
    }

    /// <summary>
    /// What <paramref name="now"/> repeats of the earlier plots, as errors on the conflict: the same creature provoked the
    /// same way, or three of the four axes the same whatever the creature.
    /// </summary>
    public static List<CoherenceIssue> Overlaps(PlotSignature now, IEnumerable<EarlierPlot> earlier)
    {
        var issues = new List<CoherenceIssue>();
        foreach (var plot in earlier.Where(p => p.Signature is not null))
        {
            var was = plot.Signature!;
            var same = new List<string>();
            if (was.Provocation == now.Provocation && now.Provocation != PlotProvocations.Other)
            {
                same.Add($"provocation ({now.Provocation})");
            }

            if (was.Bond == now.Bond && now.Bond != PlotBonds.None)
            {
                same.Add($"bond ({now.Bond})");
            }

            if (was.Victim == now.Victim && now.Victim != PlotVictims.Other)
            {
                same.Add($"victim ({now.Victim})");
            }

            if (was.Cure == now.Cure && now.Cure != PlotCures.Other)
            {
                same.Add($"cure ({now.Cure})");
            }

            var creature = was.VaesenId == now.VaesenId;
            if ((creature && was.Provocation == now.Provocation && now.Provocation != PlotProvocations.Other) || same.Count >= 3)
            {
                issues.Add(CoherenceIssue.Error(Code, "truth.provocation",
                    $"This plot repeats \"{plot.Title}\"{(creature ? ", with the same creature," : "")} in its {string.Join(", ", same)}: {was.OneLine}",
                    $"Tell another story: the creature provoked some other way than {now.Provocation}{(same.Count > 1 ? $", and not the same {string.Join(" or ", same.Skip(1).Select(s => s.Split(' ')[0]))}" : "")}."));
            }
        }

        return issues;
    }
}
