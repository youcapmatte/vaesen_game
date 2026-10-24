using System.Globalization;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Validation;

/// <summary>What the case is checked against: the canon it was generated from and the Society's persistent cast.</summary>
public sealed class ValidationContext
{
    public required ContentPack Pack { get; init; }

    public required LevelDefinition Level { get; init; }

    public required List<VaesenDefinition> Vaesen { get; init; }

    public ConflictArchetype? Conflict { get; init; }

    /// <summary>Persistent characters this Society has lost (dead or departed): they must not appear.</summary>
    public HashSet<string> UnavailableCharacterIds { get; init; } = [];

    /// <summary>Character ids the Society's persistent cast holds besides the canonical roster.</summary>
    public HashSet<string> SocietyCharacterIds { get; init; } = [];

    /// <summary>The Society's persistent characters (id, name), whose first names a new significant person must not echo.</summary>
    public List<(string Id, string Name)> WorldNames { get; init; } = [];

    /// <summary>
    /// From the Society's roster of the level (fixes 4, section 12.1): who holds each place (its keeper, owner, vicar or
    /// wise woman), by place id. A case may not give the place someone else.
    /// </summary>
    public Dictionary<string, (string Id, string Name)> Holders { get; init; } = [];

    /// <summary>The level's public facts the Society's other cases there already explain (fixes 4, section 12.2).</summary>
    public List<Generation.SettledPublicFact> SettledPublicFacts { get; init; } = [];

    /// <summary>
    /// Parent and child pairs the Society's world records (fixes 5, section 7), as "a|b" with the ids in order: a son's
    /// patronymic that echoes his father's name is expected, not a mix-up.
    /// </summary>
    public HashSet<string> ParentChild { get; init; } = [];

    /// <summary>The key of a pair in <see cref="ParentChild"/>.</summary>
    public static string PairKey(string a, string b) => string.CompareOrdinal(a, b) < 0 ? $"{a}|{b}" : $"{b}|{a}";

    /// <summary>The most facts a lever may require (review 2, item 8): a voice session cannot hold more.</summary>
    public int MaxLeverFacts { get; init; } = 8;
}

/// <summary>
/// How much of the case exists yet: each generation step is checked on what it has produced so far, and the checks
/// that need the whole case (reachability, redundancy, fair play) run once the hook is written.
/// </summary>
[Flags]
public enum CheckScope
{
    Conflict = 1,
    Escalation = 2,
    Cast = 4,

    /// <summary>Sub-step 6a: the facts, decoy threads and which facts the events are.</summary>
    Facts = 8,

    /// <summary>Sub-step 6b: what each mind holds, and the channels.</summary>
    Minds = 16,

    /// <summary>Sub-step 6c: beats, climax and resolution levers.</summary>
    Arc = 32,

    /// <summary>Sub-step 6d: the hook and the places used.</summary>
    Hook = 64,

    /// <summary>The checks on the finished case.</summary>
    Whole = 128,

    /// <summary>The creature's mind, for a creature that can be bargained with (between the arc and the hook).</summary>
    Creature = 256,

    Clues = Facts | Minds | Arc | Creature | Hook | Whole,
    Full = Conflict | Escalation | Cast | Clues,
}

/// <summary>
/// The structural checks of spec 7.10, in C#: reachability, redundancy, order independence, provenance, decoys, fair
/// play, affordance fit, vocabulary and cast coherence, plus the giveaway, opening, resolution and beat rules of
/// 7.4-7.9. Judgement the graph cannot make goes to the critic.
/// </summary>
public sealed partial class CaseValidator : ICoherenceValidator
{
    /// <summary>A group that only talks (or only searches) should still reach this share of the load-bearing facts.</summary>
    public const double EntryStyleCoverage = 0.5;

    /// <summary>Whether a relationship's nature is parent and child ("father", "eldest son"), not an in-law.</summary>
    public static bool IsParentChild(string nature) => Run.IsParentChild(nature);

    public List<CoherenceIssue> Validate(CasePacket packet, ValidationContext context, CheckScope scope = CheckScope.Full)
    {
        var run = new Run(packet, context);
        if (scope.HasFlag(CheckScope.Conflict))
        {
            run.CheckConflict();
        }

        if (scope.HasFlag(CheckScope.Escalation))
        {
            run.CheckEscalation();
        }

        if (scope.HasFlag(CheckScope.Cast))
        {
            run.CheckCast();
        }

        if (scope.HasFlag(CheckScope.Facts))
        {
            run.CheckFacts();
        }

        if (scope.HasFlag(CheckScope.Minds))
        {
            run.CheckChannels();
            run.CheckProvenance();
            run.CheckHomes();
            run.CheckMindsPlayable();
            run.CheckSourcesPlayable();
        }

        if (scope.HasFlag(CheckScope.Arc))
        {
            run.CheckArc();
        }

        if (scope.HasFlag(CheckScope.Creature))
        {
            run.CheckCreatureMinds();
        }

        if (scope.HasFlag(CheckScope.Hook))
        {
            run.CheckHook();
            run.CheckGeography();
            run.CheckHouseNames();
            run.CheckSceneLeaks();
            run.CheckHookNames();
        }

        if (scope.HasFlag(CheckScope.Whole))
        {
            run.CheckLevel();
            run.CheckReachability();
            run.CheckAftermath();
            run.CheckHookGiver();
            run.CheckOneHookGiver();
            run.CheckCitedFacts();
            run.CheckHookGiverKnows();
            run.CheckBeatProvenance();
            run.CheckBeatsDoNotSolve();
            run.CheckThreatsBelieved();
            run.CheckRedundancy();
            run.CheckOrderIndependence();
            run.CheckDecoysAndFairPlay();
            run.CheckDiscoverableSolves();
            run.CheckBeatsAndTension();
        }

        return run.Issues;
    }

    private sealed partial class Run(CasePacket packet, ValidationContext context)
    {
        private readonly FactGraph _graph = new(packet);
        private WalkResult? _walk;

        public List<CoherenceIssue> Issues { get; } = [];

        private WalkResult FullWalk => _walk ??= _graph.Walk();

        private IEnumerable<CaseFact> LoadBearing => packet.Facts.Where(f => f.LoadBearing);

        private VaesenDefinition? Creature => context.Vaesen.FirstOrDefault();

        private void Error(string code, string at, string text, string? fix = null) => Issues.Add(CoherenceIssue.Error(code, at, text, fix));

        private void Warn(string code, string at, string text, string? fix = null) => Issues.Add(CoherenceIssue.Warning(code, at, text, fix));

        /// <summary>A warning naming the ids it is about, so the last round can send it to the step that owns them.</summary>
        private void WarnOn(List<string> targets, string code, string at, string text, string fix)
        {
            var issue = CoherenceIssue.Warning(code, at, text, fix);
            issue.Targets = targets;
            Issues.Add(issue);
        }

        // ---- Conflict step ------------------------------------------------------------------------------------

        /// <summary>Explained public facts are the level's, and not ones another case already explains (fixes 4, 12.2).</summary>
        private void CheckPublicFacts()
        {
            var ids = context.Level.PublicFactsWithIds.Select(f => f.Id).ToHashSet(StringComparer.Ordinal);
            foreach (var e in packet.PublicFactsExplained)
            {
                if (!ids.Contains(e.PublicFactId))
                {
                    Error(IssueCodes.UnknownReference, "publicFactsExplained", $"'{e.PublicFactId}' is not one of the level's public facts.",
                        $"Use the id of a public fact (pf1 to pf{ids.Count}), or leave it out.");
                }
                else if (context.SettledPublicFacts.Find(s => s.PublicFactId == e.PublicFactId) is { } settled)
                {
                    Warn(IssueCodes.Consistency, "publicFactsExplained",
                        $"{e.PublicFactId} is already explained for this Society by {settled.MysteryId}{(settled.Played ? "" : " (queued)")}: {settled.Explanation}",
                        "Keep to that explanation in the truth, and drop it from publicFactsExplained.");
                }
            }
        }

        /// <summary>
        /// A place the Society's roster gives a keeper or owner keeps them (fixes 4, section 12.1): a case may promote,
        /// kill or move them as its own events, but not replace them with a new person.
        /// </summary>
        private void CheckHolders()
        {
            foreach (var member in packet.Cast.Where(c => c.Keeps is not null))
            {
                if (context.Level.FindPlace(member.Keeps) is not { } place)
                {
                    Error(IssueCodes.UnknownReference, $"cast:{member.CharacterId}", $"{member.Name} keeps '{member.Keeps}', which is no place of the level.");
                }
                else if (context.Holders.TryGetValue(place.Id, out var holder) && holder.Id != member.CharacterId)
                {
                    Error(IssueCodes.CastCoherence, $"cast:{member.CharacterId}",
                        $"{member.Name} is given as the holder of {place.Name}, but in this Society's world {holder.Name} ({holder.Id}) holds it.",
                        $"Cast {holder.Name} (instanceRef persistent, id {holder.Id}) in that part, or make {member.Name} someone who does not hold {place.Name}.");
                }
            }

            foreach (var member in packet.Cast.Where(c => c.InstanceRef is InstanceRefs.Persistent or InstanceRefs.Promoted))
            {
                var known = context.WorldNames.Find(w => w.Id == member.CharacterId);
                if (known.Name is { Length: > 0 } name && !string.Equals(name, member.Name, StringComparison.Ordinal))
                {
                    Error(IssueCodes.CastCoherence, $"cast:{member.CharacterId}", $"{member.CharacterId} is {name} in the Society's world, not {member.Name}.",
                        $"Use the name {name}; a persistent character keeps their name.");
                }
            }
        }

        public void CheckConflict()
        {
            CheckPublicFacts();
            var truth = packet.Truth;
            var conflict = context.Conflict;
            if (conflict is null)
            {
                Error(IssueCodes.UnknownReference, "brief", $"Unknown conflict archetype '{packet.Brief.ConflictArchetypeId}'.",
                    "Use one of the conflict archetype ids offered in the brief.");
            }
            else if (Creature is not null && !conflict.Tags.Intersect(Creature.CompatibleConflictTags).Any())
            {
                Error(IssueCodes.AffordanceFit, "brief", $"Conflict '{conflict.Id}' does not suit {Creature.Name}.");
            }

            if (!ReasonKinds.All.Contains(truth.ReasonKind))
            {
                Error(IssueCodes.Schema, "truth.reasonKind", $"Unknown reason kind '{truth.ReasonKind}'.{CoherenceIssue.Allowed(ReasonKinds.All)}");
            }

            if (!CaseFocus.All.Contains(truth.Focus))
            {
                Error(IssueCodes.Schema, "truth.focus", $"Unknown focus '{truth.Focus}'.{CoherenceIssue.Allowed(CaseFocus.All)}");
            }

            if (!PlotCentreKinds.All.Contains(truth.PlotCentre.Kind))
            {
                Error(IssueCodes.Schema, "truth.plotCentre", $"Unknown plot centre kind '{truth.PlotCentre.Kind}'.{CoherenceIssue.Allowed(PlotCentreKinds.All)}");
            }

            if (string.IsNullOrWhiteSpace(truth.Provocation) || string.IsNullOrWhiteSpace(truth.Summary))
            {
                Error(IssueCodes.Schema, "truth", "The conflict needs a provocation and a summary of the truth.");
            }

            // The provocation must physically exist in the level (no forest, no logging).
            var place = context.Level.FindPlace(truth.ProvocationPlaceId);
            if (place is null)
            {
                Error(IssueCodes.AffordanceFit, "truth.provocationPlaceId", $"The provocation's place '{truth.ProvocationPlaceId}' is not in the level.",
                    "Put the provocation in one of the level's places.");
            }

            if (!context.Pack.Vocabulary.Contains(truth.ProvocationAffordanceTag))
            {
                Error(IssueCodes.Vocabulary, "truth.provocationAffordanceTag", $"'{truth.ProvocationAffordanceTag}' is not in the vocabulary.");
            }
            else
            {
                var offered = context.Pack.Vocabulary.ExpandAll(place is null
                    ? context.Level.AllTags()
                    : place.AllTags().Concat(context.Level.Terrain));
                if (!offered.Contains(truth.ProvocationAffordanceTag))
                {
                    Error(IssueCodes.AffordanceFit, "truth.provocationAffordanceTag",
                        $"The provocation needs '{truth.ProvocationAffordanceTag}', which {(place is null ? "the level" : place.Name)} does not offer.");
                }

                if (conflict is not null && conflict.ProvocationAffordances.Count > 0
                    && !context.Pack.Vocabulary.Expand(truth.ProvocationAffordanceTag).Overlaps(conflict.ProvocationAffordances))
                {
                    Warn(IssueCodes.AffordanceFit, "truth.provocationAffordanceTag",
                        $"'{truth.ProvocationAffordanceTag}' is not among the provocation affordances of '{conflict.Id}'.");
                }
            }

            if (truth.PlotCentre.Kind == PlotCentreKinds.HiddenHand && string.IsNullOrWhiteSpace(truth.HiddenHandMotive))
            {
                Error(IssueCodes.Schema, "truth.hiddenHandMotive", "A hidden-hand case must fix the hand's motive in the conflict step.");
            }
        }

        // ---- Escalation step ----------------------------------------------------------------------------------

        public void CheckEscalation()
        {
            var events = packet.Truth.Escalation;
            if (events.Count < 3)
            {
                Error(IssueCodes.Opening, "truth.escalation", "Escalation needs at least three concrete events.");
            }

            foreach (var duplicate in events.GroupBy(e => e.Id).Where(g => g.Count() > 1))
            {
                Error(IssueCodes.Schema, $"event:{duplicate.Key}", "Escalation event id is used twice.");
            }

            foreach (var e in events)
            {
                if (e.PlaceId is not null && context.Level.FindPlace(e.PlaceId) is null)
                {
                    Error(IssueCodes.AffordanceFit, $"event:{e.Id}", $"Place '{e.PlaceId}' is not in the level.");
                }

                if (!SignRegisters.All.Contains(e.Register))
                {
                    Error(IssueCodes.Schema, $"event:{e.Id}", $"Unknown register '{e.Register}'.{CoherenceIssue.Allowed(SignRegisters.All)}");
                }
            }

            var opening = events.Where(e => e.Opening).ToList();
            var needed = packet.Truth.Focus == CaseFocus.What ? 3 : 2;
            if (opening.Count < needed)
            {
                Error(IssueCodes.Opening, "truth.escalation",
                    $"The opening needs a convergence of at least {needed} peculiar events; it has {opening.Count}.",
                    "Mark more events as opening, each deniable alone, inexplicable together.");
            }

            if (opening.Select(e => e.Register).Distinct().Count() < Math.Min(2, opening.Count))
            {
                Warn(IssueCodes.Opening, "truth.escalation", "The opening events are all in one register; spread them (animals, weather, health, sounds...).");
            }
        }

        // ---- Humans step --------------------------------------------------------------------------------------

        public void CheckCast()
        {
            CheckHolders();
            CheckRecordsPlayable();
            CheckPatronymics();
            if (packet.Cast.Count < 3)
            {
                Error(IssueCodes.CastCoherence, "cast", "A case needs at least three cast members.");
            }

            foreach (var duplicate in packet.Cast.GroupBy(c => c.CharacterId).Where(g => g.Count() > 1))
            {
                Error(IssueCodes.CastCoherence, $"cast:{duplicate.Key}", "Cast member id is used twice.");
            }

            foreach (var member in packet.Cast)
            {
                var at = $"cast:{member.CharacterId}";
                if (!InstanceRefs.All.Contains(member.InstanceRef))
                {
                    Error(IssueCodes.Schema, at, $"Unknown instance ref '{member.InstanceRef}'.{CoherenceIssue.Allowed(InstanceRefs.All)}");
                }

                if (!CastImportance.All.Contains(member.Importance))
                {
                    Error(IssueCodes.Schema, at, $"Unknown importance '{member.Importance}'.{CoherenceIssue.Allowed(CastImportance.All)}");
                }

                if (member.InstanceRef is InstanceRefs.Persistent or InstanceRefs.Promoted)
                {
                    if (context.Pack.FindCharacter(member.CharacterId) is null && !context.SocietyCharacterIds.Contains(member.CharacterId))
                    {
                        Error(IssueCodes.CastCoherence, at, $"Marked {member.InstanceRef}, but no such character exists in the Society's world.",
                            "Use a persistent character id from the list offered, or mark them new.");
                    }
                }
                else if (context.SocietyCharacterIds.Contains(member.CharacterId) || context.Pack.FindCharacter(member.CharacterId) is not null)
                {
                    Error(IssueCodes.CastCoherence, at, "A new character cannot take the id of someone already in the Society's world.",
                        "Mark them persistent to bring that person in, or give the new person their own id.");
                }
                else if (member.Identity is null || string.IsNullOrWhiteSpace(member.Identity.Occupation))
                {
                    Error(IssueCodes.CastCoherence, at, "A new character needs an identity (age, occupation, appearance...).");
                }

                if (member.Importance == CastImportance.Significant && member.InstanceRef == InstanceRefs.New)
                {
                    foreach (var (id, name) in context.WorldNames.Where(w => w.Id != member.CharacterId))
                    {
                        if (NameRules.FirstNameClash(member.Name, name) is { } why)
                        {
                            // An error at the humans step, fixed by a patch before any later step writes the name into prose.
                            Error(IssueCodes.CastCoherence, at,
                                $"{member.Name} and {name}, already in the Society's world, have {why}.",
                                $"Rename {member.Name}, keeping the id. Avoid these first names and any that begins with the same three letters: {string.Join(", ", context.WorldNames.Select(w => NameRules.FirstName(w.Name)).Where(n => n.Length > 0).Distinct())}; and keep clear of the other names in this case.");
                            break;
                        }
                    }
                }

                if (member.Importance == CastImportance.Significant && string.IsNullOrWhiteSpace(member.Handle))
                {
                    Warn(IssueCodes.CastCoherence, at, $"{member.Name} has no handle: give a short one players can use instead of the name (\"the widow\").");
                }

                if (context.UnavailableCharacterIds.Contains(member.CharacterId))
                {
                    Error(IssueCodes.CastCoherence, at, "This character is dead or gone for this Society and cannot appear.");
                }

                if (member.HomePlaceId is not null && context.Level.FindPlace(member.HomePlaceId) is null
                    && member.InstanceRef == InstanceRefs.New)
                {
                    Error(IssueCodes.AffordanceFit, at, $"Home place '{member.HomePlaceId}' is not in the level.");
                }

                foreach (var action in member.Wants.PermittedActions.Where(a => !DesperationTiers.All.Contains(a.Desperation)))
                {
                    Error(IssueCodes.Schema, at, $"Action '{action.Id}' has unknown desperation '{action.Desperation}'.{CoherenceIssue.Allowed(DesperationTiers.All)}");
                }

                // Ties may reach anyone in the Society's world, not only this case's cast.
                foreach (var rel in member.Relationships.Where(r => packet.FindCast(r.CharacterId) is null && !packet.OffstagePeople.Exists(o => o.Id == r.CharacterId)
                    && !context.SocietyCharacterIds.Contains(r.CharacterId) && context.Pack.FindCharacter(r.CharacterId) is null))
                {
                    // A tie to no one is a placeholder the runtime cannot follow (review 3, item 5): an error, fixed by a patch.
                    Error(IssueCodes.UnknownReference, at, $"Relationship to '{rel.CharacterId}', who is not in the cast or the Society's world.",
                        "Point the relationship at someone in the cast, the offstage people or the Society's world by their id, or drop it.");
                }
            }

            if (packet.Truth.PlotCentre.Kind == PlotCentreKinds.HiddenHand)
            {
                var hand = packet.FindCast(packet.Truth.PlotCentre.CharacterId);
                if (hand is null)
                {
                    Error(IssueCodes.CastCoherence, "truth.plotCentre", "The hidden hand is not in the cast.", "Cast the hidden hand and point plotCentre.characterId at them.");
                }
                else if (hand.Importance != CastImportance.Significant)
                {
                    Error(IssueCodes.CastCoherence, "truth.plotCentre", "The hidden hand must be a significant character.");
                }
            }

            if (packet.Cast.Count(c => c.Importance == CastImportance.Resident) < 2)
            {
                Warn(IssueCodes.CastCoherence, "cast", "Few location residents: add ordinary people unconnected to the case for colour.");
            }

            var conflict = context.Conflict;
            if (conflict is not null)
            {
                foreach (var slot in conflict.RoleSlots.Where(s => s.Required && !packet.Cast.Exists(c => c.RoleSlotId == s.Id)))
                {
                    Warn(IssueCodes.CastCoherence, $"conflict:{conflict.Id}", $"Nobody fills the required role '{slot.Name}' ({slot.Id}).");
                }
            }

            var ranks = context.Pack.Setting.PeriodConventions.SocialRanks;
            foreach (var member in packet.Cast.Where(c => c.Identity is { Rank.Length: > 0 } && ranks.Count > 0))
            {
                if (!ranks.Contains(member.Identity!.Rank, StringComparer.OrdinalIgnoreCase))
                {
                    Warn(IssueCodes.CastCoherence, $"cast:{member.CharacterId}", $"{member.Name}'s rank '{member.Identity.Rank}' is not one of the setting's.{CoherenceIssue.Allowed(ranks)}");
                }
            }

            CheckNames();
        }

        /// <summary>
        /// Names heard by voice must not be confused (fixes 2, section 6): every named person, the cast and the people
        /// the truth and the events mention, against every other. No shared first name or first syllable, and no surname
        /// that contains another's first name ("Per Ers" against "Jon Persson").
        /// </summary>
        private void CheckNames()
        {
            var people = packet.Cast.Select(c => (Name: c.Name, At: $"cast:{c.CharacterId}"))
                .Concat(packet.OffstagePeople.Select(o => (Name: o.Name, At: "offstagePeople")))
                .ToList();
            var texts = new[] { packet.Truth.Summary, packet.Truth.Provocation, packet.Truth.HiddenHandMotive ?? "", packet.Truth.DecoyReading ?? "" }
                .Concat(packet.Truth.Escalation.Select(e => e.Description));
            foreach (var mentioned in texts.SelectMany(NameRules.Mentioned).Distinct(StringComparer.Ordinal))
            {
                if (!people.Exists(p => p.Name.Contains(mentioned, StringComparison.Ordinal)))
                {
                    people.Add((mentioned, "truth"));
                }
            }

            for (var i = 0; i < people.Count; i++)
            {
                for (var j = i + 1; j < people.Count; j++)
                {
                    // A first name in another's surname is a warning: it may be a patronymic of the period (Eriksdotter).
                    if (NameRules.Clash(people[i].Name, people[j].Name) is null && NameRules.SurnameEcho(people[i].Name, people[j].Name) is { } echo
                        && !Kin(people[i].At, people[j].At))
                    {
                        Warn(IssueCodes.CastCoherence, people[j].At.StartsWith("cast:", StringComparison.Ordinal) ? people[j].At : people[i].At,
                            $"'{people[i].Name}' and '{people[j].Name}': {echo}. Fine for a parent and child; otherwise players may mix them up.");
                    }

                    if (NameRules.Clash(people[i].Name, people[j].Name) is { } why)
                    {
                        // The later cast member is renamed (the first may be a persistent character or a name in the truth).
                        var at = people[j].At.StartsWith("cast:", StringComparison.Ordinal) ? people[j].At : people[i].At;
                        Error(IssueCodes.CastCoherence, at, $"'{people[i].Name}' and '{people[j].Name}' sound alike ({why}); players will mix them up.",
                            $"Rename {(at == people[j].At ? people[j].Name : people[i].Name)}, keeping the id: no two people share a first name or first syllable, and no surname holds another's first name.");
                    }
                }
            }
        }

        // ---- Sub-step 6a: facts -------------------------------------------------------------------------------

        public void CheckFacts()
        {
            foreach (var duplicate in packet.Facts.GroupBy(f => f.Id).Where(g => g.Count() > 1))
            {
                Error(IssueCodes.Schema, $"fact:{duplicate.Key}", "Fact id is used twice.");
            }

            foreach (var fact in packet.Facts)
            {
                if (!FactKinds.All.Contains(fact.Kind))
                {
                    Error(IssueCodes.Schema, $"fact:{fact.Id}", $"Unknown fact kind '{fact.Kind}'.{CoherenceIssue.Allowed(FactKinds.All)}");
                }

                if (fact.PointsTo is not (FactPointsTo.TrueAnswer or FactPointsTo.Neutral) && !FactPointsTo.IsDecoy(fact.PointsTo))
                {
                    Error(IssueCodes.Schema, $"fact:{fact.Id}", $"pointsTo must be trueAnswer, neutral or decoy:<threadId>, not '{fact.PointsTo}'.");
                }
            }

            if (!LoadBearing.Any(f => f.Kind == FactKinds.Identity && f.PointsTo == FactPointsTo.TrueAnswer))
            {
                Error(IssueCodes.Schema, "facts", "No load-bearing identity fact points to the true answer, so the truth cannot be assembled.");
            }

            if (!LoadBearing.Any(f => f.DeepLore && f.PointsTo == FactPointsTo.TrueAnswer))
            {
                Error(IssueCodes.Giveaway, "facts", "No load-bearing fact of the solution rests on deep lore; the trail is the obvious answer.",
                    "Build the solution trail from the creature's deep lore (list its deep lore ids in loreIds) and use common knowledge as misdirection.");
            }

            var knownLore = context.Vaesen.SelectMany(v => v.AllLore.Select(l => l.Id).Concat(v.AmbiguousSigns.Select(s => s.Id)).Concat(v.GiveawayTells.Select(s => s.Id))).ToHashSet();
            foreach (var fact in packet.Facts)
            {
                foreach (var loreId in fact.LoreIds.Where(id => !knownLore.Contains(id)))
                {
                    Warn(IssueCodes.UnknownReference, $"fact:{fact.Id}", $"Lore id '{loreId}' is not in the creature's lore.");
                }
            }

            foreach (var e in packet.Truth.Escalation)
            {
                foreach (var id in e.FactIds)
                {
                    Fact(id, $"event:{e.Id}");
                }
            }

            foreach (var thread in packet.SeasonThreads)
            {
                foreach (var id in thread.FactIds)
                {
                    Fact(id, $"seasonThread:{thread.ThreadId}");
                }
            }

            var threadIds = packet.Decoys.Select(d => d.ThreadId).ToHashSet(StringComparer.Ordinal);
            foreach (var fact in packet.Facts.Where(f => FactPointsTo.IsDecoy(f.PointsTo) && !threadIds.Contains(FactPointsTo.DecoyThreadId(f.PointsTo)!)))
            {
                Error(IssueCodes.Decoy, $"fact:{fact.Id}", $"Points to decoy thread '{FactPointsTo.DecoyThreadId(fact.PointsTo)}', which does not exist.");
            }

            foreach (var decoy in packet.Decoys)
            {
                var at = $"decoy:{decoy.ThreadId}";
                foreach (var id in decoy.FactIds.Concat(decoy.ClearedByFactIds))
                {
                    Fact(id, at);
                }

                if (decoy.FactIds.Count == 0)
                {
                    Error(IssueCodes.Decoy, at, "A decoy thread needs facts that point down it.");
                }

                foreach (var fact in decoy.FactIds.Select(packet.FindFact).OfType<CaseFact>().Where(f => f.PointsTo != FactPointsTo.Decoy(decoy.ThreadId)))
                {
                    Error(IssueCodes.Decoy, at, $"Fact {fact.Id} is listed in the thread but points to '{fact.PointsTo}'.");
                }

                if (decoy.ClearedByFactIds.Count == 0)
                {
                    Error(IssueCodes.Decoy, at, "A decoy must be cleared by a true fact.", "Add a fact that is true from every angle and clears this trail.");
                }

                foreach (var fact in decoy.ClearedByFactIds.Select(packet.FindFact).OfType<CaseFact>().Where(f => FactPointsTo.IsDecoy(f.PointsTo)))
                {
                    Error(IssueCodes.Decoy, at, $"Fact {fact.Id} clears the decoy but is itself decoy evidence.");
                }

                if (decoy.VaesenId is not null && context.Pack.FindVaesen(decoy.VaesenId) is null)
                {
                    Error(IssueCodes.UnknownReference, at, $"Decoy creature '{decoy.VaesenId}' is not in the catalogue.");
                }
            }
        }

        // ---- Sub-step 6b: minds and channels ------------------------------------------------------------------

        public void CheckChannels()
        {
            foreach (var duplicate in packet.Channels.GroupBy(c => c.Id).Where(g => g.Count() > 1))
            {
                Error(IssueCodes.Schema, $"channel:{duplicate.Key}", "Channel id is used twice.");
            }

            foreach (var channel in packet.Channels)
            {
                var at = $"channel:{channel.Id}";
                if (!ChannelKinds.All.Contains(channel.Kind))
                {
                    Error(IssueCodes.Schema, at, $"Unknown channel kind '{channel.Kind}'.{CoherenceIssue.Allowed(ChannelKinds.All)}");
                }

                foreach (var text in channel.Access.Requires.Where(r => AccessRequirement.Parse(r) is null))
                {
                    Error(IssueCodes.Schema, at, $"Cannot read access requirement '{text}'. Allowed: knowsFact:<factId>, trust>=<0-3>, roll:<skill>, time:<when>, place:<placeId>, item:<what>.",
                        "Use knowsFact:<factId>, trust>=<n>, roll:<skill>, time:<when>, place:<placeId> or item:<what>.");
                }

                if (string.IsNullOrWhiteSpace(channel.Reveal))
                {
                    Error(IssueCodes.Schema, at, "A channel needs a reveal: what the players perceive.");
                }

                Fact(channel.FactId, at);
                Person(channel.SourceCharacterId, at);
                foreach (var gate in _graph.FactGates(channel))
                {
                    Fact(gate, at);
                }

                if (channel.Kind is ChannelKinds.Trace or ChannelKinds.Document && string.IsNullOrEmpty(channel.PlaceId))
                {
                    Error(IssueCodes.AffordanceFit, at, "A trace or document must be somewhere: give it a placeId.");
                }

                if (channel.PlaceId is null)
                {
                    continue;
                }

                var place = context.Level.FindPlace(channel.PlaceId);
                if (place is null)
                {
                    Error(IssueCodes.AffordanceFit, at, $"Place '{channel.PlaceId}' is not in the level.");
                }
                else if (channel.AffordanceId is not null && place.FindAffordance(channel.AffordanceId) is null)
                {
                    Error(IssueCodes.AffordanceFit, at, $"'{channel.AffordanceId}' is not an affordance of {place.Name}.",
                        $"Use one of: {string.Join(", ", place.AllAffordances().Select(a => a.Id))}.");
                }
            }

            foreach (var member in packet.Cast)
            {
                foreach (var k in member.Knows)
                {
                    Fact(k.FactId, $"cast:{member.CharacterId}");
                }

                foreach (var lie in member.Lies)
                {
                    Fact(lie.About, $"cast:{member.CharacterId}");
                }
            }

            if (!packet.Channels.Exists(c => c.Kind == ChannelKinds.KnowledgeGated))
            {
                Error(IssueCodes.Giveaway, "channels", "There is no knowledge-gated step.",
                    "Add a sign that only resolves once the group understands the creature, or finds someone who knows the old ways.");
            }

            if (packet.Truth.Focus == CaseFocus.What)
            {
                foreach (var fact in packet.Facts.Where(f => f.Giveaway))
                {
                    foreach (var channel in packet.Channels.Where(c => c.FactId == fact.Id && !_graph.FactGates(c).Any()))
                    {
                        Error(IssueCodes.Giveaway, $"channel:{channel.Id}",
                            "A giveaway tell can be found straight away in an identity case; gate it behind other discoveries.");
                    }
                }
            }
        }

        public void CheckProvenance()
        {
            foreach (var member in packet.Cast)
            {
                foreach (var k in member.Knows)
                {
                    var at = $"cast:{member.CharacterId}";
                    var source = k.Source ?? "";
                    if (source is KnowledgeSources.Witnessed or KnowledgeSources.Self or KnowledgeSources.Lore)
                    {
                        continue;
                    }

                    if (source.StartsWith(KnowledgeSources.OffstagePrefix, StringComparison.Ordinal))
                    {
                        var who = source[KnowledgeSources.OffstagePrefix.Length..];
                        if (!packet.OffstagePeople.Exists(o => o.Id == who || string.Equals(o.Name, who, StringComparison.OrdinalIgnoreCase)))
                        {
                            Error(IssueCodes.Provenance, at, $"Knows {k.FactId}, told by '{who}', who is not among the offstage people.",
                                "Use told:offstage:<id> with an id from offstagePeople, or told:bystander:<role> for anonymous gossip.");
                        }
                    }
                    else if (KnowledgeSources.IsHearsay(source))
                    {
                        if (source.Length == KnowledgeSources.BystanderPrefix.Length)
                        {
                            Error(IssueCodes.Provenance, at, $"Knows {k.FactId} from a bystander with no role.", "Write told:bystander:<role>, such as told:bystander:carter.");
                        }
                    }
                    else if (source.StartsWith(KnowledgeSources.ToldPrefix, StringComparison.Ordinal))
                    {
                        var teller = packet.FindCast(source[KnowledgeSources.ToldPrefix.Length..]);
                        if (teller is null)
                        {
                            Error(IssueCodes.Provenance, at, $"Knows {k.FactId}, told by '{source[KnowledgeSources.ToldPrefix.Length..]}', who is not in the cast.");
                        }
                        else if (!teller.Knows.Exists(t => t.FactId == k.FactId))
                        {
                            // On the teller: the patch then holds the person whose knowledge is missing.
                            Error(IssueCodes.Provenance, $"cast:{teller.CharacterId}", $"{member.Name} knows {k.FactId}, told by {teller.Name}, who does not know it.",
                                $"Give {teller.Name} the knowledge of {k.FactId}, with its own source.");
                        }
                    }
                    else if (source.StartsWith(KnowledgeSources.FoundPrefix, StringComparison.Ordinal))
                    {
                        var channel = packet.FindChannel(source[KnowledgeSources.FoundPrefix.Length..]);
                        if (channel is null || channel.FactId != k.FactId)
                        {
                            Error(IssueCodes.Provenance, at, $"Knows {k.FactId} from '{source}', which is not a channel of that fact.");
                        }
                    }
                    else
                    {
                        Error(IssueCodes.Provenance, at, $"Knows {k.FactId} from an unreadable source '{source}'. Allowed: witnessed, self, lore, told:<characterId>, told:offstage:<id>, told:bystander:<role>, found:<channelId>.",
                            "Use witnessed, self, lore, told:<characterId>, told:offstage:<id>, told:bystander:<role> or found:<channelId>.");
                    }
                }
            }

            // A lie that tells the fact is no lie (fixes 2, section 7).
            foreach (var member in packet.Cast)
            {
                foreach (var lie in member.Lies)
                {
                    if (packet.FindFact(lie.About) is { } fact && NameRules.SaysTheSame(lie.Story, fact.Statement))
                    {
                        Warn(IssueCodes.Provenance, $"cast:{member.CharacterId}", $"{member.Name}'s lie about {fact.Id} says what the fact says, so it is not a lie.",
                            $"Give {member.Name} a story that hides or bends {fact.Id}, or drop the lie.");
                    }
                }
            }

            // A person can only tell what they know: every testimony is backed by a knowledge entry. A trace or document
            // that names a person is theirs (in their home), not something they say.
            foreach (var c in packet.Channels.Where(c => !string.IsNullOrEmpty(c.SourceCharacterId) && c.Kind is not (ChannelKinds.Document or ChannelKinds.Trace)))
            {
                var person = packet.FindCast(c.SourceCharacterId);
                if (person is not null && !person.Knows.Exists(k => k.FactId == c.FactId))
                {
                    Error(IssueCodes.Provenance, $"cast:{person.CharacterId}", $"{person.Name} is the source of channel {c.Id} ({c.FactId}) but does not know {c.FactId}.",
                        $"Add {c.FactId} to {person.Name}'s knows, with how they learned it.");
                }
            }
        }

        /// <summary>
        /// A trace or document in a dwelling is in its owner's own home (fixes 3, section 6): not another person's house,
        /// not a shared building they do not live in. A warning, sent to minds and channels.
        /// </summary>
        public void CheckHomes()
        {
            foreach (var c in packet.Channels.Where(c => c.Kind is ChannelKinds.Trace or ChannelKinds.Document && c.PlaceId is not null && c.AffordanceId is not null))
            {
                if (context.Level.FindPlace(c.PlaceId) is not { } place || Dwelling(place, c.AffordanceId!) is not { } home)
                {
                    continue;
                }

                var owner = packet.FindCast(c.SourceCharacterId);
                var resident = packet.Cast.Find(m => m.HomeAffordanceId == home.Id && m.HomePlaceId == place.Id);
                string? problem = null;
                if (owner is null)
                {
                    problem = resident is null ? null : $"sits in {resident.Name}'s home ({home.Name}) but names no owner";
                }
                else if (owner.HomeAffordanceId is null)
                {
                    problem = resident is not null && resident != owner
                        ? $"puts {owner.Name}'s things in {resident.Name}'s home ({home.Name})"
                        : $"puts {owner.Name}'s things in {home.Name}, which is not said to be their home";
                }
                else if (owner.HomeAffordanceId != home.Id || owner.HomePlaceId != place.Id)
                {
                    problem = $"puts {owner.Name}'s things in {home.Name}, not in their own home ({owner.HomeAffordanceId})";
                }

                if (problem is not null)
                {
                    WarnOn([c.Id], IssueCodes.AffordanceFit, $"channel:{c.Id}", $"Channel {c.Id} {problem}.",
                        owner?.HomeAffordanceId is { } own
                            ? $"Move it to {own} or something inside it."
                            : "Name the owner and place it in their own home; give them a homeAffordanceId, or newDwelling if their place has none.");
                }
            }
        }

        /// <summary>The dwelling an affordance is, or is inside, if any.</summary>
        private static LevelAffordance? Dwelling(LevelPlace place, string affordanceId)
        {
            LevelAffordance? Find(IEnumerable<LevelAffordance> items, LevelAffordance? home)
            {
                foreach (var a in items)
                {
                    var here = Dwellings.Tags.Contains(a.Tag) ? a : home;
                    if (a.Id == affordanceId)
                    {
                        return here;
                    }

                    if (Find(a.Contains, here) is { } found)
                    {
                        return found;
                    }
                }

                return null;
            }

            return Find(place.Affordances, null);
        }

        /// <summary>
        /// The beats and the opening must not hand over the deep solve (fixes 4, section 2): at most half the deep
        /// lever's facts may come from them; the rest must be sought out through channels. An error, sent to the arc.
        /// </summary>
        public void CheckBeatsDoNotSolve()
        {
            var given = _graph.OpeningKnowledge();
            given.UnionWith(packet.Beats.SelectMany(b => b.EmitsFactIds));
            foreach (var lever in packet.ResolutionLevers.Where(l => l.SolveDepth == SolveDepths.Deep && l.RequiresFactIds.Count > 0))
            {
                var free = lever.RequiresFactIds.Where(given.Contains).ToList();
                if (free.Count * 2 > lever.RequiresFactIds.Count)
                {
                    Error(IssueCodes.Beats, "climax",
                        $"The deep solve {lever.Id} needs {string.Join(", ", lever.RequiresFactIds)}; {string.Join(", ", free)} come from the opening or the beats alone, so the case solves itself.",
                        "Let beats and the climax show pressure, not answers: take those facts out of the beats' emitsFactIds, so the players must find most of them through channels.");
                }
            }
        }

        /// <summary>
        /// Someone who lives under a threat believes something about it (fixes 4, section 7): why it is real, and why
        /// the threatener's own exposure does not stop it. A warning, sent to minds and channels.
        /// </summary>
        public void CheckThreatsBelieved()
        {
            string[] words = ["threat", "blackmail", "threaten", "hold over", "expose", "denounce", "inform on"];
            foreach (var member in packet.Cast.Where(m => m.Importance == CastImportance.Significant && m.Thinks.Count == 0))
            {
                var text = string.Join(" ", member.Wants.Drives.Concat(member.Wants.Fears).Concat(member.Secrets)).ToLowerInvariant();
                if (words.Any(text.Contains))
                {
                    WarnOn([$"{member.CharacterId}.thinks"], IssueCodes.CastCoherence, $"cast:{member.CharacterId}",
                        $"{member.Name} lives under a threat but believes nothing about it.",
                        $"Give {member.Name} a belief saying why the threat is real, and, if the threatener would be exposed too, why that does not stop it.");
                }
            }
        }

        /// <summary>
        /// A fact a beat puts into the world needs a channel players can find afterwards (fixes 4, section 3): gossip, a
        /// trace left behind, a witness, gated afterBeat:&lt;beatId&gt;. Otherwise a group not there when it fired never learns it.
        /// </summary>
        /// <summary>The hook-giver is the kind of person the assignment asked for (fixes 4, section 9). A warning, sent to the hook.</summary>
        /// <summary>
        /// One person fetched the Society (review 2, item 3): when the hook moves to someone else, the old giver's record
        /// must stop saying it was them.
        /// </summary>
        public void CheckOneHookGiver()
        {
            foreach (var member in packet.Cast.Where(c => c.CharacterId != packet.Hook.HookGiverId))
            {
                var record = string.Join(" ", new[] { member.PlotRole, member.Routine }.Concat(member.Secrets).Concat(member.Wants.PermittedActions.Select(a => a.Description)));
                if (HookGiverClaim().Match(record) is { Success: true } claim)
                {
                    var giver = packet.FindCast(packet.Hook.HookGiverId);
                    WarnOn([$"{member.CharacterId}.identity", $"{member.CharacterId}.secrets"], IssueCodes.Opening, $"cast:{member.CharacterId}",
                        $"{member.Name}'s record says \"{claim.Value}\", but the hook-giver is {giver?.Name ?? packet.Hook.HookGiverId}. Two people would claim to have fetched the Society.",
                        $"Rewrite {member.Name}'s plotRole, secrets and actions so they no longer bring the Society; keep what else they know and want.");
                }
            }
        }

        [System.Text.RegularExpressions.GeneratedRegex(@"hook[- ]giver|(fetch(ed|es)?|find|found|summon(ed)?|sent for|wrote to|telegraphed|rode to|went to|drove [\w ]{0,20}to find)\s+(the\s+)?Society", System.Text.RegularExpressions.RegexOptions.IgnoreCase)]
        private static partial System.Text.RegularExpressions.Regex HookGiverClaim();

        /// <summary>
        /// Every significant person can be played (review 2, item 4): what drives them, what they fear, their day. A
        /// persistent person drawn in needs them for this case too.
        /// </summary>
        private void CheckRecordsPlayable()
        {
            foreach (var member in packet.Cast)
            {
                var at = $"cast:{member.CharacterId}";
                if (member.Importance == CastImportance.Significant)
                {
                    var missing = new[]
                    {
                        member.Wants.Drives.Count == 0 ? "drives" : null,
                        member.Wants.Fears.Count == 0 ? "fears" : null,
                        string.IsNullOrWhiteSpace(member.Routine) ? "a routine" : null,
                    }.OfType<string>().ToList();
                    if (missing.Count > 0)
                    {
                        Error(IssueCodes.CastCoherence, at, $"{member.Name} has no {string.Join(", ", missing)}: the game master cannot play them.",
                            $"Give {member.Name} {string.Join(", ", missing)} for this case.");
                    }
                }
                else if (member.Wants.Drives.Count == 0)
                {
                    WarnOn([$"{member.CharacterId}.wants"], IssueCodes.CastCoherence, at, $"{member.Name} has no drives: a resident needs at least one thing they want.",
                        $"Give {member.Name} one drive and one fear that fit their routine.");
                }
            }
        }

        /// <summary>Everyone significant knows or thinks something the case uses (review 2, item 4).</summary>
        public void CheckMindsPlayable()
        {
            foreach (var member in packet.Cast.Where(c => c.Importance == CastImportance.Significant && c.Knows.Count == 0 && c.Thinks.Count == 0))
            {
                Error(IssueCodes.Provenance, $"cast:{member.CharacterId}", $"{member.Name} knows nothing and thinks nothing in this case.",
                    $"Give {member.Name} the facts they know (with how they learned them) and what they believe, true or false.");
            }
        }

        /// <summary>
        /// Fact ids written in free text (secrets, lies, beliefs, actions) exist, and the person knows the fact
        /// (review 2, item 4): the runtime cannot use an id in prose, and a wrong one misleads the game master.
        /// </summary>
        public void CheckCitedFacts()
        {
            foreach (var member in packet.Cast)
            {
                var texts = new[] { member.PlotRole, member.Routine, member.Wants.Guilt ?? "" }
                    .Concat(member.Secrets).Concat(member.Wants.Drives).Concat(member.Wants.Fears)
                    .Concat(member.Lies.Select(l => l.Story)).Concat(member.Thinks.Select(t => t.Belief))
                    .Concat(member.Wants.PermittedActions.Select(a => a.Description));
                foreach (var id in texts.SelectMany(t => CitedFact().Matches(t ?? "")).Select(m => m.Value).Distinct(StringComparer.Ordinal))
                {
                    var known = member.Knows.Exists(k => k.FactId == id);
                    if (packet.FindFact(id) is null || !known)
                    {
                        WarnOn([$"{member.CharacterId}.secrets", $"{member.CharacterId}.wants"], IssueCodes.Provenance, $"cast:{member.CharacterId}",
                            packet.FindFact(id) is null
                                ? $"{member.Name}'s record cites {id}, which is no fact of the case."
                                : $"{member.Name}'s record cites {id}, which they do not know.",
                            $"Write {member.Name}'s record in plain words without fact ids; what they know belongs in knows.");
                    }
                }
            }
        }

        [System.Text.RegularExpressions.GeneratedRegex(@"\bf\d+\b")]
        private static partial System.Text.RegularExpressions.Regex CitedFact();

        /// <summary>
        /// Swedish names of 1872 (review 2, item 5): a son takes his father's first name and -son, a daughter -dotter,
        /// and a wife keeps her own patronymic. Family names (Lindqvist) are inherited instead, and are left alone.
        /// </summary>
        private void CheckPatronymics()
        {
            foreach (var (a, b, nature) in Ties())
            {
                var (older, younger) = (a.Identity?.Age ?? 0) >= (b.Identity?.Age ?? 0) ? (a, b) : (b, a);
                // A persistent person's name is fixed in the Society's world: it cannot be renamed here.
                if (IsParentChild(nature) && Male(older) && !Fixed(younger) && NameRules.Patronymic(younger.Name) is { } suffix)
                {
                    var expected = NameRules.PatronymicOf(NameRules.FirstName(older.Name), suffix);
                    if (!younger.Name.EndsWith(expected, StringComparison.OrdinalIgnoreCase))
                    {
                        WarnOn([$"{younger.CharacterId}.identity"], IssueCodes.CastCoherence, $"cast:{younger.CharacterId}",
                            $"{younger.Name} is {older.Name}'s child, so in 1872 their patronymic is {expected}.",
                            $"Rename {younger.Name} to {NameRules.FirstName(younger.Name)} {expected}, keeping the id.");
                    }
                }

                if (IsSpouse(nature))
                {
                    var (wife, husband) = Male(a) ? (b, a) : (a, b);
                    var wifeLast = NameRules.LastName(wife.Name);
                    if (Male(husband) && !Male(wife) && !Fixed(wife) && wifeLast.Length > 0 && wifeLast == NameRules.LastName(husband.Name) && NameRules.Patronymic(husband.Name) == "son")
                    {
                        WarnOn([$"{wife.CharacterId}.identity"], IssueCodes.CastCoherence, $"cast:{wife.CharacterId}",
                            $"{wife.Name} carries her husband's patronymic; in 1872 a wife keeps her own, from her father's first name and -dotter.",
                            $"Give {NameRules.FirstName(wife.Name)} her own patronymic (her father's first name and -dotter), keeping the id.");
                    }
                }
            }
        }

        /// <summary>Pairs of cast members tied by a relationship either records, with what both record.</summary>
        private IEnumerable<(CastMember A, CastMember B, string Nature)> Ties() =>
            packet.Cast
                .SelectMany(a => a.Relationships.Select(r => (A: a, B: packet.FindCast(r.CharacterId), r.Nature)))
                .Where(t => t.B is not null && t.B != t.A)
                .Select(t => string.CompareOrdinal(t.A.CharacterId, t.B!.CharacterId) < 0 ? (A: t.A, B: t.B!, t.Nature) : (A: t.B!, B: t.A, t.Nature))
                .GroupBy(t => (t.A.CharacterId, t.B.CharacterId))
                .Select(g => (g.First().A, g.First().B, string.Join("; ", g.Select(t => t.Nature))));

        private bool Kin(string atA, string atB)
        {
            if (!atA.StartsWith("cast:", StringComparison.Ordinal) || !atB.StartsWith("cast:", StringComparison.Ordinal))
            {
                return false;
            }

            var (a, b) = (atA[5..], atB[5..]);
            return context.ParentChild.Contains(ValidationContext.PairKey(a, b)) || Ties().Any(t => IsParentChild(t.Nature) && ((t.A.CharacterId == a && t.B.CharacterId == b) || (t.A.CharacterId == b && t.B.CharacterId == a)));
        }

        internal static bool IsParentChild(string nature) =>
            System.Text.RegularExpressions.Regex.IsMatch(nature, @"\b(father|mother|son|daughter|parent|child|boy|girl|lad)\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase)
            && !nature.Contains("in-law", StringComparison.OrdinalIgnoreCase);

        private static bool IsSpouse(string nature) =>
            System.Text.RegularExpressions.Regex.IsMatch(nature, @"\b(wife|husband|spouse|married)\b", System.Text.RegularExpressions.RegexOptions.IgnoreCase);

        private static bool Fixed(CastMember c) => c.InstanceRef is InstanceRefs.Persistent or InstanceRefs.Promoted;

        private static bool Male(CastMember c) => (c.Identity?.Gender ?? "").Trim().ToLowerInvariant() switch
        {
            "male" or "man" or "m" or "boy" => true,
            "" => NameRules.Patronymic(c.Name) == "son",
            _ => false,
        };

        /// <summary>
        /// A first name that only lives on in a house's name ("Old Brita's Cottage") is not a person (review 2, item 4):
        /// event and hook text that names her sends players looking for someone who is not there.
        /// </summary>
        public void CheckHouseNames()
        {
            var people = packet.Cast.Select(c => NameRules.FirstName(c.Name)).Concat(packet.OffstagePeople.Select(o => NameRules.FirstName(o.Name))).ToHashSet(StringComparer.OrdinalIgnoreCase);
            foreach (var place in context.Level.Places)
            {
                foreach (var name in HouseOwner().Matches(place.Name).Select(m => m.Groups[1].Value).Where(n => !people.Contains(n)))
                {
                    var items = Items().Where(i => System.Text.RegularExpressions.Regex.IsMatch(i.Text, $@"\b{name}\b(?!'s)")).Select(i => i.Id).Distinct().ToList();
                    if (items.Count > 0)
                    {
                        WarnOn(items, IssueCodes.CastCoherence, items[0], $"{string.Join(", ", items)} speak of {name} as a person, but {name} is only the name of {place.Name}.",
                            $"Name the person who lives there now (or say \"the house\"), not {name}.");
                    }
                }
            }
        }

        [System.Text.RegularExpressions.GeneratedRegex(@"\b(\p{Lu}\p{Ll}+)'s\b")]
        private static partial System.Text.RegularExpressions.Regex HouseOwner();

        /// <summary>Each item's text with the id the critic routing understands.</summary>
        private IEnumerable<(string Id, string Text)> Items() =>
            packet.Truth.Escalation.Select(e => (e.Id, e.Description))
                .Concat(packet.Facts.Select(f => (f.Id, f.Statement)))
                .Concat(packet.Channels.Select(c => (c.Id, c.Reveal)))
                .Append(("hook", packet.Hook.Text))
                .Concat(packet.Level.SceneDescriptions.Select(s => ($"scene:{s.Key}", s.Value)))
                .Where(i => !string.IsNullOrEmpty(i.Item2));

        public void CheckHookGiver()
        {
            if (context.Pack.Setting.FindHookGiverType(packet.Brief.HookGiverType) is not { } type || packet.FindCast(packet.Hook.HookGiverId) is not { } giver)
            {
                return;
            }

            if (!type.Fits(giver.Identity ?? context.Pack.FindCharacter(giver.CharacterId)?.Identity))
            {
                WarnOn(["hook"], IssueCodes.Opening, "hook", $"The hook-giver should be {type.Meaning}, but {giver.Name} is {giver.Identity?.Occupation ?? "not one"}.",
                    $"Give the hook to a cast member who is {type.Meaning}.");
            }
        }

        public void CheckAftermath()
        {
            foreach (var beat in packet.Beats)
            {
                foreach (var factId in beat.EmitsFactIds.Where(f => !packet.Channels.Exists(c => c.FactId == f)).Distinct())
                {
                    WarnOn(["channels"], IssueCodes.Beats, $"fact:{factId}",
                        $"{factId} comes only from beat {beat.Id}: a group that was not there never learns it.",
                        $"Add a channel for {factId} gated afterBeat:{beat.Id}: next-day gossip, a trace left behind, or a witness.");
                }
            }
        }

        // ---- Sub-step 6c: the arc -----------------------------------------------------------------------------

        public void CheckArc()
        {
            foreach (var b in packet.Beats)
            {
                foreach (var id in b.EmitsFactIds)
                {
                    Fact(id, $"beat:{b.Id}");
                }

                Person(b.ActorId, $"beat:{b.Id}");
            }

            foreach (var placeId in packet.Beats.Select(b => b.Requires.Place).Append(packet.Climax.Conditions.Place).OfType<string>())
            {
                if (context.Level.FindPlace(placeId) is null)
                {
                    Error(IssueCodes.AffordanceFit, "beats", $"Beat or climax place '{placeId}' is not in the level.");
                }
            }

            foreach (var lever in packet.ResolutionLevers)
            {
                foreach (var id in lever.RequiresFactIds)
                {
                    Fact(id, $"lever:{lever.Id}");
                }

                foreach (var change in lever.Consequences.Attitudes)
                {
                    Person(change.CharacterId, $"lever:{lever.Id}");
                }
            }

            CheckResolution();
            CheckActors();

            var conditions = Creature?.ConfrontationConditions;
            if (conditions is not null)
            {
                if (conditions.Kind is ConfrontationKinds.Time or ConfrontationKinds.Both && string.IsNullOrWhiteSpace(packet.Climax.Conditions.Time))
                {
                    Error(IssueCodes.Beats, "climax", $"{Creature!.Name} can only be confronted at a certain time ({conditions.Time}); the climax must say when.");
                }

                if (conditions.Kind is ConfrontationKinds.Place or ConfrontationKinds.Both && string.IsNullOrWhiteSpace(packet.Climax.Conditions.Place))
                {
                    Error(IssueCodes.Beats, "climax", $"{Creature!.Name} can only be confronted at certain places; the climax must say where.");
                }
            }
        }

        /// <summary>
        /// A beat or the climax where a cast member acts is one of their permitted actions, at a desperation the beat's
        /// threshold unlocks (fixes 3, section 4). A warning, sent to the arc.
        /// </summary>
        private void CheckActors()
        {
            var acts = packet.Beats.Select(b => (Target: b.Id, Label: $"Beat {b.Id}", b.ActorId, b.ActionId, b.Threshold))
                .Append(("climax", "The climax", packet.Climax.ActorId, packet.Climax.ActionId, packet.Climax.Threshold));
            foreach (var (target, label, actorId, actionId, threshold) in acts)
            {
                if (packet.FindCast(actorId) is not { } actor || actor.Wants.PermittedActions.Count == 0)
                {
                    continue;
                }

                var options = string.Join("; ", actor.Wants.PermittedActions.Select(a => $"{a.Id} ({a.Desperation}): {a.Description}"));
                var action = actor.Wants.PermittedActions.Find(a => a.Id == actionId);
                if (action is null)
                {
                    WarnOn([target], IssueCodes.Beats, target == "climax" ? "climax" : $"beat:{target}",
                        $"{label} has {actor.CharacterId} act outside {actor.Name}'s permitted actions.",
                        $"Name the action as actionId, one of {options}; or change what {actor.Name} does.");
                }
                else if (!DesperationTiers.UnlockedAt(threshold).Contains(action.Desperation))
                {
                    WarnOn([target], IssueCodes.Beats, target == "climax" ? "climax" : $"beat:{target}",
                        string.Create(CultureInfo.InvariantCulture, $"{label} at {threshold:0.00} has {actor.Name} {action.Id}, a {action.Desperation}-desperation act; at that tension only {string.Join(" and ", DesperationTiers.UnlockedAt(threshold))} are open."),
                        string.Create(CultureInfo.InvariantCulture, $"Move it to a threshold of at least {(action.Desperation == DesperationTiers.High ? DesperationTiers.HighFrom : DesperationTiers.MediumFrom):0.00}, or give {actor.Name} a milder act."));
                }
            }
        }

        private void CheckResolution()
        {
            if (packet.ResolutionLevers.Count == 0)
            {
                Error(IssueCodes.Resolution, "resolutionLevers", "The case has no way to end.");
                return;
            }

            foreach (var lever in packet.ResolutionLevers.Where(l => !SolveDepths.All.Contains(l.SolveDepth)))
            {
                Error(IssueCodes.Schema, $"lever:{lever.Id}", $"Unknown solve depth '{lever.SolveDepth}'.{CoherenceIssue.Allowed(SolveDepths.All)}");
            }

            // A voice session cannot gather a dozen facts for one ending (review 2, item 8).
            foreach (var lever in packet.ResolutionLevers.Where(l => l.RequiresFactIds.Count > context.MaxLeverFacts))
            {
                Error(IssueCodes.Resolution, $"lever:{lever.Id}", $"{lever.Id} requires {lever.RequiresFactIds.Count} facts; at most {context.MaxLeverFacts} can be gathered at a voice table.",
                    $"Keep the {context.MaxLeverFacts} facts the ending truly turns on; the others support it without being required.");
            }

            if (packet.ResolutionLevers.Count < 2)
            {
                Warn(IssueCodes.Resolution, "resolutionLevers", "Only one resolution lever; offer several with different consequences.");
            }

            if (packet.Truth.PlotCentre.Kind == PlotCentreKinds.HiddenHand)
            {
                if (!packet.ResolutionLevers.Exists(l => l.SolveDepth == SolveDepths.Crude))
                {
                    Error(IssueCodes.Resolution, "resolutionLevers", "A hidden-hand case needs a crude solve that is always available.");
                }

                if (!packet.ResolutionLevers.Exists(l => l.SolveDepth == SolveDepths.Deep))
                {
                    Error(IssueCodes.Resolution, "resolutionLevers", "A hidden-hand case needs a deep solve (expose the hand, lift the curse, save the victim).");
                }
            }

            foreach (var lever in packet.ResolutionLevers.Where(l => l.SolveDepth == SolveDepths.Deep && l.RequiresFactIds.Count == 0))
            {
                Warn(IssueCodes.Resolution, $"lever:{lever.Id}", "A deep solve should name the facts it takes to see it.");
            }

            CheckDeeds();
        }

        /// <summary>
        /// A creature that can be bargained with has a mind, whose terms ask for the deep lever's deeds (fixes 3,
        /// section 10): otherwise the runtime has nothing to play it from, or its terms and the deep solve disagree.
        /// </summary>
        public void CheckCreatureMinds()
        {
            foreach (var mind in packet.CreatureMinds)
            {
                foreach (var k in mind.Knows)
                {
                    Fact(k.FactId, "creatureMinds");
                }
            }

            if (Creature is null || Creature.ResolutionShape != ResolutionShapes.Negotiable)
            {
                return;
            }

            var mindOf = packet.CreatureMinds.Find(m => m.VaesenId == Creature.Id);
            if (mindOf is null)
            {
                Error(IssueCodes.Resolution, "creatureMinds", $"{Creature.Name} can be bargained with but has no mind: what it wants, its terms, its red lines and how it answers.",
                    $"Add one creatureMinds entry with vaesenId {Creature.Id}.");
                return;
            }

            var deep = packet.ResolutionLevers.Where(l => l.SolveDepth == SolveDepths.Deep).SelectMany(l => l.RequiredDeeds).Select(d => d.Id).ToHashSet(StringComparer.Ordinal);
            if (mindOf.Terms.Count == 0 || mindOf.TermDeedIds.Count == 0)
            {
                Error(IssueCodes.Resolution, "creatureMinds", $"{Creature.Name}'s mind has no terms tied to the deep lever's deeds.",
                    "Give its terms, and list in termDeedIds the deep lever's deeds that meet them.");
            }
            else if (mindOf.TermDeedIds.FirstOrDefault(d => !deep.Contains(d)) is { } stray)
            {
                Error(IssueCodes.Resolution, "creatureMinds", $"{Creature.Name}'s terms name '{stray}', which is not a deed of a deep lever.",
                    $"Use the deep lever's deed ids: {string.Join(", ", deep)}.");
            }
        }

        /// <summary>
        /// Every lever says what the players must do (fixes 3, section 9), with ids that exist, at places the players can
        /// reach: the climax's place or one a channel, a beat or a scene uses.
        /// </summary>
        private void CheckDeeds()
        {
            var reachable = packet.Channels.Select(c => c.PlaceId)
                .Concat(packet.Beats.Select(b => b.Requires.Place))
                .Append(packet.Climax.Conditions.Place)
                .Concat(packet.Level.PlacesUsed)
                .OfType<string>()
                .ToHashSet(StringComparer.Ordinal);
            foreach (var lever in packet.ResolutionLevers)
            {
                var at = $"lever:{lever.Id}";
                if (lever.RequiredDeeds.Count == 0)
                {
                    Error(IssueCodes.Resolution, at, $"Lever {lever.Id} has no required deeds: what must the players do for it to take effect?",
                        "List its deeds: where, when and before whom. A lever that only needs force has one deed (defeat or drive off the creature).");
                }

                if (!DeedOrders.All.Contains(lever.DeedOrder))
                {
                    Error(IssueCodes.Schema, at, $"Unknown deed order '{lever.DeedOrder}'.{CoherenceIssue.Allowed(DeedOrders.All)}");
                }

                foreach (var deed in lever.RequiredDeeds)
                {
                    if (deed.PerformedBy.Who.Count == 0)
                    {
                        Error(IssueCodes.Resolution, at, $"Deed {deed.Id} says nobody does it: name who must (performedBy: cast ids, or players).");
                    }

                    foreach (var performer in deed.PerformedBy.Who.Where(w => w != DeedPerformers.Players))
                    {
                        Person(performer, at);
                    }

                    var place = deed.Place is null ? null : context.Level.FindPlace(deed.Place);
                    if (deed.Place is not null && place is null)
                    {
                        Error(IssueCodes.UnknownReference, at, $"Deed {deed.Id} is at '{deed.Place}', which is not a place of the level.");
                    }

                    if (deed.AffordanceId is not null && (place ?? context.Level.Places.Find(p => p.FindAffordance(deed.AffordanceId) is not null))?.FindAffordance(deed.AffordanceId) is null)
                    {
                        Error(IssueCodes.AffordanceFit, at, $"Deed {deed.Id} uses '{deed.AffordanceId}', which is not an affordance of {(place?.Name ?? "the level")}.");
                    }

                    foreach (var witness in deed.Witnesses)
                    {
                        Person(witness, at);
                    }

                    if (!WitnessRules.All.Contains(deed.WitnessRule))
                    {
                        Error(IssueCodes.Schema, at, $"Deed {deed.Id} has unknown witness rule '{deed.WitnessRule}'.{CoherenceIssue.Allowed(WitnessRules.All)}");
                    }

                    if (deed.Time is { Length: > 0 } time && !AccessRequirement.Times.Contains(time))
                    {
                        Warn(IssueCodes.Resolution, at, $"Deed {deed.Id} needs the time '{time}'.{CoherenceIssue.Allowed(AccessRequirement.Times)}");
                    }

                    if (place is not null && !reachable.Contains(place.Id))
                    {
                        WarnOn([lever.Id], IssueCodes.Resolution, at, $"Deed {deed.Id} is at {place.Name}, which nothing in the case brings the players to.",
                            "Put it where the climax happens or at a place a channel or beat uses.");
                    }
                }
            }
        }

        // ---- Sub-step 6d: the hook and the scenes -------------------------------------------------------------

        /// <summary>
        /// The case keeps to the level's geography (fixes 4, section 13): it names no authority the level does not have
        /// within reach, no known town besides the level's neighbours and its journey, and never the real analogue.
        /// </summary>
        public void CheckGeography()
        {
            var g = context.Level.Geography;
            if (g.IsEmpty)
            {
                return;
            }

            var text = string.Join('\n', CaseTexts());
            bool Says(string phrase) => System.Text.RegularExpressions.Regex.IsMatch(text,
                $@"(?<![\p{{L}}]){System.Text.RegularExpressions.Regex.Escape(phrase)}(?![\p{{L}}])", System.Text.RegularExpressions.RegexOptions.IgnoreCase);

            foreach (var (kind, where, words) in g.Authorities.All().Where(a => string.IsNullOrWhiteSpace(a.Where)))
            {
                if (words.FirstOrDefault(Says) is { } word)
                {
                    Warn(IssueCodes.Consistency, "geography", $"The case speaks of a {word}, but {context.Level.Name} lists no {kind} within reach.",
                        "Leave it out, or use an authority the level's geography lists.");
                }
            }

            var allowed = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "Stockholm", "Uppsala", context.Level.Journey.FromHeadquarters };
            allowed.UnionWith(g.Neighbours.Select(n => n.Name));
            allowed.UnionWith(context.Level.Journey.Routes.SelectMany(r => r.Legs).SelectMany(l => new[] { l.From, l.To }));
            var known = context.Pack.Setting.PeriodTransport.SelectMany(l => l.Places)
                .Concat(context.Pack.Setting.RegionalInstitutions.Select(i => i.Seat))
                .Concat(context.Pack.Levels.SelectMany(l => l.Geography.Neighbours).Select(n => n.Name))
                .Where(t => t.Length > 2 && !allowed.Contains(t))
                .Distinct(StringComparer.OrdinalIgnoreCase);
            foreach (var town in known.Where(Says))
            {
                Warn(IssueCodes.Consistency, "geography", $"The case names {town}, which is not among {context.Level.Name}'s neighbouring towns.",
                    $"Use one of: {string.Join(", ", g.Neighbours.Select(n => n.Name))}.");
            }

            if (g.Fictional && g.RealAnalogue.Split(' ', '(')[0] is { Length: > 2 } analogue && Says(analogue) && !allowed.Contains(analogue))
            {
                Warn(IssueCodes.Consistency, "geography", $"The case names {analogue}, the real place {context.Level.Name} is modelled on.", "Never name the real analogue.");
            }
        }

        /// <summary>Every piece of prose in the case a player could hear.</summary>
        private IEnumerable<string> CaseTexts() =>
            new[] { packet.Truth.Summary, packet.Truth.Provocation, packet.Hook.Text }
                .Concat(packet.Truth.Escalation.Select(e => e.Description))
                .Concat(packet.Facts.Select(f => f.Statement))
                .Concat(packet.Channels.Select(c => c.Reveal))
                .Concat(packet.Level.SceneDescriptions.Values)
                .Concat(packet.Cast.SelectMany(c => new[] { c.PlotRole, c.Routine }.Concat(c.Secrets)))
                .Concat(packet.OffstagePeople.Select(o => $"{o.Relation} {o.Note}"))
                .Where(t => !string.IsNullOrWhiteSpace(t));

        public void CheckHook()
        {
            foreach (var id in packet.Hook.AssertedFactIds)
            {
                Fact(id, "hook");
            }

            foreach (var id in packet.Hook.OpeningEventIds.Where(id => !packet.Truth.Escalation.Exists(e => e.Id == id)))
            {
                Error(IssueCodes.UnknownReference, "hook", $"Unknown opening event '{id}'.");
            }

            Person(packet.Hook.HookGiverId, "hook");
            if (string.IsNullOrEmpty(packet.Hook.HookGiverId))
            {
                Error(IssueCodes.CastCoherence, "hook", "The hook needs a hook-giver from the cast.");
            }

            if (packet.Hook.OpeningEventIds.Count == 0)
            {
                Error(IssueCodes.Opening, "hook", "The hook mentions none of the opening events.");
            }

            if (!string.IsNullOrEmpty(packet.Brief.HookChannelId) && packet.Hook.Channel != packet.Brief.HookChannelId)
            {
                Warn(IssueCodes.Opening, "hook", $"The hook uses channel '{packet.Hook.Channel}', not the assigned '{packet.Brief.HookChannelId}'.");
            }

            foreach (var fact in packet.Hook.AssertedFactIds.Select(packet.FindFact).OfType<CaseFact>().Where(f => f.Kind == FactKinds.Cause))
            {
                Error(IssueCodes.Opening, "hook", $"The hook states the cause ({fact.Id}); the true conflict is never a clue at the start.");
            }

            if (packet.Truth.Focus == CaseFocus.What)
            {
                var openingFacts = _graph.OpeningKnowledge();
                foreach (var fact in packet.Facts.Where(f => f.Giveaway && openingFacts.Contains(f.Id)))
                {
                    Error(IssueCodes.Giveaway, $"fact:{fact.Id}", "A giveaway tell is in the opening of a case whose puzzle is the creature's identity.",
                        "Open on ambiguous signs only; let the giveaway surface late as confirmation.");
                }
            }

            foreach (var id in packet.Level.PlacesUsed.Where(id => context.Level.FindPlace(id) is null))
            {
                Error(IssueCodes.AffordanceFit, "level.placesUsed", $"Place '{id}' is not in the level.");
            }

            foreach (var id in packet.Level.SceneDescriptions.Keys.Where(id => context.Level.FindPlace(id) is null))
            {
                Error(IssueCodes.AffordanceFit, "level.sceneDescriptions", $"Scene for place '{id}', which is not in the level.");
            }
        }

        // ---- Review 3: scenes, the hook's writer, beats and the people they lean on ------------------------------

        /// <summary>
        /// A scene description is read to the players when they arrive (review 3, item 4; fixes 5, section 6): it must
        /// not show what a channel at that place keeps behind trust, a roll, a time, an item, a fact or a beat. A channel
        /// is at the place when it sits there or its teller lives there. Code compares the scene's key terms with each
        /// gated reveal's (not the place's own words, not what the opening already says): three shared, or one proper
        /// name the reveal brings in, is a leak. An error for the hook and scenes, quoting the sentence.
        /// </summary>
        public void CheckSceneLeaks()
        {
            var open = packet.Channels.Where(c => !Gated(c)).Select(c => c.Reveal)
                .Append(packet.Hook.Text)
                .Concat(packet.Truth.Escalation.Where(e => e.Opening).Select(e => e.Description));
            var openText = string.Join(' ', open);
            var common = SceneWords.Of(openText);
            var known = packet.Cast.Select(c => c.Name).Concat(packet.OffstagePeople.Select(o => o.Name))
                .Concat(context.Level.Places.Select(p => p.Name)).Append(context.Level.Name).Append(openText)
                .SelectMany(n => n.Split(' ', StringSplitOptions.RemoveEmptyEntries)).ToHashSet(StringComparer.Ordinal);
            foreach (var (placeId, scene) in packet.Level.SceneDescriptions)
            {
                var place = context.Level.FindPlace(placeId);
                var own = SceneWords.Of(string.Join(' ', new[] { place?.Name, place?.Description, context.Level.Name }
                    .Concat(place?.AllAffordances().Select(a => a.Name) ?? [])));
                var seen = SceneWords.Of(scene);
                var here = packet.Channels.Where(c => Gated(c) && (c.PlaceId == placeId || packet.FindCast(c.SourceCharacterId)?.HomePlaceId == placeId));
                foreach (var channel in here)
                {
                    var hidden = SceneWords.Of(channel.Reveal);
                    hidden.ExceptWith(own);
                    hidden.ExceptWith(common);
                    var shown = hidden.Intersect(seen).ToList();
                    var names = IntroducedName().Matches(channel.Reveal).Select(m => m.Value)
                        .Where(n => !known.Contains(n) && System.Text.RegularExpressions.Regex.IsMatch(scene, $@"\b{n}\b")).Distinct().ToList();
                    if (shown.Count < SceneWords.LeakWords && names.Count == 0)
                    {
                        continue;
                    }

                    var terms = shown.Concat(names).ToList();
                    var sentence = SentenceOf(scene, terms);
                    Error(IssueCodes.Giveaway, $"scene:{placeId}",
                        $"The scene at {place?.Name ?? placeId} shows what channel {channel.Id} ({channel.FactId}) keeps behind {string.Join(", ", channel.Access.Requires.Append(channel.Access.Item).OfType<string>())}: it shares \"{string.Join("\", \"", terms)}\" with that reveal, in \"{sentence}\". The game master reads the scene aloud on arrival.",
                        $"Rewrite the sentence \"{sentence}\" so it sets the place without showing what {channel.Id} reveals.");
                }
            }
        }

        /// <summary>The scene's sentence that holds most of the shared terms.</summary>
        private static string SentenceOf(string scene, List<string> terms) =>
            System.Text.RegularExpressions.Regex.Split(scene, @"(?<=[.!?;])\s+")
                .OrderByDescending(s => SceneWords.Of(s).Count(terms.Contains) + terms.Count(t => s.Contains(t, StringComparison.Ordinal)))
                .First()
                .Trim();

        /// <summary>A capitalised word inside a sentence, not at its start: a proper name a reveal brings in.</summary>
        [System.Text.RegularExpressions.GeneratedRegex(@"(?<=[\p{Ll},;:] )\p{Lu}\p{Ll}+")]
        private static partial System.Text.RegularExpressions.Regex IntroducedName();

        /// <summary>A channel players cannot open on arrival: it needs trust, a roll, a time, an item, a fact or a beat.</summary>
        private bool Gated(CaseChannel c) =>
            c.Access.Item is { Length: > 0 } || c.Access.Requires.Exists(r => AccessRequirement.Parse(r) is { Kind: not AccessRequirement.Place });

        /// <summary>
        /// A relative or person the hook names is someone in the case (review 3, item 3): the hook is the first thing
        /// players read, and a sister called Maja there and Frida in the case breaks the hook-giver at once.
        /// </summary>
        public void CheckHookNames()
        {
            var names = packet.Cast.Select(c => c.Name).Concat(packet.OffstagePeople.Select(o => o.Name)).Concat(context.WorldNames.Select(w => w.Name)).ToList();
            bool Known(string name) => names.Exists(n => n.Split(' ').Contains(name, StringComparer.Ordinal) || n == name);
            var text = packet.Hook.Text ?? "";
            var named = HookRelative().Matches(text).Select(m => (Name: m.Groups["name"].Value, Said: m.Value))
                .Concat(NameRules.Mentioned(text).Select(n => (Name: n, Said: n)));
            foreach (var (name, said) in named.Where(n => !Known(n.Name)).DistinctBy(n => n.Name))
            {
                Error(IssueCodes.CastCoherence, "hook", $"The hook says \"{said}\", but no one in the case is called {name}.",
                    $"Use the name the case gives that person ({string.Join(", ", packet.OffstagePeople.Select(o => $"{o.Name}, {o.Relation}"))}), or leave the name out.");
            }
        }

        [System.Text.RegularExpressions.GeneratedRegex(@"\b(?:[Mm]y|[Oo]ur|[Hh]is|[Hh]er|[Tt]heir)\s+(?:\p{L}+\s+)?(?:sister|brother|wife|husband|daughter|son|mother|father|cousin|aunt|uncle|niece|nephew|friend|neighbour|neighbor|master|mistress|girl|boy|lad)\s*,?\s+(?<name>\p{Lu}\p{Ll}+)")]
        private static partial System.Text.RegularExpressions.Regex HookRelative();

        /// <summary>
        /// The hook-giver knows what their hook states (review 3, item 3): the runtime plays them from their knows, and
        /// would have them deny what they wrote. A warning, sent to minds and channels.
        /// </summary>
        public void CheckHookGiverKnows()
        {
            if (packet.FindCast(packet.Hook.HookGiverId) is not { } giver)
            {
                return;
            }

            var unknown = packet.Hook.AssertedFactIds.Where(f => packet.FindFact(f) is not null && !giver.Knows.Exists(k => k.FactId == f)).ToList();
            if (unknown.Count > 0)
            {
                WarnOn([$"{giver.CharacterId}.knows"], IssueCodes.Provenance, $"cast:{giver.CharacterId}",
                    $"{giver.Name} brought the Society with a hook that states {string.Join(", ", unknown)}, which {giver.Name} does not know.",
                    $"Add {string.Join(", ", unknown)} to {giver.Name}'s knows, with how they learned it.");
            }
        }

        /// <summary>
        /// A beat's actor puts into the world only what they could (review 3, item 3): a fact another person observed is
        /// not theirs to show or tell, though their own deed in the beat is. A warning, sent to the arc.
        /// </summary>
        public void CheckBeatProvenance()
        {
            foreach (var beat in packet.Beats)
            {
                if (packet.FindCast(beat.ActorId) is not { } actor)
                {
                    continue;
                }

                // A fact that names the actor is their own deed, done in the beat ("Wester has the papers taken"): theirs.
                var names = new[] { NameRules.FirstName(actor.Name), NameRules.LastName(actor.Name) }.Where(n => n.Length > 2).ToList();
                var theirs = beat.EmitsFactIds
                    .Where(f => packet.FindFact(f) is { Kind: not FactKinds.Symptom } fact && !actor.Knows.Exists(k => k.FactId == f)
                        && !names.Exists(n => System.Text.RegularExpressions.Regex.IsMatch(fact.Statement, $@"\b{System.Text.RegularExpressions.Regex.Escape(n)}\b")))
                    .ToList();
                if (theirs.Count > 0)
                {
                    WarnOn([beat.Id], IssueCodes.Provenance, $"beat:{beat.Id}",
                        $"Beat {beat.Id} has {actor.Name} put {string.Join(", ", theirs)} into the world, which {actor.Name} does not know.",
                        $"Take {string.Join(", ", theirs)} out of {beat.Id}'s emitsFactIds, or let the beat's actor be someone who knows it.");
                }
            }
        }

        /// <summary>
        /// A person who tells a load-bearing fact is played, not read from a template (review 3, item 4): a resident who
        /// carries the cause needs a fear and something they will do when pressed. A warning, sent to their step.
        /// </summary>
        public void CheckSourcesPlayable()
        {
            var tellers = packet.Channels
                .Where(c => c.Kind is ChannelKinds.Testimony or ChannelKinds.Overheard && packet.FindFact(c.FactId) is { LoadBearing: true })
                .Select(c => c.SourceCharacterId)
                .OfType<string>()
                .Distinct(StringComparer.Ordinal);
            foreach (var member in tellers.Select(packet.FindCast).OfType<CastMember>())
            {
                var missing = new[]
                {
                    member.Wants.Fears.Count == 0 ? "a fear" : null,
                    member.Wants.PermittedActions.Count == 0 ? "a permitted action" : null,
                }.OfType<string>().ToList();
                if (missing.Count > 0)
                {
                    WarnOn([$"{member.CharacterId}.wants"], IssueCodes.CastCoherence, $"cast:{member.CharacterId}",
                        $"{member.Name} tells load-bearing facts but has no {string.Join(" and no ", missing)}: the game master cannot play their resistance.",
                        $"Give {member.Name} {string.Join(" and ", missing)} that fit what they know and who they protect.");
                }
            }
        }

        // ---- The whole case ----------------------------------------------------------------------------------

        public void CheckLevel()
        {
            var level = context.Level;
            if (packet.Level.LevelId != level.Id)
            {
                Error(IssueCodes.AffordanceFit, "level", $"The case names level '{packet.Level.LevelId}' but was assigned '{level.Id}'.");
            }

            var offered = context.Pack.Vocabulary.ExpandAll(level.AllTags());
            foreach (var creature in context.Vaesen)
            {
                foreach (var tag in creature.RequiredAffordances.Where(t => !offered.Contains(t)))
                {
                    Error(IssueCodes.AffordanceFit, $"vaesen:{creature.Id}", $"{creature.Name} needs '{tag}', which the level does not offer.");
                }
            }

            foreach (var c in packet.Channels.Where(c => c.PlaceId is not null && level.FindPlace(c.PlaceId) is not null && !packet.Level.PlacesUsed.Contains(c.PlaceId)))
            {
                // The runtime loads only the places used (fixes 5, section 7): an error, sent to minds and channels to move it.
                Error(IssueCodes.AffordanceFit, $"channel:{c.Id}", $"Place '{c.PlaceId}' holds a clue but is not one of the places the case uses.",
                    $"Move {c.Id} to a place the case uses ({string.Join(", ", packet.Level.PlacesUsed)}), or to its owner's home there.");
            }

            foreach (var fact in packet.Facts.Where(f => !f.LoadBearing && !_graph.HasAnySource(f.Id) && !_graph.OpeningKnowledge().Contains(f.Id)))
            {
                Warn(IssueCodes.Unreachable, $"fact:{fact.Id}", "This fact has no channel and is never revealed.");
            }
        }

        private void Fact(string? id, string at)
        {
            if (id is not null && packet.FindFact(id) is null)
            {
                Error(IssueCodes.UnknownReference, at, $"Unknown fact '{id}'.");
            }
        }

        private void Person(string? id, string at)
        {
            if (!string.IsNullOrEmpty(id) && packet.FindCast(id) is null)
            {
                Error(IssueCodes.UnknownReference, at, $"Unknown cast member '{id}'.");
            }
        }

        public void CheckReachability()
        {
            foreach (var fact in LoadBearing.Where(f => !FullWalk.Known.Contains(f.Id)))
            {
                Error(IssueCodes.Unreachable, $"fact:{fact.Id}",
                    $"Load-bearing fact '{fact.Statement}' cannot be reached from the opening.",
                    "Add an open channel, or make sure the facts its channels are gated on can be reached.");
            }
        }

        public void CheckRedundancy()
        {
            var opening = _graph.OpeningKnowledge();
            foreach (var fact in LoadBearing.Where(f => !opening.Contains(f.Id)))
            {
                var sources = _graph.SourcesOf(fact.Id);
                if (sources.Count < 2)
                {
                    // Say why channels that exist do not count: hearsay is the usual reason, and invisible without this.
                    var hearsay = packet.Channels.Where(c => c.FactId == fact.Id && _graph.IsHearsay(c)).ToList();
                    var why = hearsay.Count == 0
                        ? ""
                        : $" {string.Join(", ", hearsay.Select(c => $"{c.Id} ({packet.FindCast(c.SourceCharacterId)?.Name})"))} rest only on hearsay (a bystander or someone offstage), which does not count here.";
                    Error(IssueCodes.Redundancy, $"fact:{fact.Id}",
                        sources.Count == 0 && hearsay.Count > 0
                            ? $"Load-bearing fact '{fact.Statement}' has only hearsay channels; it needs a direct one, and two independent channels in all.{why}"
                            : $"Load-bearing fact '{fact.Statement}' has {sources.Count} independent channel(s); it needs at least two.{why}",
                        hearsay.Count == 0
                            ? "Place it through another person or another spot: a trace, a document, an overheard remark."
                            : "Give a teller first-hand knowledge (witnessed, self, lore, or told by a cast member who knows it), or add a trace or a document.");
                }
            }
        }

        /// <summary>
        /// Order independence: no single channel may be the only way in (losing any one channel still leaves every
        /// load-bearing fact reachable), and groups that only talk or only search still get a good share.
        /// </summary>
        public void CheckOrderIndependence()
        {
            var loadBearing = LoadBearing.Select(f => f.Id).Where(FullWalk.Known.Contains).ToList();
            foreach (var channel in packet.Channels.Where(c => FullWalk.UsedChannels.Contains(c.Id)))
            {
                var without = _graph.Walk(c => c.Id != channel.Id);
                var lost = loadBearing.Where(f => !without.Known.Contains(f)).ToList();
                if (lost.Count > 0)
                {
                    Error(IssueCodes.SinglePointOfFailure, $"channel:{channel.Id}",
                        $"Without this one channel, {string.Join(", ", lost)} can no longer be reached.",
                        "Give the facts that unlock the rest a second, independent way in.");
                }
            }

            if (loadBearing.Count == 0)
            {
                return;
            }

            CheckEntryStyle("only talk to people",
                c => c.Kind is ChannelKinds.Testimony or ChannelKinds.Overheard or ChannelKinds.Event or ChannelKinds.KnowledgeGated, loadBearing);
            CheckEntryStyle("only search places and papers",
                c => c.Kind is ChannelKinds.Trace or ChannelKinds.Document or ChannelKinds.Event or ChannelKinds.KnowledgeGated, loadBearing);
        }

        private void CheckEntryStyle(string style, Func<CaseChannel, bool> filter, List<string> loadBearing)
        {
            var walk = _graph.Walk(filter);
            var reached = loadBearing.Count(walk.Known.Contains);
            if (reached < loadBearing.Count * EntryStyleCoverage)
            {
                Warn(IssueCodes.EntryStyle, "channels",
                    $"Players who {style} reach only {reached} of {loadBearing.Count} load-bearing facts.",
                    "Mix testimony and physical traces across the trail.");
            }
        }

        public void CheckDecoysAndFairPlay()
        {
            foreach (var decoy in packet.Decoys.Where(d => d.ClearedByFactIds.Count > 0 && !d.ClearedByFactIds.Any(FullWalk.Known.Contains)))
            {
                Error(IssueCodes.Decoy, $"decoy:{decoy.ThreadId}", "None of the facts that clear this decoy can be reached, so it can never be resolved.");
            }

            // Fair play: an honest clue to the true answer, reachable early and without resolving any decoy.
            var truthKinds = new[] { FactKinds.Identity, FactKinds.Cause, FactKinds.Motive };
            var opening = _graph.OpeningKnowledge();
            var honest = _graph.Walk(gateFilter: f => !FactPointsTo.IsDecoy(packet.FindFact(f)?.PointsTo));
            var early = packet.Channels.Where(c =>
                    packet.FindFact(c.FactId) is { PointsTo: FactPointsTo.TrueAnswer } fact
                    && truthKinds.Contains(fact.Kind)
                    && _graph.FactGates(c).All(opening.Contains))
                .ToList();
            if (early.Count == 0)
            {
                Error(IssueCodes.FairPlay, "facts", "No clue to the true answer can be found early: every one is gated behind other discoveries.",
                    "Give at least one identity, cause or motive fact an ungated channel.");
            }

            if (!packet.Facts.Exists(f => f.PointsTo == FactPointsTo.TrueAnswer && truthKinds.Contains(f.Kind) && honest.Known.Contains(f.Id)))
            {
                Error(IssueCodes.FairPlay, "facts", "The true answer can only be reached through decoy evidence.");
            }

            var hand = packet.Truth.PlotCentre.Kind == PlotCentreKinds.HiddenHand ? packet.Truth.PlotCentre.CharacterId : null;
            if (hand is not null)
            {
                var withoutConfession = _graph.Walk(c => c.SourceCharacterId != hand);
                if (!packet.Facts.Exists(f => f.PointsTo == FactPointsTo.TrueAnswer && f.Kind is FactKinds.Identity or FactKinds.Motive
                        && withoutConfession.Known.Contains(f.Id)))
                {
                    Error(IssueCodes.FairPlay, "truth.plotCentre", "The hidden hand can only be exposed by their own words.",
                        "Leave an honest trace or witness pointing at them.");
                }
            }
        }

        public void CheckDiscoverableSolves()
        {
            foreach (var lever in packet.ResolutionLevers.Where(l => l.SolveDepth == SolveDepths.Deep && l.RequiresFactIds.Count > 0))
            {
                if (!lever.RequiresFactIds.All(FullWalk.Known.Contains))
                {
                    Error(IssueCodes.Resolution, $"lever:{lever.Id}", "The deep solve needs facts that cannot be reached, so it is not discoverable.");
                }
            }
        }

        /// <summary>
        /// The engine computes the fact weights (<see cref="Generation.Derived.Tension"/>), so a threshold players
        /// cannot reach is the engine's fault, not the model's: reported as a warning for review, never fed back.
        /// </summary>
        public void CheckBeatsAndTension()
        {
            var tension = packet.Tension;
            if (tension.Start is < 0 or > 1)
            {
                Warn(IssueCodes.Beats, "tension.start", "The tension dial starts outside 0-1.");
            }

            foreach (var beat in packet.Beats.Where(b => b.Threshold is < 0 or > 1))
            {
                Warn(IssueCodes.Beats, $"beat:{beat.Id}", "Beat threshold is outside 0-1.");
            }

            if (LoadBearing.Any(f => !FullWalk.Known.Contains(f.Id)))
            {
                // Reachability already says what is missing; the dial cannot be judged until it is fixed.
                return;
            }

            foreach (var beat in packet.Beats.Where(b => !FullWalk.FiredBeats.Contains(b.Id)))
            {
                Warn(IssueCodes.Beats, $"beat:{beat.Id}",
                    $"Internal: the computed weights reach {FullWalk.FinalTension:0.00}, short of this beat's threshold ({beat.Threshold:0.00}).");
            }

            if (FullWalk.FinalTension < packet.Climax.Threshold)
            {
                Warn(IssueCodes.Beats, "climax",
                    $"Internal: the computed weights reach {FullWalk.FinalTension:0.00}, short of the climax threshold ({packet.Climax.Threshold:0.00}).");
            }
        }
    }
}

/// <summary>A validator of a case under construction (spec 6.3).</summary>
public interface ICoherenceValidator
{
    List<CoherenceIssue> Validate(CasePacket packet, ValidationContext context, CheckScope scope = CheckScope.Full);
}
