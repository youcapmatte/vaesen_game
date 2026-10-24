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

        // Indentation costs about a fifth of the tokens of every JSON block, and the model reads it as well without.
        Formatting = Formatting.None,
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
    /// <param name="step">The step it is for; null (or the critic) gets everything.</param>
    public static string CaseContext(MysteryDraft draft, string? step = null)
    {
        var brief = draft.Brief;
        var hook = draft.Pack.Setting.FindHookChannel(brief.HookChannelId);
        var origin = draft.Vaesen.OriginSet.Find(o => o.Id == brief.OriginId);
        var offered = (draft.World?.Characters ?? draft.Pack.Characters.Select(WorldSeeder.Instantiate).ToList())
            .Where(c => brief.PersistentCastIds.Contains(c.Id))
            .ToList();
        var recent = draft.History.Mysteries.TakeLast(2).Select(m => m.MysteryId).ToHashSet(StringComparer.Ordinal);
        var ranked = offered
            .OrderByDescending(c => Relevance(c, draft.Level, recent))
            .ThenBy(c => brief.PersistentCastIds.IndexOf(c.Id))
            .ToList();
        var others = ranked.Skip(FullDetailCast).Select(c => $"- {c.Id}: {c.Name}, {c.Identity.Occupation}, {c.Home}").ToList();
        var roster = ranked
            .Take(FullDetailCast)
            .Select(c => new
            {
                id = c.Id,
                name = c.Name,
                tier = c.Tier,
                status = c.Status,
                c.Identity.Occupation,
                c.Home,
                c.Identity.Age,
                attitudeToSociety = c.Attitude,
                c.AttitudeReason,
                voice = c.Voice.Register,
                drives = c.Drives.Select(d => d.Text),
                latentHooks = c.LatentHooks.Select(h => h.Text),
                networkLinks = c.NetworkLinks.Select(l => $"{l.TargetId} ({l.Place}): {l.Influence}"),
                ties = c.Relationships.Select(r => $"{r.CharacterId}: {r.Nature} ({r.Sentiment:+0;-0;0})"),
                remembers = c.Memories.TakeLast(3).Select(m => $"{m.When}: {m.Text}"),
                knowsFromBefore = c.Knowledge.TakeLast(5).Select(k => k.Statement),
            });
        var overlay = draft.World?.Overlay ?? [];
        var gone = draft.World?.Characters.Where(c => c.Status is CharacterStatuses.Dead or CharacterStatuses.Departed).ToList() ?? [];

        var text = new StringBuilder();
        text.AppendLine("# The assignment");
        text.AppendLine($"- Society: {draft.History.SocietyName} ({draft.History.SocietyId}). Investigators: {(draft.History.Investigators.Count == 0 ? "not known yet" : string.Join("; ", draft.History.Investigators))}.");
        text.AppendLine($"- Month: {brief.Month} (in-world year {(brief.Year > 0 ? brief.Year : draft.Pack.Setting.StartYear)}).");
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
        // Each step gets only the canon it works from (fixes 2, section 5); the critic gets all of it.
        bool For(params string[] steps) => step is null or CriticStep.Name || steps.Contains(step);
        if (For(ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, ResidentsStep.StepName, MindsStep.StepName, ArcStep.StepName, HookStep.StepName))
        {
            text.AppendLine("# The level");
            text.AppendLine(Json(LevelView(draft.Level, step)));
            text.AppendLine();
        }

        if (For(ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, FactsStep.StepName, MindsStep.StepName, ArcStep.StepName, CreatureStep.StepName))
        {
            text.AppendLine("# The creature");
            text.AppendLine(Json(CreatureView(draft.Vaesen, step)));
            text.AppendLine();
        }

        // Humans need only the archetype the conflict chose; the conflict chooses from all of them.
        var chosen = draft.Conflicts.Where(c => c.Id == draft.Packet.Brief.ConflictArchetypeId).ToList();
        if (For(ConflictStep.StepName) || (step == HumansStep.StepName && chosen.Count == 0))
        {
            text.AppendLine("# Conflict archetypes on offer");
            text.AppendLine(Json(draft.Conflicts));
            text.AppendLine();
        }
        else if (step == HumansStep.StepName)
        {
            text.AppendLine("# The conflict archetype chosen");
            text.AppendLine(Json(chosen));
            text.AppendLine();
        }

        var settled = SettledAbout(draft);
        if (settled.Count > 0 && For(ConflictStep.StepName, EscalationStep.StepName, HumansStep.StepName, FactsStep.StepName))
        {
            text.AppendLine("# What is already settled about this place for this Society (keep to it)");
            text.AppendLine(string.Join("\n", settled));
            text.AppendLine();
        }

        var locals = draft.World?.RosterFor(draft.Level.Id) is null ? [] : draft.World.PeopleOf(draft.Level.Id).Where(c => c.Status is CharacterStatuses.Alive or CharacterStatuses.Absent).ToList();
        if (locals.Count > 0 && For(HumansStep.StepName))
        {
            text.AppendLine("# The people of this place (this Society's world: the same people every case here)");
            text.AppendLine(Json(locals.Select(c => new
            {
                id = c.Id,
                name = c.Name,
                role = c.RosterRole,
                place = c.HomePlaceId,
                c.Identity.Age,
                c.Identity.Occupation,
                who = c.Background,
                status = c.Status == CharacterStatuses.Alive ? null : c.Status,
                attitudeToSociety = c.Attitude == 0 ? (int?)null : c.Attitude,
                remembers = c.Memories.Count == 0 ? null : c.Memories.TakeLast(2).Select(m => $"{m.When}: {m.Text}"),
                ties = c.Relationships.Count == 0 ? null : c.Relationships.Select(r => $"{r.CharacterId}: {r.Nature}"),
            })));
            text.AppendLine();
        }

        if (For(HumansStep.StepName))
        {
            text.AppendLine("# Persistent characters who may be drawn in (the Society's world, most relevant to this place first)");
            text.AppendLine(Json(roster));
            if (others.Count > 0)
            {
                text.AppendLine();
                text.AppendLine("# Others in the Society's world (one line each; they may still be drawn in by id)");
                text.AppendLine(string.Join("\n", others));
            }

            if (gone.Count > 0)
            {
                text.AppendLine();
                text.AppendLine("# Dead or gone (must not appear; may be remembered)");
                text.AppendLine(string.Join("; ", gone.Select(c => $"{c.Name} ({c.Id}): {c.Status}")));
            }
        }

        if (overlay.Count > 0 && For(ConflictStep.StepName, HumansStep.StepName))
        {
            text.AppendLine();
            text.AppendLine("# What the Society's past mysteries have changed in the world (true for this Society only)");
            foreach (var entry in overlay)
            {
                text.AppendLine($"- {(entry.LevelId is null ? "" : entry.LevelId + ": ")}{entry.Text}");
            }
        }

        return text.ToString();
    }

    /// <summary>
    /// What the Society's other cases on this level settled about it (fixes 4, section 12.2): public facts explained by
    /// cases played (the overlay) or queued, and for a return visit the cases before.
    /// </summary>
    private static List<string> SettledAbout(MysteryDraft draft)
    {
        var lines = draft.SettledPublicFacts
            .Select(s => $"- {s.PublicFactId} ({s.Fact}) is explained{(s.Played ? "" : " in a case not yet played")}: {s.Explanation}")
            .ToList();
        if (draft.Brief.ReturnVisit && draft.Brief.PreviousMysteryIds.Count > 0)
        {
            lines.Insert(0, $"- This is a return visit: the Society has been here before ({string.Join(", ", draft.Brief.PreviousMysteryIds)}). Its people remember them, and time has passed.");
        }

        return lines;
    }

    /// <summary>
    /// The level as a step needs it. Only minds and channels place things in rooms and furniture, so only they (and the
    /// critic) get the affordances, as one line each; the others get the places' names and descriptions.
    /// </summary>
    private static object LevelView(LevelDefinition level, string? step)
    {
        if (step is null or CriticStep.Name)
        {
            return level;
        }

        var withAffordances = step == MindsStep.StepName;
        var dwellingsOnly = step is HumansStep.StepName or ResidentsStep.StepName;
        return new
        {
            level.Id,
            level.Name,
            level.Region,
            level.SettlementType,
            level.Terrain,
            level.Description,
            level.SeasonNotes,
            level.Travel,
            publicFacts = level.PublicFactsWithIds.Select(f => $"{f.Id}: {f.Text}"),
            places = level.Places.Select(p => new
            {
                p.Id,
                p.Name,
                ownerRole = string.IsNullOrWhiteSpace(p.OwnerRole) ? null : p.OwnerRole,
                p.Description,
                p.Tags,
                affordances = withAffordances ? Affordances(p.Affordances).ToList()
                    : dwellingsOnly ? p.AllAffordances().Where(a => Dwellings.Tags.Contains(a.Tag)).Select(a => $"{a.Id} ({a.Tag}): {a.Name}").ToList()
                    : null,
            }),
        };
    }

    /// <summary>"mill-attic (attic): Grain loft", nested ones after their container, "(lockable)" where it applies.</summary>
    private static IEnumerable<string> Affordances(IEnumerable<LevelAffordance> affordances) => affordances.SelectMany(a =>
        Affordances(a.Contains).Prepend($"{a.Id} ({a.Tag}): {a.Name}{(a.Lockable ? " (lockable)" : "")}"));

    /// <summary>
    /// The creature as a step needs it: the conflict, the facts and the critic see all of it; escalation the signs it
    /// shows; humans what folk believe; minds and the arc the lore, weaknesses and how it is faced.
    /// </summary>
    private static object CreatureView(VaesenDefinition v, string? step) => step switch
    {
        EscalationStep.StepName => new { v.Id, v.Name, v.Summary, v.Domain, v.Seasonality, v.CommonKnowledge, v.GiveawayTells, v.AmbiguousSigns },
        HumansStep.StepName => new { v.Id, v.Name, v.Summary, v.Domain, v.ReasonModel, v.Seasonality, v.CommonKnowledge },
        MindsStep.StepName or ArcStep.StepName => new
        {
            v.Id, v.Name, v.Summary, v.Domain, v.ResolutionShape, v.Seasonality, v.CommonKnowledge, v.DeepLore, v.ConfrontationConditions, v.Weaknesses, v.Protections,
        },
        _ => v,
    };

    /// <summary>
    /// The first names the Society's world already holds (fixes 3, section 7), for the humans step: a new significant
    /// person must not echo one of them across cases.
    /// </summary>
    /// <param name="levelId">The level of the case: the people of other levels do not count (fixes 4, section 12.1).</param>
    public static string UsedNames(SocietyWorld? world, string levelId)
    {
        var names = (world?.NamesThatMatter(levelId) ?? [])
            .Select(c => Validation.NameRules.FirstName(c.Name))
            .Where(n => n.Length > 0)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Order(StringComparer.Ordinal)
            .ToList();
        return names.Count == 0
            ? ""
            : $"- First names already used in the Society's world: {string.Join(", ", names)}. Do not give a new significant character a first name on this list, or one that begins with the same three letters. Common names may still be used for residents and the dead.";
    }

    /// <summary>How many persistent characters a case sees in full; the rest get one line each (fixes, section 10).</summary>
    public const int FullDetailCast = 8;

    private static readonly HashSet<string> CommonWords = new(StringComparer.OrdinalIgnoreCase)
    {
        "with", "from", "below", "above", "near", "lake", "river", "manor", "northern", "southern", "eastern", "western",
        "north", "south", "east", "west", "plain", "village", "town", "farm", "crofts", "their", "this", "that", "upper", "lower",
    };

    /// <summary>
    /// How plausibly a persistent character belongs in a case at <paramref name="level"/>: a home there, network links
    /// to it, hooks that name it, or a part in the Society's last two mysteries.
    /// </summary>
    public static int Relevance(CharacterInstance c, LevelDefinition level, IReadOnlySet<string> recentMysteryIds)
    {
        var words = (level.Name + " " + level.Region + " " + string.Join(" ", level.Places.Select(p => p.Name)))
            .Split([' ', ',', '.', ';', ':', '(', ')', '-', '\''], StringSplitOptions.RemoveEmptyEntries)
            .Where(w => w.Length >= 4 && !CommonWords.Contains(w))
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        bool Mentions(string? text) => !string.IsNullOrEmpty(text) && words.Any(w => text.Contains(w, StringComparison.OrdinalIgnoreCase));

        var score = 0;
        if (Mentions(c.Home) || Mentions(c.Identity.Home))
        {
            score += 3;
        }

        score += 2 * c.NetworkLinks.Count(l => Mentions(l.Place));
        if (c.LatentHooks.Exists(h => Mentions(h.Text)))
        {
            score += 1;
        }

        if (c.Memories.Exists(m => m.MysteryId is not null && recentMysteryIds.Contains(m.MysteryId)))
        {
            score += 1;
        }

        return score;
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

    public static string GiveawayRuleForChannels(AssignmentBrief brief) => brief.Focus == CaseFocus.What
        ? "Identity is the puzzle: every channel to a giveaway fact is gated on other discoveries (knowsFact)."
        : "Identity is not the puzzle here; giveaway facts may be found early.";

    public static string GiveawayRuleForHook(AssignmentBrief brief) => brief.Focus == CaseFocus.What
        ? "Identity is the puzzle: the hook states no giveaway fact and reports no event that is one."
        : "Identity is not the puzzle here; the hook may already hint at what the creature is.";

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
        ? "solveDepth is exactly one of: crude, partial, deep. There is a hidden hand, so include a crude solve that is always available (deal with the creature by force) and a deep solve (expose the hand, lift the curse, save the victim), the better outcome, which must be discoverable."
        : vaesen.ResolutionShape == ResolutionShapes.Negotiable
            ? "The creature's grievance can be answered. solveDepth is exactly one of: crude, partial, deep. The lever that answers the grievance at its root is deep. A lever that eases it or drives the creature off without answering the cause is partial. One that simply defeats or destroys is crude. Include at least one deep lever and at least one cruder one."
            : "solveDepth is exactly one of: crude, partial, deep. The creature cannot be bargained with: the levers are about driving it off, ending the provocation or protecting its victims.";

    /// <summary>When the arc writes the creature's mind (fixes 3, section 10): always for one that can be bargained with.</summary>
    public static string CreatureMindRule(VaesenDefinition vaesen) => vaesen.ResolutionShape == ResolutionShapes.Negotiable
        ? $"{vaesen.Name} can be bargained with: write its mind as one entry in creatureMinds (vaesenId {vaesen.Id}). What it wants; its terms, what it would accept to be appeased, which must agree with the deep lever's deeds (list those deed ids in termDeedIds); its red lines; the facts it knows (source witnessed) and what it believes, true or false; its voice; and how it answers an offering, a threat, steel, a prayer, a bargain, an apology and a lie."
        : $"If {vaesen.Name} can be spoken to, write its mind as one entry in creatureMinds (vaesenId {vaesen.Id}): wants, terms, red lines, what it knows and believes, its voice, and how it answers an offering, a threat, steel, a prayer, a bargain, an apology and a lie. If it cannot be spoken to, leave creatureMinds empty.";

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

    /// <summary>The case so far, then the instruction (a patch call has its own problems and answer in it).</summary>
    public static string WithCase(string instruction, object? soFar) => soFar is null ? instruction : $"# The case so far\n{Json(soFar)}\n\n{instruction}";

    /// <summary>The case so far (the step's view of it), and on a retry the issues to fix and the previous answer.</summary>
    public static string WithCaseAndFeedback(string instruction, object? soFar, IReadOnlyList<CoherenceIssue> feedback, string? previousJson) =>
        WithFeedback(WithCase(instruction, soFar), feedback, previousJson);

    /// <summary>The instruction of a fix call: the step's last answer, changed only where the issues ask.</summary>
    public static string Revise(string step) =>
        $"# Revise: {step}\nYour answer to the {step} step passed, but a review found the problems below. Answer again with the same JSON, changed only where the problems ask: keep every other item, id and word exactly as it was.";

    /// <summary>The instruction, and on a retry the issues to fix and the previous answer.</summary>
    public static string WithFeedback(string instruction, IReadOnlyList<CoherenceIssue> feedback, string? previousJson)
    {
        var text = new StringBuilder();
        text.AppendLine(instruction);
        if (feedback.Count > 0)
        {
            text.AppendLine();
            text.AppendLine("# Your last answer to this step had these problems. Fix them and keep what worked.");
            foreach (var group in feedback.GroupBy(i => (i.Severity, i.Code, i.Explanation, i.SuggestedFix)))
            {
                var (severity, code, explanation, fix) = group.Key;
                text.AppendLine($"- [{severity}] {code} at {string.Join(", ", group.Select(i => i.Location).Distinct())}: {explanation}{(fix is null ? "" : $" Fix: {fix}")}");
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
