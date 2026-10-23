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
        ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, ResidentsStep.StepName, FactsStep.StepName, MindsStep.StepName, ArcStep.StepName, CreatureStep.StepName, HookStep.StepName,
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

        if (t is "creatureMinds" or "creatureMind")
        {
            return (CreatureStep.StepName, "creatureMinds");
        }

        // A lever's deed ("r3-d1") is the lever's.
        if (DeedId().Match(t) is { Success: true } deed)
        {
            return (ArcStep.StepName, $"lever:{deed.Groups[1].Value}");
        }

        if (t is "eventFacts")
        {
            return (FactsStep.StepName, t);
        }

        if (t is "channels" or "channel")
        {
            return (MindsStep.StepName, Patch.NewItem + "channel");
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
        "critic." + CriticChecks.Leverage => HumansStep.StepName,
        "critic." + CriticChecks.NpcRecords => HumansStep.StepName,
        "critic." + CriticChecks.MindsAgree => MindsStep.StepName,
        "critic." + CriticChecks.Mechanisms => EscalationStep.StepName,
        "critic." + CriticChecks.ActsToPlan => ArcStep.StepName,
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

    /// <summary>
    /// What a fix that passed changed upstream, as issues for the items built on it: a revised event for its facts, a
    /// revised fact for the channels that reveal it and the people who know it. Without them a fixed date stays wrong
    /// in every channel that quotes it, and the critic raises it again.
    /// </summary>
    public static List<(string Step, CoherenceIssue Issue)> Dependents(CasePacket before, CasePacket after)
    {
        var found = new List<(string Step, CoherenceIssue Issue)>();
        CoherenceIssue Follow(string location, string what, string text) => new()
        {
            Code = IssueCodes.Consistency,
            Severity = IssueSeverities.Warning,
            Location = location,
            Explanation = $"{what} was revised and now reads: \"{text}\". This item is built on it: make it agree (dates, names, details) and change nothing else.",
            Source = IssueSources.Structural,
        };

        foreach (var e in after.Truth.Escalation)
        {
            var old = before.Truth.Escalation.Find(x => x.Id == e.Id);
            if (old is not null && (old.Description != e.Description || old.When != e.When))
            {
                found.AddRange(e.FactIds.Select(f => (FactsStep.StepName, Follow($"fact:{f}", $"Event {e.Id}", $"{e.When}: {e.Description}"))));
            }
        }

        foreach (var f in after.Facts)
        {
            if (before.Facts.Find(x => x.Id == f.Id) is { } old && old.Statement != f.Statement)
            {
                found.AddRange(after.Channels.Where(c => c.FactId == f.Id).Select(c => (MindsStep.StepName, Follow($"channel:{c.Id}", $"Fact {f.Id}", f.Statement))));
                found.AddRange(after.Cast.Where(c => c.Knows.Exists(k => k.FactId == f.Id)).Select(c => (MindsStep.StepName, Follow($"cast:{c.CharacterId}", $"Fact {f.Id}", f.Statement))));
            }
        }

        return found;
    }

    [GeneratedRegex("^([efdcbr])\\d+$")]
    private static partial Regex ItemId();

    [GeneratedRegex(@"^(r\d+)-d\d+$")]
    private static partial Regex DeedId();
}
