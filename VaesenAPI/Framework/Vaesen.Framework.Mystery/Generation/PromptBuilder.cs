using System.Reflection;
using System.Text;
using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Builds the prompts from the templates in Generation/Prompts (embedded markdown, editable without touching code).
/// The system prompt is the same for every case and the case context for every step of a case, so both are cached;
/// only the step instruction changes per call.
/// </summary>
public static class PromptBuilder
{
    private static readonly JsonSerializerSettings Compact = new()
    {
        NullValueHandling = NullValueHandling.Ignore,
        DefaultValueHandling = DefaultValueHandling.Include,
        Formatting = Formatting.Indented,
    };

    public static string Template(string name)
    {
        var assembly = typeof(PromptBuilder).Assembly;
        var resource = assembly.GetManifestResourceNames().FirstOrDefault(n => n.EndsWith($".Prompts.{name}.md", StringComparison.Ordinal))
            ?? throw new InvalidOperationException($"Prompt template '{name}' is not embedded.");
        using var stream = assembly.GetManifestResourceStream(resource)!;
        using var reader = new StreamReader(stream);
        return reader.ReadToEnd();
    }

    public static string Fill(string template, IReadOnlyDictionary<string, string> values)
    {
        var text = new StringBuilder(template);
        foreach (var (key, value) in values)
        {
            text.Replace("{{" + key + "}}", value);
        }

        return text.ToString();
    }

    public static string Json(object value) => JsonConvert.SerializeObject(value, Compact);

    public static string System(Setting setting)
    {
        var conventions = setting.PeriodConventions;
        var period = string.Join(" ", new[]
        {
            $"Travel: {string.Join("; ", conventions.Travel)}.",
            $"Communication: {string.Join("; ", conventions.Communication)}.",
            $"Money: {conventions.Money}",
            $"Ranks: {string.Join(", ", conventions.SocialRanks)}.",
            string.Join(" ", conventions.Notes),
        });

        return Fill(Template("system"), new Dictionary<string, string>
        {
            ["era"] = setting.Era,
            ["worldTruths"] = string.Join("\n", setting.WorldTruths.Select(t => $"- {t}")),
            ["periodConventions"] = period,
        });
    }

    /// <summary>Everything the case is made from: the brief, the level, the creature, the conflicts and the people on offer.</summary>
    public static string CaseContext(MysteryDraft draft)
    {
        var brief = draft.Brief;
        var hook = draft.Pack.Setting.FindHookChannel(brief.HookChannelId);
        var origin = draft.Vaesen.OriginSet.Find(o => o.Id == brief.OriginId);
        var roster = draft.Pack.Characters.Where(c => brief.PersistentCastIds.Contains(c.Id)).Select(c => new
        {
            id = c.Id,
            name = c.Name,
            c.Identity.Occupation,
            c.Identity.Home,
            c.Identity.Age,
            attitudeToSociety = c.StartingAttitude,
            c.AttitudeReason,
            voice = c.Voice.Register,
            drives = c.Drives.Select(d => d.Text),
            latentHooks = c.LatentHooks.Select(h => h.Text),
        });

        var text = new StringBuilder();
        text.AppendLine("# The assignment");
        text.AppendLine($"- Society: {draft.History.SocietyName} ({draft.History.SocietyId}). Investigators: {(draft.History.Investigators.Count == 0 ? "not known yet" : string.Join("; ", draft.History.Investigators))}.");
        text.AppendLine($"- Month: {brief.Month} (in-world year {draft.Pack.Setting.StartYear}).");
        text.AppendLine($"- Level: {draft.Level.Name} ({draft.Level.Id}).");
        text.AppendLine($"- Creature: {draft.Vaesen.Name} ({draft.Vaesen.Id}).");
        text.AppendLine($"- Reason kind: {brief.ReasonKind}.");
        if (origin is not null)
        {
            text.AppendLine($"- Origin of the affliction: {origin.Description} (hidden hand: {(origin.HiddenHand ? "yes, " + origin.HandDisposition : "no")}).");
        }

        text.AppendLine($"- Hidden hand: {(brief.HiddenHand ? $"yes ({brief.HandDisposition})" : "no")}. Plot centre: {brief.PlotCentreKind}.");
        text.AppendLine($"- Focus: {brief.Focus} ({FocusMeaning(brief.Focus)}).");
        text.AppendLine($"- Hook channel: {hook?.Name ?? brief.HookChannelId} ({brief.HookChannelId}). Hook-giver type: {brief.HookGiverType}.");
        text.AppendLine($"- Red herring density: {brief.RedHerringDensity}.");
        text.AppendLine();
        text.AppendLine("# The level");
        text.AppendLine(Json(draft.Level));
        text.AppendLine();
        text.AppendLine("# The creature");
        text.AppendLine(Json(draft.Vaesen));
        text.AppendLine();
        text.AppendLine("# Conflict archetypes on offer");
        text.AppendLine(Json(draft.Conflicts));
        text.AppendLine();
        text.AppendLine("# Persistent characters who may be drawn in");
        text.AppendLine(Json(roster));
        return text.ToString();
    }

    public static string FocusMeaning(string focus) => focus switch
    {
        CaseFocus.What => "identifying the creature is the puzzle",
        CaseFocus.WhoWhy => "the creature can be identified early; who is involved and why is hidden",
        CaseFocus.How => "everything can be understood; the resolution is the dilemma",
        _ => focus,
    };

    public static string HandLineForConflict(AssignmentBrief brief) => brief.HiddenHand
        ? $"There is a hidden hand: a person who caused or controls this ({brief.HandDisposition}). The plot centres on them and the creature is their weapon or symptom. Fix who they are and their motive in hiddenHandMotive."
        : $"There is no hidden hand: the plot centres on {(brief.PlotCentreKind == PlotCentreKinds.Nature ? "the creature's own nature" : "the provocation")}. Set hiddenHandMotive to null.";

    public static string HandLineForHumans(AssignmentBrief brief) => brief.HiddenHand
        ? "Cast the hidden hand fixed in the conflict as a significant character and give their id in hiddenHandCharacterId. Their guilt and their belief about how exposed they are drive the case."
        : "There is no hidden hand; set hiddenHandCharacterId to null. Whoever has something to hide (the afflicted, the one who provoked) carries the pressure.";

    public static string OpeningRule(AssignmentBrief brief) => brief.Focus == CaseFocus.What
        ? "at least three, all built from the creature's ambiguous signs (never its giveaway tells)."
        : "at least two.";

    public static string GiveawayRuleForEscalation(AssignmentBrief brief) => brief.Focus == CaseFocus.What
        ? "Identity is the puzzle: no opening event may be one of the creature's giveaway tells or close to one. Many quiet oddities beat one loud event."
        : "Identity is not the puzzle here, so the creature's giveaway signs may appear from the start.";

    public static string GiveawayRuleForClues(AssignmentBrief brief) => brief.Focus == CaseFocus.What
        ? "Identity is the puzzle: every channel to a giveaway fact is gated on other discoveries (knowsFact), and no giveaway is in the hook or the opening."
        : "Identity is not the puzzle here; giveaway facts may be found early.";

    public static string DecoyRule(string density) => density switch
    {
        "high" => "Build two decoy threads: one a whole false case with its own suspect, clues and a climax the players could solve; the other may be a decoy creature from the folklore the players will half-remember.",
        "medium" => "Build one or two decoy threads; one may be a whole false case with its own suspect.",
        _ => "Build one decoy thread, often the obvious folklore reading of the creature or a suspicious but innocent person.",
    };

    public static string ClimaxRule(VaesenDefinition vaesen)
    {
        var c = vaesen.ConfrontationConditions;
        var places = string.Join(", ", c.PlaceTags);
        return c.Kind switch
        {
            ConfrontationKinds.Time => $"the creature can only be confronted {c.Time}: the climax conditions must say when (time).",
            ConfrontationKinds.Place => $"the creature can only be confronted at {places}: the climax conditions must name a place id of the level where that is (place).",
            ConfrontationKinds.Both => $"the creature can only be confronted {c.Time} at {places}: the climax conditions must give both the time and a place id of the level.",
            _ => "set its conditions to where and when it naturally lands (or null).",
        } + " Pressing an accusation too early yields denial, an alibi or doubters, not an ending.";
    }

    public static string LeverRule(AssignmentBrief brief, VaesenDefinition vaesen) => brief.HiddenHand
        ? "There is a hidden hand, so include a crude solve that is always available (deal with the creature by force) and a deep solve (expose the hand, lift the curse, save the victim), the better outcome, which must be discoverable."
        : vaesen.ResolutionShape == ResolutionShapes.Negotiable
            ? "The creature's grievance can be answered: include a lever that answers it, alongside a cruder one."
            : "The creature cannot be bargained with: the levers are about driving it off, ending the provocation or protecting its victims.";

    public static string SeasonRule(AssignmentBrief brief) => brief.SeasonThreads.Count == 0
        ? "There is no season thread yet: leave seasonThreads empty."
        : "Carry these season threads, each as at least one fact with its own channels, never resolved here: " + string.Join("; ", brief.SeasonThreads);

    public static string FocusRuleForCritic(string focus) => focus switch
    {
        CaseFocus.What => "with identity as the puzzle, concealment must be hard and no giveaway may be in the opening or the early load-bearing clues.",
        CaseFocus.WhoWhy => "identifying the creature may come early; the difficulty is in who is behind it and why.",
        _ => "everything can be understood; the difficulty is the dilemma of how to resolve it.",
    };

    public static string SolveRuleForCritic(AssignmentBrief brief) => brief.HiddenHand
        ? "there is a crude solve and a deep, discoverable solve; the deep one is the better outcome."
        : "the resolution levers are distinct, plausible and have different consequences for the world and the cast.";

    /// <summary>The case so far, and on a retry the issues to fix and the previous answer.</summary>
    public static string WithCaseAndFeedback(string instruction, CasePacket soFar, IReadOnlyList<CoherenceIssue> feedback, string? previousJson)
    {
        var text = new StringBuilder();
        text.AppendLine("# The case so far");
        text.AppendLine(Json(soFar));
        text.AppendLine();
        text.AppendLine(instruction);
        if (feedback.Count > 0)
        {
            text.AppendLine();
            text.AppendLine("# Your last answer to this step had these problems. Fix them and keep what worked.");
            foreach (var issue in feedback)
            {
                text.AppendLine($"- [{issue.Severity}] {issue.Code} at {issue.Location}: {issue.Explanation}{(issue.SuggestedFix is null ? "" : $" Fix: {issue.SuggestedFix}")}");
            }

            if (previousJson is not null)
            {
                text.AppendLine();
                text.AppendLine("Your last answer:");
                text.AppendLine(previousJson);
            }
        }

        return text.ToString();
    }
}
