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
public sealed class CaseValidator : ICoherenceValidator
{
    /// <summary>A group that only talks (or only searches) should still reach this share of the load-bearing facts.</summary>
    public const double EntryStyleCoverage = 0.5;

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
        }

        if (scope.HasFlag(CheckScope.Whole))
        {
            run.CheckLevel();
            run.CheckReachability();
            run.CheckRedundancy();
            run.CheckOrderIndependence();
            run.CheckDecoysAndFairPlay();
            run.CheckDiscoverableSolves();
            run.CheckBeatsAndTension();
        }

        return run.Issues;
    }

    private sealed class Run(CasePacket packet, ValidationContext context)
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

        public void CheckConflict()
        {
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
                                $"Rename {member.Name}, keeping the id: a first name that does not sound like {NameRules.FirstName(name)} or any other on the list.");
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
                foreach (var rel in member.Relationships.Where(r => packet.FindCast(r.CharacterId) is null
                    && !context.SocietyCharacterIds.Contains(r.CharacterId) && context.Pack.FindCharacter(r.CharacterId) is null))
                {
                    Warn(IssueCodes.UnknownReference, at, $"Relationship to '{rel.CharacterId}', who is not in the cast or the Society's world.");
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
            var people = packet.Cast.Select(c => (Name: c.Name, At: $"cast:{c.CharacterId}")).ToList();
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

                    if (KnowledgeSources.IsHearsay(source))
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
                        Error(IssueCodes.Provenance, at, $"Knows {k.FactId} from an unreadable source '{source}'. Allowed: witnessed, self, lore, told:<characterId>, told:bystander:<role>, found:<channelId>.",
                            "Use witnessed, self, lore, told:<characterId>, told:bystander:<role> or found:<channelId>.");
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
                Warn(IssueCodes.AffordanceFit, $"channel:{c.Id}", $"Place '{c.PlaceId}' holds a clue but is not in placesUsed.");
            }

            foreach (var fact in packet.Facts.Where(f => !f.LoadBearing && !_graph.SourcesOf(f.Id).Any() && !_graph.OpeningKnowledge().Contains(f.Id)))
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
                        : $" {string.Join(", ", hearsay.Select(c => $"{c.Id} ({packet.FindCast(c.SourceCharacterId)?.Name})"))} rest only on bystander gossip, which never counts.";
                    Error(IssueCodes.Redundancy, $"fact:{fact.Id}",
                        $"Load-bearing fact '{fact.Statement}' has {sources.Count} independent channel(s); it needs at least two.{why}",
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
