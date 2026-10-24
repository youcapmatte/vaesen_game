using System.Text;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// A short look at whether the critic's issues were really fixed (review 2, item 1), so "fix applied" is never set on
/// trust: the critic model at low effort, with the same system and context as the critic so it reads the cache.
/// </summary>
public sealed class FixVerifier(IMysteryModel model, MysteryOptions options)
{
    public const string StepName = "verify";

    /// <summary>For each issue, in order: null when resolved, otherwise what still stands.</summary>
    public async Task<ResultBase<List<string?>>> VerifyAsync(MysteryDraft draft, IReadOnlyList<CoherenceIssue> issues, int caseAttempt, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<List<string?>>();
        var list = new StringBuilder();
        for (var i = 0; i < issues.Count; i++)
        {
            var issue = issues[i];
            list.AppendLine($"{i + 1}. {issue.Code} at {string.Join(", ", issue.Targets is { Count: > 0 } t ? t : [issue.Location])}: {issue.Explanation}{(issue.SuggestedFix is null ? "" : $" Fix asked: {issue.SuggestedFix}")}");
        }

        var entry = new GenerationLogEntry { CaseAttempt = caseAttempt, Step = StepName };
        draft.Log.Add(entry);
        entry.Instruction = PromptBuilder.WithCase(PromptBuilder.Fill(PromptBuilder.Template("verify"), new Dictionary<string, string> { ["issues"] = list.ToString() }), draft.Packet);
        var reply = await model.GenerateAsync<VerifyOutput>(new ModelRequest
        {
            Step = StepName,
            Model = options.CriticModel,
            Effort = options.LightEffort,
            MaxTokens = 8000,
            System = draft.SystemPrompt,
            Context = draft.ContextFor(CriticStep.Name),
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

        // An issue the look did not answer counts as not verified: never fixed on trust.
        return ret.Ok(issues.Select((_, i) => answer.Value.Results.Find(r => r.Issue == i + 1) is { } r
            ? r.Resolved ? null : (string.IsNullOrWhiteSpace(r.Remaining) ? "Still as described." : r.Remaining)
            : "The verifying look did not answer this one.").ToList());
    }
}
