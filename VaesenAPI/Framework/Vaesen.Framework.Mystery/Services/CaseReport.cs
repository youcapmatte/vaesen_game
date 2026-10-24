using System.Globalization;
using System.Text;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Services;

/// <summary>
/// A case packet as a readable Markdown report, for the owner to judge generated cases without a session (CaseLab and
/// the host's case view). Spoilers throughout.
/// </summary>
public static class CaseReport
{
    /// <summary>A stored case with its brief, its log and, if it was rejected, the step that gave up.</summary>
    public static string ToMarkdown(Data.StoredCase stored) => ToMarkdown(stored.Packet, stored.Brief, stored.Log, stored.FailedStep);

    public static string ToMarkdown(CasePacket p, AssignmentBrief? brief = null, IReadOnlyList<GenerationLogEntry>? log = null, string? failedStep = null)
    {
        var s = new StringBuilder();
        var inv = CultureInfo.InvariantCulture;
        s.AppendLine(inv, $"# {p.Title}");
        s.AppendLine();
        if (failedStep is not null)
        {
            s.AppendLine(inv, $"> **Rejected: the {failedStep} step gave up.** What follows is the case as far as it got; the last attempt of {failedStep} is at the end, under \"Where it failed\".");
            s.AppendLine();
        }

        s.AppendLine(inv, $"> Spoilers: this is the hidden GM file. `{p.MysteryId}`, generated {p.GeneratedAt:yyyy-MM-dd HH:mm} UTC by {p.Models.Creative}{(string.IsNullOrEmpty(p.Models.Critic) ? "" : $", critic {p.Models.Critic}")}.");
        s.AppendLine();
        s.AppendLine(inv, $"- **Level:** {p.Brief.LevelId}, {(p.Brief.Year > 0 ? new InWorldDate(p.Brief.Year, p.Brief.Month).ToString() : $"month {p.Brief.Month}")}{(p.Brief.ReturnVisit ? $"; a return visit after {string.Join(", ", p.Brief.PreviousMysteryIds)}" : "")}");
        s.AppendLine(inv, $"- **Creature:** {string.Join(", ", p.Brief.VaesenIds)}; reason {p.Truth.ReasonKind}{(p.Truth.Origin is null ? "" : $", origin {p.Truth.Origin}")}");
        s.AppendLine(inv, $"- **Shape:** focus {p.Truth.Focus}, plot centre {p.Truth.PlotCentre.Kind}{(p.Truth.PlotCentre.CharacterId is null ? "" : $" ({p.Truth.PlotCentre.CharacterId})")}, conflict {p.Brief.ConflictArchetypeId}");
        s.AppendLine(inv, $"- **Hook:** {p.Hook.Channel} from {Name(p, p.Hook.HookGiverId)}");
        s.AppendLine(inv, $"- **Validation:** {(p.Validation.Passed ? "passed" : "FAILED")}, {p.Validation.Runs} validator runs, {p.Validation.Issues.Count(i => i.IsError)} errors, {p.Validation.Issues.Count(i => !i.IsError)} warnings");
        if (brief?.Notes.Count > 0)
        {
            s.AppendLine(inv, $"- **Assignment notes:** {string.Join(" ", brief.Notes)}");
        }

        Section(s, "The truth");
        s.AppendLine(p.Truth.Summary);
        s.AppendLine();
        if (p.PublicFactsExplained.Count > 0)
        {
            s.AppendLine("**Public facts this case explains** (they join the Society's world when it is played; a later case on the level must keep to them):");
            foreach (var e in p.PublicFactsExplained)
            {
                s.AppendLine(inv, $"- {e.PublicFactId} \"{e.Fact}\": {e.Explanation}");
            }

            s.AppendLine();
        }

        s.AppendLine(inv, $"**Provocation** ({p.Truth.ProvocationPlaceId}, {p.Truth.ProvocationAffordanceTag}): {p.Truth.Provocation}");
        if (p.Truth.HiddenHandMotive is not null)
        {
            s.AppendLine();
            s.AppendLine(inv, $"**Hidden hand:** {p.Truth.HiddenHandMotive}");
        }

        if (p.Truth.DecoyReading is not null)
        {
            s.AppendLine();
            s.AppendLine(inv, $"**Decoy reading:** {p.Truth.DecoyReading}");
        }

        Section(s, "The hook");
        s.AppendLine("> " + p.Hook.Text.Replace("\n", "\n> ", StringComparison.Ordinal));
        s.AppendLine();
        s.AppendLine(inv, $"States {Ids(p.Hook.AssertedFactIds)}; reports events {Ids(p.Hook.OpeningEventIds)}.");

        Section(s, "Escalation");
        foreach (var e in p.Truth.Escalation)
        {
            s.AppendLine(inv, $"- **{e.Id}**{(e.Opening ? " (opening)" : "")} [{e.Register}, {e.PlaceId ?? "terrain"}, {e.When}]: {e.Description} → {Ids(e.FactIds)}");
        }

        Section(s, "Facts and their channels");
        var graph = new FactGraph(p);
        var walk = graph.Walk();
        foreach (var f in p.Facts)
        {
            var flags = new List<string> { f.Kind, f.PointsTo };
            if (f.LoadBearing)
            {
                flags.Insert(0, "LOAD-BEARING");
            }

            if (f.Giveaway)
            {
                flags.Add("giveaway");
            }

            if (f.DeepLore)
            {
                flags.Add("deep lore");
            }

            var position = walk.Order.IndexOf(f.Id);
            s.AppendLine(inv, $"- **{f.Id}** ({string.Join(", ", flags)}; {(position < 0 ? "never reached" : $"reached #{position + 1}")}, {graph.SourcesOf(f.Id).Count} sources): {f.Statement}");
            foreach (var c in p.Channels.Where(c => c.FactId == f.Id))
            {
                var where = c.SourceCharacterId is not null ? Name(p, c.SourceCharacterId) : $"{c.PlaceId}/{c.AffordanceId}";
                var gates = c.Access.Requires.Concat(c.Access.Item is { Length: > 0 } item ? [$"item: {item}"] : Array.Empty<string>()).ToList();
                var gate = gates.Count == 0 ? "open" : string.Join(", ", gates);
                s.AppendLine(inv, $"  - {c.Id} {c.Kind} via {where} [{gate}]: {c.Reveal}");
            }
        }

        Section(s, "Cast");
        foreach (var c in p.Cast)
        {
            var who = c.Identity is null ? "" : $"{c.Identity.Age}, {c.Identity.Occupation}. ";
            s.AppendLine(inv, $"- **{c.Name}**{(c.Handle is null ? "" : $", \"{c.Handle}\"")} (`{c.CharacterId}`, {c.InstanceRef}, {c.Importance}, {c.PlotRole}{(c.Keeps is null ? "" : $", keeps {c.Keeps}")}): {who}Wants {string.Join("; ", c.Wants.Drives)}.{(c.Wants.Guilt is null ? "" : $" Guilt: {c.Wants.Guilt}")}");
            if (c.Knows.Count > 0)
            {
                s.AppendLine(inv, $"  - knows {string.Join(", ", c.Knows.Select(k => $"{k.FactId} ({k.Source})"))}");
            }

            foreach (var lie in c.Lies)
            {
                s.AppendLine(inv, $"  - lies about {lie.About}: {lie.Story}");
            }

            foreach (var belief in c.Thinks)
            {
                s.AppendLine(inv, $"  - thinks ({(belief.True ? "true" : "false")}): {belief.Belief}");
            }
        }

        if (p.Decoys.Count > 0)
        {
            Section(s, "Decoys");
            foreach (var d in p.Decoys)
            {
                s.AppendLine(inv, $"- **{d.ThreadId}**: {d.Description} Facts {Ids(d.FactIds)}, cleared by {Ids(d.ClearedByFactIds)}. {d.Resolution}");
            }
        }

        if (p.OffstagePeople.Count > 0)
        {
            Section(s, "Offstage people");
            foreach (var o in p.OffstagePeople)
            {
                s.AppendLine(inv, $"- **{o.Name}** (`{o.Id}`), {o.Relation}. {o.Note}");
            }
        }

        Section(s, "Beats and climax");
        foreach (var b in p.Beats.OrderBy(b => b.Threshold))
        {
            s.AppendLine(inv, $"- **{b.Id}** at {b.Threshold:0.00}{(b.Requires.Place is null ? "" : $", at {b.Requires.Place}")}{(b.Requires.Time is null ? "" : $", {b.Requires.Time}")}: {b.Description} → {Ids(b.EmitsFactIds)}");
        }

        s.AppendLine(inv, $"- **Climax** at {p.Climax.Threshold:0.00} ({p.Climax.Conditions.Place ?? "anywhere"}, {p.Climax.Conditions.Time ?? "any time"}): {p.Climax.Description}");
        s.AppendLine(inv, $"- Tension starts at {p.Tension.Start:0.00} and is {p.Tension.Arrival:0.00} on arrival. The deep solve's path ({Ids(p.Tension.DeepPath)}) ends at {p.Tension.DeepPathTotal:0.00}, {(p.Climax.Threshold > 0 ? p.Tension.DeepPathTotal / p.Climax.Threshold : 0):0.00} times the climax threshold; everything reachable adds up to {walk.FinalTension:0.00}.");

        Section(s, "Resolution levers");
        foreach (var r in p.ResolutionLevers)
        {
            s.AppendLine(inv, $"- **{r.Id}** ({r.SolveDepth}, needs {Ids(r.RequiresFactIds)}): {r.Description} World: {string.Join(" ", r.Consequences.World)}");
            if (r.RequiredDeeds.Count > 0)
            {
                s.AppendLine(inv, $"  - Deeds ({(r.DeedOrder == DeedOrders.Listed ? "in order" : "any order")}):");
                foreach (var d in r.RequiredDeeds)
                {
                    var where = string.Join(", ", new[] { d.Place, d.AffordanceId, d.Time }.OfType<string>());
                    var by = d.PerformedBy.Who.Count == 0 ? "" : $", by {(d.PerformedBy.Who.Count > 1 ? d.PerformedBy.Rule + " of " : "")}{string.Join(", ", d.PerformedBy.Who)}";
                    var before = d.Witnesses.Count == 0 ? "" : $", before {(d.Witnesses.Count > 1 ? d.WitnessRule + " of " : "")}{string.Join(", ", d.Witnesses)}";
                    s.AppendLine(inv, $"    - {d.Id}: {d.Description}{(where.Length == 0 ? "" : $" [{where}]")}{by}{before}");
                }
            }
        }

        foreach (var m in p.CreatureMinds)
        {
            Section(s, $"The creature's mind ({m.VaesenId})");
            s.AppendLine(inv, $"- Wants: {string.Join("; ", m.Wants)}");
            s.AppendLine(inv, $"- Terms: {string.Join("; ", m.Terms)} (deeds {Ids(m.TermDeedIds)})");
            s.AppendLine(inv, $"- Red lines: {string.Join("; ", m.RedLines)}");
            if (m.Blames.Count > 0)
            {
                s.AppendLine(inv, $"- Blames: {string.Join("; ", m.Blames.Select(b => $"{b.CharacterId}: {b.Why}"))}");
            }
            s.AppendLine(inv, $"- Knows {Ids(m.Knows.Select(k => k.FactId))}; thinks: {string.Join("; ", m.Thinks.Select(t => $"{t.Belief} ({(t.True ? "true" : "false")})"))}");
            s.AppendLine(inv, $"- Voice: {m.Voice}");
            var r = m.Reactions;
            s.AppendLine(inv, $"- Offering: {r.Offering} Threat: {r.Threat} Steel: {r.Steel} Prayer: {r.Prayer} Bargain: {r.Bargain} Apology: {r.Apology} Lie: {r.Lie}");
        }

        if (p.Level.SceneDescriptions.Count > 0)
        {
            Section(s, "Scenes");
            foreach (var (place, text) in p.Level.SceneDescriptions)
            {
                s.AppendLine(inv, $"- **{place}**: {text}");
            }
        }

        Section(s, "Validator");
        if (p.Validation.Issues.Count == 0)
        {
            s.AppendLine("No issues.");
        }

        foreach (var i in p.Validation.Issues.OrderBy(i => i.IsError ? 0 : 1))
        {
            s.AppendLine(inv, $"- {(i.IsError ? "**error**" : "warning")}{(i.Applied ? " (fix applied)" : "")} `{i.Code}` at {i.Location} ({i.Source}): {i.Explanation}{(i.SuggestedFix is null ? "" : $" *Fix:* {i.SuggestedFix}")}");
        }

        if (log is { Count: > 0 })
        {
            Section(s, "Generation log");
            s.AppendLine("| case | step | try | kind | passed | in | cache read | cache written | out | ms | USD | issues |");
            s.AppendLine("|---|---|---|---|---|---|---|---|---|---|---|---|");
            foreach (var e in log)
            {
                s.AppendLine(inv, $"| {e.CaseAttempt} | {e.Step} | {e.Attempt} | {e.Kind} | {(e.Passed ? "yes" : "no")} | {e.InputTokens} | {e.CacheReadTokens} | {e.CacheWriteTokens} | {e.OutputTokens} | {e.LatencyMs} | {e.CostUsd:0.000} | {(e.Error ?? string.Join("; ", e.Issues.Where(i => i.IsError).Select(i => i.Code).Distinct()))} |");
            }

            s.AppendLine();
            s.AppendLine(inv, $"Total: {log.Count(e => e.Kind != LogEntryKinds.Check)} calls ({log.Count(e => e.Kind == LogEntryKinds.Patch)} patches, {log.Count(e => e.Kind == LogEntryKinds.Fix)} fixes), {log.Sum(e => e.InputTokens)} input, {log.Sum(e => e.CacheReadTokens)} read from the cache and {log.Sum(e => e.CacheWriteTokens)} written to it, {log.Sum(e => e.OutputTokens)} output tokens, {log.Sum(e => e.LatencyMs) / 1000.0:0.0} s, about ${log.Sum(e => e.CostUsd):0.00} at list prices.");

            // A rejected case: the failed step's last attempt, with its problems and its answer as it came back.
            var lastTry = failedStep is null ? null : log.LastOrDefault(e => e.Step == failedStep);
            if (lastTry is not null)
            {
                Section(s, "Where it failed");
                s.AppendLine(inv, $"The {failedStep} step's last attempt (try {lastTry.Attempt}, {lastTry.Kind}):");
                s.AppendLine();
                foreach (var i in lastTry.Issues.OrderBy(i => i.IsError ? 0 : 1))
                {
                    s.AppendLine(inv, $"- {(i.IsError ? "**error**" : "warning")} `{i.Code}` at {i.Location}: {i.Explanation}");
                }

                if (!string.IsNullOrWhiteSpace(lastTry.Output))
                {
                    s.AppendLine();
                    s.AppendLine("```json");
                    s.AppendLine(lastTry.Output);
                    s.AppendLine("```");
                }
            }
        }

        return s.ToString();
    }

    private static void Section(StringBuilder s, string title)
    {
        s.AppendLine();
        s.AppendLine("## " + title);
        s.AppendLine();
    }

    private static string Ids(IEnumerable<string> ids)
    {
        var list = ids.ToList();
        return list.Count == 0 ? "nothing" : string.Join(", ", list);
    }

    private static string Name(CasePacket p, string? id) => p.FindCast(id)?.Name ?? id ?? "nobody";
}
