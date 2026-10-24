using System.Text;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The scoped re-check (fixes 5, section 4): one small call on the small model that sees only the items the critic's
/// issues name, as they were before the last round and as they are now, and says per issue whether the problem is
/// gone. Only its yes makes a label "fixed (verified)". It replaces review 2's look at the whole case.
/// </summary>
public sealed class FixVerifier(IMysteryModel model, MysteryOptions options)
{
    public const string StepName = "verify";

    /// <summary>The most of one item shown; a long fact list is cut, not the call.</summary>
    private const int ItemChars = 1500;

    /// <summary>For each issue, in order: null when resolved, otherwise what still stands.</summary>
    public async Task<ResultBase<List<string?>>> VerifyAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, CasePacket before, int caseAttempt, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<List<string?>>();
        var (was, now) = (CaseItems.Tree(before), CaseItems.Tree(draft.Packet));
        var list = new StringBuilder();
        for (var i = 0; i < issues.Count; i++)
        {
            var issue = issues[i];
            list.AppendLine($"## Issue {i + 1}: {issue.Code}");
            list.AppendLine($"Problem: {issue.Explanation}");
            if (issue.SuggestedFix is { Length: > 0 } fix)
            {
                list.AppendLine($"Fix asked: {fix}");
            }

            foreach (var part in (issue.Parts ?? []).Where(p => p.Owner == "code"))
            {
                list.AppendLine($"Done by code: {part.Kind} {part.Detail} ({part.Note})");
            }

            var targets = (issue.Parts ?? []).Where(p => p.Kind == FixPartKinds.Patch).Select(p => p.Target)
                .Concat(issue.Targets ?? [])
                .Where(t => t.Length > 0)
                .Distinct(StringComparer.Ordinal);
            foreach (var target in targets)
            {
                list.AppendLine($"- {target} before: {Cut(CaseItems.Json(was, target))}");
                list.AppendLine($"- {target} now: {Cut(CaseItems.Json(now, target))}");
            }

            list.AppendLine();
        }

        var entry = new GenerationLogEntry { CaseAttempt = caseAttempt, Step = StepName };
        draft.Log.Add(entry);
        entry.Instruction = PromptBuilder.Fill(PromptBuilder.Template("verify"), new Dictionary<string, string> { ["issues"] = list.ToString() });
        var reply = await model.GenerateAsync<VerifyOutput>(new ModelRequest
        {
            Step = StepName,
            Model = options.LightModel,
            Effort = options.LightEffort,
            MaxTokens = 4000,
            System = draft.SystemPrompt,
            Context = $"A re-check of fixes to one case, \"{draft.Packet.Title}\". Only the items the issues name are shown, before and after.",
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
        entry.CacheReadTokens = answer.CacheReadTokens;
        entry.CacheWriteTokens = answer.CacheWriteTokens;
        entry.LatencyMs = answer.LatencyMs;
        entry.PrefixHash = answer.PrefixHash;
        entry.CostUsd = ModelPrices.Cost(answer.Model, answer.InputTokens, answer.CacheWriteTokens, answer.CacheReadTokens, answer.OutputTokens);
        entry.Output = answer.RawJson;
        entry.Passed = true;

        // An issue the re-check did not answer counts as not verified: never fixed on trust.
        return ret.Ok(issues.Select((_, i) => answer.Value.Results.Find(r => r.Issue == i + 1) is { } r
            ? r.Resolved ? null : (string.IsNullOrWhiteSpace(r.Remaining) ? "Still as described." : r.Remaining)
            : "The re-check did not answer this one.").ToList());
    }

    private static string Cut(string? json) => json is null ? "(not in the case)" : json.Length <= ItemChars ? json : json[..ItemChars] + "…";
}
