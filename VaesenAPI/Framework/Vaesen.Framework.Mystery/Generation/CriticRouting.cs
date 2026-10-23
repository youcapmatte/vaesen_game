using System.Text.RegularExpressions;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Sends each of the critic's issues to the step that wrote what it is about (fixes 2, section 1), by the ids in its
/// targets: a lever goes to the arc, a motive to the conflict, a channel to minds and channels. An issue that spans
/// steps is split, one part per owning step, each located on its items so the step can patch just those.
/// </summary>
public static partial class CriticRouting
{
    /// <summary>The steps a fix can go to, in pipeline order: upstream fixes come first, since they can affect what follows.</summary>
    public static readonly IReadOnlyList<string> PipelineOrder =
    [
        ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, FactsStep.StepName, MindsStep.StepName, ArcStep.StepName, HookStep.StepName,
    ];

    private static readonly HashSet<string> MindParts = new(StringComparer.OrdinalIgnoreCase) { "knows", "thinks", "lies" };

    /// <summary>The issues grouped by owning step, in pipeline order; each part keeps a link to the issue it came from.</summary>
    public static List<(string Step, List<(CoherenceIssue Part, CoherenceIssue Original)> Issues)> Route(IEnumerable<CoherenceIssue> issues)
    {
        var routed = new List<(string Step, CoherenceIssue Part, CoherenceIssue Original)>();
        foreach (var issue in issues)
        {
            var targets = issue.Targets ?? [];
            if (targets.Count == 0)
            {
                routed.Add((DefaultStep(issue.Code), issue, issue));
                continue;
            }

            foreach (var (step, location) in targets.Select(Owner).Distinct())
            {
                routed.Add((step, Copy(issue, location), issue));
            }
        }

        return routed
            .GroupBy(r => r.Step)
            .OrderBy(g => PipelineOrder.ToList().IndexOf(g.Key))
            .Select(g => (g.Key, g.Select(r => (r.Part, r.Original)).ToList()))
            .ToList();
    }

    /// <summary>The step that owns a target id, and the issue location that names it for a patch ("lever:r3").</summary>
    public static (string Step, string Location) Owner(string target)
    {
        var t = target.Trim();
        if (t.StartsWith("truth.", StringComparison.Ordinal))
        {
            return (ConflictStep.StepName, t);
        }

        if (t is "hook" or "placesUsed" || t.StartsWith("scene", StringComparison.Ordinal))
        {
            return (HookStep.StepName, t);
        }

        if (t is "climax")
        {
            return (ArcStep.StepName, t);
        }

        if (t is "eventFacts")
        {
            return (FactsStep.StepName, t);
        }

        var item = ItemId().Match(t);
        if (item.Success)
        {
            return item.Groups[1].Value switch
            {
                "e" => (EscalationStep.StepName, $"event:{t}"),
                "f" => (FactsStep.StepName, $"fact:{t}"),
                "d" => (FactsStep.StepName, $"decoy:{t}"),
                "c" => (MindsStep.StepName, $"channel:{t}"),
                "b" => (ArcStep.StepName, $"beat:{t}"),
                _ => (ArcStep.StepName, $"lever:{t}"),
            };
        }

        // A person, with the part meant: what they know and say is minds and channels', who they are is humans'.
        var dot = t.LastIndexOf('.');
        var person = dot > 0 ? t[..dot] : t;
        var part = dot > 0 ? t[(dot + 1)..] : "";
        return MindParts.Contains(part) ? (MindsStep.StepName, $"cast:{person}") : (HumansStep.StepName, $"cast:{person}");
    }

    /// <summary>Where an issue with no targets goes: it judges a step's design as a whole, so that step revises.</summary>
    public static string DefaultStep(string code) => code switch
    {
        "critic." + CriticChecks.Hook => HookStep.StepName,
        "critic." + CriticChecks.Solves => ArcStep.StepName,
        "critic." + CriticChecks.DeepLoreTrail => FactsStep.StepName,
        "critic." + CriticChecks.HonestRedHerrings => FactsStep.StepName,
        "critic." + CriticChecks.HumansFromConflict => HumansStep.StepName,
        _ => MindsStep.StepName,
    };

    private static CoherenceIssue Copy(CoherenceIssue issue, string location) => new()
    {
        Code = issue.Code,
        Severity = issue.Severity,
        Location = location,
        Explanation = issue.Explanation,
        SuggestedFix = issue.SuggestedFix,
        Source = issue.Source,
        Targets = issue.Targets,
    };

    [GeneratedRegex("^([efdcbr])\\d+$")]
    private static partial Regex ItemId();
}
