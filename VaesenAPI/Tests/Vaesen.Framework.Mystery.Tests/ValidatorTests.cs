using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;
using Xunit.Abstractions;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>The structural checks, on a hand-written case that passes and on broken copies of it.</summary>
public sealed class ValidatorTests(ITestOutputHelper output)
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();

    public static CasePacket SampleCase() =>
        JsonConvert.DeserializeObject<CasePacket>(File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "Fixtures", "sample-case-forsby.json")))!;

    public static ValidationContext ContextFor(CasePacket packet) => new()
    {
        Pack = Pack,
        Level = Pack.FindLevel(packet.Brief.LevelId)!,
        Vaesen = packet.Brief.VaesenIds.Select(id => Pack.FindVaesen(id)!).ToList(),
        Conflict = Pack.FindConflict(packet.Brief.ConflictArchetypeId),
    };

    private List<CoherenceIssue> Validate(CasePacket packet)
    {
        var issues = new CaseValidator().Validate(packet, ContextFor(packet));
        foreach (var issue in issues)
        {
            output.WriteLine(issue.ToString());
        }

        return issues;
    }

    private static void AssertError(List<CoherenceIssue> issues, string code, string? location = null) =>
        Assert.Contains(issues, i => i.IsError && i.Code == code && (location is null || i.Location == location));

    [Fact]
    public void The_sample_case_passes_every_structural_check()
    {
        var issues = Validate(SampleCase());

        Assert.DoesNotContain(issues, i => i.IsError);
    }

    [Fact]
    public void Every_issue_about_a_fixed_value_names_the_allowed_values()
    {
        var packet = SampleCase();
        packet.Facts[0].Kind = "lore";
        packet.Channels[0].Kind = "physical";
        packet.ResolutionLevers[0].SolveDepth = "surface";
        packet.Cast[0].Importance = "core";
        packet.Cast[0].Wants.PermittedActions[0].Desperation = "extreme";
        packet.Truth.Escalation[0].Register = "smell";

        var issues = Validate(packet).Where(i => i.Code == IssueCodes.Schema).ToList();

        Assert.Equal(6, issues.Count);
        Assert.All(issues, i => Assert.Contains("Allowed: ", i.Explanation, StringComparison.Ordinal));
        Assert.Contains(issues, i => i.Explanation == "Unknown solve depth 'surface'. Allowed: crude, partial, deep.");
        Assert.Contains(issues, i => i.Explanation.EndsWith("Allowed: symptom, cause, identity, alibi, motive, resolution, season, colour.", StringComparison.Ordinal));
    }

    [Fact]
    public void A_case_stored_before_the_renames_still_reads_its_importance_and_desperation()
    {
        var old = """{ "cast": [ { "characterId": "npc-a", "tier": "resident", "wants": { "permittedActions": [ { "id": "x", "tier": "high" } ] } } ] }""";

        var packet = JsonConvert.DeserializeObject<CasePacket>(old)!;
        var written = JsonConvert.SerializeObject(packet);

        Assert.Equal(CastImportance.Resident, packet.Cast[0].Importance);
        Assert.Equal(DesperationTiers.High, packet.Cast[0].Wants.PermittedActions[0].Desperation);
        Assert.DoesNotContain("\"tier\"", written, StringComparison.Ordinal);
    }

    [Fact]
    public void Players_reach_every_load_bearing_fact_and_the_beats()
    {
        var packet = SampleCase();

        var walk = new FactGraph(packet).Walk();

        Assert.All(packet.Facts.Where(f => f.LoadBearing), f => Assert.Contains(f.Id, walk.Known));
        Assert.Equal(["b1", "b2", "b3"], walk.FiredBeats.Order());
        Assert.True(walk.FinalTension >= packet.Climax.Threshold);
    }

    [Fact]
    public void A_fact_with_one_way_in_breaks_redundancy_and_order_independence()
    {
        var packet = SampleCase();
        packet.Channels.RemoveAll(c => c.Id == "c10");

        var issues = Validate(packet);

        AssertError(issues, IssueCodes.Redundancy, "fact:f7");
        AssertError(issues, IssueCodes.SinglePointOfFailure, "channel:c9");
    }

    [Fact]
    public void A_fact_gated_on_something_unreachable_is_unreachable()
    {
        var packet = SampleCase();
        foreach (var channel in packet.Channels.Where(c => c.FactId == "f9"))
        {
            channel.Access.Requires.Add("knowsFact:f99");
        }

        var issues = Validate(packet);

        AssertError(issues, IssueCodes.Unreachable, "fact:f9");
        AssertError(issues, IssueCodes.UnknownReference);
    }

    [Fact]
    public void Someone_cannot_testify_to_what_they_do_not_know()
    {
        var packet = SampleCase();
        packet.FindCast("npc-per-holm")!.Knows.RemoveAll(k => k.FactId == "f14");

        var issues = Validate(packet);

        // Both sit on Per, whose knowledge is missing, so a patch of his mind can add it.
        Assert.Contains(issues, i => i.IsError && i.Code == IssueCodes.Provenance && i.Location == "cast:npc-per-holm" && i.Explanation.Contains("channel c21", StringComparison.Ordinal));
        Assert.Contains(issues, i => i.IsError && i.Code == IssueCodes.Provenance && i.Location == "cast:npc-per-holm" && i.Explanation.Contains("told by Per Holm", StringComparison.Ordinal));
    }

    [Fact]
    public void A_giveaway_tell_cannot_be_found_at_once_in_an_identity_case()
    {
        var packet = SampleCase();
        packet.FindChannel("c11")!.Access.Requires.Clear();

        var issues = Validate(packet);

        AssertError(issues, IssueCodes.Giveaway, "channel:c11");
    }

    [Fact]
    public void Near_misses_are_repaired_in_code_and_reported_but_judgement_calls_are_left()
    {
        var arc = new Generation.ArcOutput
        {
            ResolutionLevers =
            [
                new ResolutionLever { Id = "r1", SolveDepth = " Surface " },
                new ResolutionLever { Id = "r2", SolveDepth = "answersGrievance" },
                new ResolutionLever { Id = "r3", SolveDepth = "half" },
                new ResolutionLever { Id = "r4", SolveDepth = "Deep" },
            ],
        };
        var facts = new Generation.FactsOutput { Facts = [new Generation.FactDraft { Id = "f1", Kind = "lore" }, new Generation.FactDraft { Id = "f2", Kind = "IDENTITY" }] };

        var repaired = Generation.EnumRepair.Normalize(arc).Concat(Generation.EnumRepair.Normalize(facts)).ToList();

        Assert.Equal([SolveDepths.Crude, SolveDepths.Deep, SolveDepths.Partial, SolveDepths.Deep], arc.ResolutionLevers.Select(l => l.SolveDepth));
        Assert.Equal(FactKinds.Identity, facts.Facts[1].Kind);
        Assert.Equal("lore", facts.Facts[0].Kind);
        Assert.Equal(5, repaired.Count);
        Assert.All(repaired, i => Assert.Equal(IssueCodes.Repaired, i.Code));
        Assert.All(repaired, i => Assert.False(i.IsError));
        Assert.Contains(repaired, i => i.Location == "ResolutionLevers[r3].SolveDepth" && i.Explanation == "'half' was read as 'partial'.");
    }

    [Fact]
    public void A_tie_may_reach_anyone_in_the_Society_world_but_not_nobody()
    {
        var packet = SampleCase();
        packet.Cast[0].Relationships = [new CastRelationship { CharacterId = "npc-sara-lindqvist" }, new CastRelationship { CharacterId = "npc-nobody" }];
        var context = ContextFor(packet);
        var withWorld = new ValidationContext { Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict, SocietyCharacterIds = ["npc-sara-lindqvist"] };

        var issues = new CaseValidator().Validate(packet, withWorld, CheckScope.Cast);

        Assert.DoesNotContain(issues, i => i.Explanation.Contains("npc-sara-lindqvist", StringComparison.Ordinal));
        Assert.Contains(issues, i => !i.IsError && i.Explanation.Contains("'npc-nobody', who is not in the cast or the Society's world", StringComparison.Ordinal));
    }

    [Fact]
    public void Critic_targets_go_to_the_step_that_owns_them()
    {
        Assert.Equal(("conflict", "truth.hiddenHandMotive"), Generation.CriticRouting.Owner("truth.hiddenHandMotive"));
        Assert.Equal(("escalation", "event:e4"), Generation.CriticRouting.Owner("e4"));
        Assert.Equal(("humans", "cast:npc-per-ers"), Generation.CriticRouting.Owner("npc-per-ers.identity"));
        Assert.Equal(("minds", "cast:npc-per-ers"), Generation.CriticRouting.Owner("npc-per-ers.knows"));
        Assert.Equal(("facts", "fact:f19"), Generation.CriticRouting.Owner("f19"));
        Assert.Equal(("facts", "decoy:d1"), Generation.CriticRouting.Owner("d1"));
        Assert.Equal(("minds", "channel:c43"), Generation.CriticRouting.Owner("c43"));
        Assert.Equal(("arc", "lever:r3"), Generation.CriticRouting.Owner("r3"));
        Assert.Equal(("arc", "climax"), Generation.CriticRouting.Owner("climax"));
        Assert.Equal(("hook", "scene:mill"), Generation.CriticRouting.Owner("scene:mill"));

        var routed = Generation.CriticRouting.Route([new CoherenceIssue
        {
            Code = "critic.solves", Location = "r3; motive", Explanation = "Add f24 to r3 and state the motive.", Source = IssueSources.Critic,
            Targets = ["r3", "truth.hiddenHandMotive"],
        }]);

        Assert.Equal(["conflict", "arc"], routed.Select(r => r.Step));
        Assert.Equal("lever:r3", Assert.Single(routed[1].Issues).Part.Location);
    }

    [Fact]
    public void A_revised_fact_sends_its_channels_and_knowers_after_it_and_a_patch_can_add_a_channel()
    {
        var before = SampleCase();
        var after = SampleCase();
        var fact = after.Facts.First(f => after.Channels.Exists(c => c.FactId == f.Id) && after.Cast.Exists(c => c.Knows.Exists(k => k.FactId == f.Id)));
        fact.Statement = "In the week of 20 September " + fact.Statement;

        var follow = Generation.CriticRouting.Dependents(before, after);

        Assert.All(follow, f => Assert.Equal("minds", f.Step));
        Assert.Equal(
            after.Channels.Where(c => c.FactId == fact.Id).Select(c => $"channel:{c.Id}")
                .Concat(after.Cast.Where(c => c.Knows.Exists(k => k.FactId == fact.Id)).Select(c => $"cast:{c.CharacterId}")),
            follow.Select(f => f.Issue.Location));
        Assert.Contains("week of 20 September", follow[0].Issue.Explanation, StringComparison.Ordinal);
        Assert.Empty(Generation.CriticRouting.Dependents(before, SampleCase()));

        // A fact no channel teaches: the critic targets channels, and the patch adds one.
        Assert.Equal(("minds", "new:channel"), Generation.CriticRouting.Owner("channels"));
        var channels = new List<CaseChannel> { new() { Id = "c1" } };
        var lists = new[] { new Generation.PatchList("channel", channels, c => ((CaseChannel)c).Id) };
        var ask = new[] { new CoherenceIssue { Code = "critic.fairClues", Location = "new:channel", Explanation = "Nothing teaches f15." } };
        Assert.True(Generation.Patch.Patchable(lists, ask, 0.25, anyCode: true));
        Assert.False(Generation.Patch.Patchable(lists, ask, 0.25));
        Generation.Patch.Merge(lists, [new Generation.PatchList("channel", new List<CaseChannel> { new() { Id = "c2" } }, c => ((CaseChannel)c).Id)]);
        Assert.Equal(["c1", "c2"], channels.Select(c => c.Id));
    }

    [Fact]
    public void Each_calls_schema_takes_the_cases_ids_as_enums()
    {
        var packet = SampleCase();
        var level = Pack.FindLevel(packet.Brief.LevelId)!;
        var draft = new Generation.MysteryDraft
        {
            Brief = new AssignmentBrief { LevelId = level.Id, VaesenId = "nack" },
            Pack = Pack,
            History = new SocietyHistory(),
            Level = level,
            Vaesen = Pack.FindVaesen("nack")!,
            Conflicts = [],
            Packet = packet,
        };
        string Enum(IEnumerable<string> ids) => new System.Text.Json.Nodes.JsonArray(ids.Distinct().Select(i => (System.Text.Json.Nodes.JsonNode)i!).ToArray()).ToJsonString();

        var ids = Generation.CaseIds.For(draft, "minds");
        var minds = Ai.StructuredSchema.For(typeof(Generation.MindsOutput), ids).ToJsonString();

        Assert.Contains(Enum(packet.Facts.Select(f => f.Id)), minds, StringComparison.Ordinal);
        Assert.Contains(Enum(packet.Cast.Select(c => c.CharacterId)), minds, StringComparison.Ordinal);
        Assert.Contains(Enum(level.Places.Select(p => p.Id)), minds, StringComparison.Ordinal);
        Assert.Contains(Enum(level.Places.SelectMany(p => p.AllAffordances()).Select(a => a.Id)), minds, StringComparison.Ordinal);
        Assert.Contains(Enum(AccessRequirement.Allowed(packet.Facts.Select(f => f.Id), level.Places.Select(p => p.Id))), minds, StringComparison.Ordinal);
        Assert.DoesNotContain("x-ids", minds, StringComparison.Ordinal);

        // The facts step names its own facts: their ids stay open there, and events are fixed.
        var facts = Ai.StructuredSchema.For(typeof(Generation.FactsOutput), Generation.CaseIds.For(draft, "facts")).ToJsonString();
        Assert.DoesNotContain(Enum(packet.Facts.Select(f => f.Id)), facts, StringComparison.Ordinal);
        Assert.Contains(Enum(packet.Truth.Escalation.Select(e => e.Id)), facts, StringComparison.Ordinal);

        // Without ids the schema is the plain one.
        Assert.DoesNotContain("x-ids", Ai.StructuredSchema.For<Generation.ArcOutput>().ToJsonString(), StringComparison.Ordinal);
    }

    [Fact]
    public void Gossip_from_bystanders_is_hearsay_and_never_a_facts_own_way_in()
    {
        var packet = SampleCase();
        var fact = packet.Facts.First(f => f.LoadBearing && packet.Channels.Count(c => c.FactId == f.Id) == 2);
        var tellers = packet.Channels.Where(c => c.FactId == fact.Id).ToList();
        foreach (var c in tellers)
        {
            c.Kind = ChannelKinds.Testimony;
            c.PlaceId = null;
            c.AffordanceId = null;
            var person = packet.Cast.First(m => m.Importance == CastImportance.Significant && m.CharacterId != tellers[0].SourceCharacterId || c == tellers[0]);
            c.SourceCharacterId ??= person.CharacterId;
            var teller = packet.FindCast(c.SourceCharacterId)!;
            teller.Knows.RemoveAll(k => k.FactId == fact.Id);
            teller.Knows.Add(new CastKnowledge { FactId = fact.Id, Source = "told:bystander:carter", When = "last week" });
        }

        var issues = Validate(packet);

        Assert.DoesNotContain(issues, i => i.Code == IssueCodes.Provenance && i.Explanation.Contains("carter", StringComparison.Ordinal));
        AssertError(issues, IssueCodes.Redundancy, $"fact:{fact.Id}");
    }

    [Fact]
    public void A_beat_or_climax_act_must_be_permitted_and_unlocked_by_its_tension()
    {
        var packet = SampleCase();
        var axel = packet.FindCast("npc-axel-wester")!;
        axel.Wants.PermittedActions.Add(new CharacterAction { Id = "wester-burn", Desperation = DesperationTiers.High, Description = "Burn the dam papers and the office." });
        packet.Beats.First(b => b.Id == "b2").ActionId = "wester-burn";
        packet.Climax.ActorId = axel.CharacterId;
        packet.Climax.ActionId = "wester-sabotage";

        var issues = Validate(packet);

        Assert.Contains(issues, i => !i.IsError && i.Location == "beat:b2" && i.Explanation.Contains("high-desperation", StringComparison.Ordinal) && i.Targets!.SequenceEqual(["b2"]));
        Assert.Contains(issues, i => !i.IsError && i.Location == "climax" && i.Explanation.Contains("outside Hugo Wester's permitted actions", StringComparison.Ordinal));
        Assert.Equal(("arc", "lever:r3"), Generation.CriticRouting.Owner("r3-d1"));
    }

    [Fact]
    public void A_home_trace_sits_in_its_owners_own_home()
    {
        var packet = SampleCase();
        var level = Pack.FindLevel(packet.Brief.LevelId)!;
        var (place, dwelling) = level.Places.SelectMany(p => p.AllAffordances().Select(a => (p, a))).First(x => Dwellings.Tags.Contains(x.a.Tag));
        var owner = packet.Cast.First(m => m.Importance == CastImportance.Significant);
        var other = packet.Cast.First(m => m.Importance == CastImportance.Significant && m != owner);
        other.HomePlaceId = place.Id;
        other.HomeAffordanceId = dwelling.Id;
        var trace = packet.Channels.First(c => c.Kind == ChannelKinds.Trace);
        trace.PlaceId = place.Id;
        trace.AffordanceId = dwelling.Id;
        trace.SourceCharacterId = owner.CharacterId;

        Assert.Contains(Validate(packet), i => !i.IsError && i.Location == $"channel:{trace.Id}" && i.Explanation.Contains($"in {other.Name}'s home", StringComparison.Ordinal));

        // Their own home, built by the engine and kept on the packet, is fine.
        var home = Dwellings.For(place.Id, owner.CharacterId, owner.Name);
        packet.Level.AddedAffordances.Add(new AddedAffordance { PlaceId = place.Id, Affordance = home });
        owner.HomePlaceId = place.Id;
        owner.HomeAffordanceId = home.Id;
        trace.AffordanceId = home.Contains[2].Id;
        var context = ContextFor(packet);
        var issues = new CaseValidator().Validate(packet, new ValidationContext { Pack = context.Pack, Level = level.ForCase(packet.Level), Vaesen = context.Vaesen, Conflict = context.Conflict });
        Assert.DoesNotContain(issues, i => i.Location == $"channel:{trace.Id}");
    }

    [Fact]
    public void A_new_significant_person_must_not_echo_a_first_name_from_the_societys_world()
    {
        var packet = SampleCase();
        var context = ContextFor(packet);
        var issues = new CaseValidator().Validate(packet, new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            WorldNames = [("npc-per-ers", "Per Ers"), ("npc-jonas-berg", "Jonas Berg")],
        });

        // An error at the humans step, so it is patched before any later step writes the name into prose.
        Assert.True(Assert.Single(issues, i => i.Location == "cast:npc-per-holm" && i.Explanation.Contains("Society's world", StringComparison.Ordinal)).IsError);
        Assert.Null(NameRules.FirstNameClash("Anna Lind", "Anders Berg"));
        Assert.Null(NameRules.FirstNameClash("Gunnar Ek", "Gustaf Roos"));
        Assert.NotNull(NameRules.FirstNameClash("Henning Wahl", "Henrik Ekelund"));
        Assert.NotNull(NameRules.FirstNameClash("Per Holm", "Per Ers"));
    }

    [Fact]
    public void Levers_need_deeds_with_real_ids_and_a_bargaining_creature_a_mind_that_matches_them()
    {
        var packet = SampleCase();
        packet.ResolutionLevers[0].RequiredDeeds.Clear();
        packet.ResolutionLevers[1].RequiredDeeds[0].Witnesses = ["npc-nobody"];
        packet.CreatureMinds[0].TermDeedIds = ["r1-d1"];

        var issues = Validate(packet);

        AssertError(issues, IssueCodes.Resolution, "lever:r1");
        AssertError(issues, IssueCodes.UnknownReference, "lever:r2");
        Assert.Contains(issues, i => i.IsError && i.Location == "creatureMinds" && i.Explanation.Contains("'r1-d1'", StringComparison.Ordinal));

        packet.CreatureMinds.Clear();
        Assert.Contains(Validate(packet), i => i.IsError && i.Location == "creatureMinds" && i.Explanation.Contains("has no mind", StringComparison.Ordinal));
    }

    [Fact]
    public void Names_that_sound_alike_anywhere_in_the_case_are_flagged()
    {
        Assert.NotNull(NameRules.SurnameEcho("Per Ers", "Jon Persson"));
        Assert.Null(NameRules.Clash("Erik Johansson", "Karin Eriksdotter"));
        Assert.NotNull(NameRules.Clash("Johan Berg", "Jonas Lund"));
        Assert.Null(NameRules.Clash("Anna Lind", "Karl Hagg"));
        Assert.Equal(["Jon Persson"], NameRules.Mentioned("The sawyer Jon Persson drowned below the falls."));

        var packet = SampleCase();
        packet.Cast[1].Name = "Per Ers";
        packet.Truth.Summary += " Years ago the sawyer Jon Persson drowned there.";

        // A first name in another's surname is only a warning: it may be a patronymic (Erik and Karin Eriksdotter).
        Assert.Contains(Validate(packet), i => !i.IsError && i.Location == "cast:npc-per-holm" && i.Explanation.Contains("'Per Ers' and 'Jon Persson'", StringComparison.Ordinal));
    }

    [Fact]
    public void A_lie_that_tells_the_fact_and_a_rank_off_the_list_are_warned()
    {
        var packet = SampleCase();
        var fact = packet.Facts[0];
        packet.Cast[0].Lies = [new CastLie { About = fact.Id, Story = fact.Statement }];
        packet.Cast[0].Knows.Add(new CastKnowledge { FactId = fact.Id, Source = KnowledgeSources.Witnessed });
        packet.Cast[0].Identity!.Rank = "duke of nowhere";

        var issues = Validate(packet);

        Assert.Contains(issues, i => !i.IsError && i.Location == $"cast:{packet.Cast[0].CharacterId}" && i.Explanation.Contains("is not a lie", StringComparison.Ordinal));
        Assert.Contains(issues, i => !i.IsError && i.Explanation.Contains("rank 'duke of nowhere'", StringComparison.Ordinal));
    }

    [Fact]
    public void Giveaway_and_deep_lore_are_derived_from_the_lore_ids()
    {
        var packet = SampleCase();
        foreach (var fact in packet.Facts)
        {
            (fact.Giveaway, fact.DeepLore) = (!fact.Giveaway, !fact.DeepLore);
        }

        Generation.Derived.LoreFlags(packet.Facts, Pack.FindVaesen("nack")!);

        // Back to what the fixture says, from the lore ids alone.
        var sample = SampleCase();
        Assert.All(packet.Facts, f => Assert.Equal((sample.FindFact(f.Id)!.Giveaway, sample.FindFact(f.Id)!.DeepLore), (f.Giveaway, f.DeepLore)));
        Assert.True(packet.FindFact("f8")!.Giveaway);
    }

    [Fact]
    public void The_engine_weights_facts_so_a_full_solution_passes_the_climax_and_scales_stray_beats()
    {
        var packet = SampleCase();
        packet.Beats[0].Threshold = 0.95;

        var adjusted = Generation.Derived.Tension(packet);
        var walk = new FactGraph(packet).Walk();

        // Nothing known on arrival or of colour weighs anything; decoys half, load-bearing facts double.
        Assert.Equal(packet.Tension.Start, packet.Tension.Arrival, 3);
        Assert.All(new[] { "f1", "f2", "f3", "f10", "f16" }, id => Assert.Equal(0, packet.Tension.FactWeights[id]));
        Assert.Equal(2 * packet.Tension.FactWeights["f15"], packet.Tension.FactWeights["f7"], 3);
        Assert.Equal(0.5 * packet.Tension.FactWeights["f15"], packet.Tension.FactWeights["f11"], 3);

        // The deep solve (r2: f5, f6, f9 and what gates them) ends 15% past the climax.
        Assert.Contains("f5", packet.Tension.DeepPath);
        Assert.Contains("f9", packet.Tension.DeepPath);
        Assert.Equal(packet.Climax.Threshold * 1.15, packet.Tension.DeepPathTotal, 2);
        Assert.All(packet.Beats, b => Assert.InRange(b.Threshold, packet.Tension.Start, packet.Climax.Threshold));
        Assert.Contains(adjusted, i => i.Code == IssueCodes.Beats && !i.IsError);
        Assert.True(walk.FinalTension >= packet.Climax.Threshold);
        Assert.All(packet.Beats, b => Assert.Contains(b.Id, walk.FiredBeats));
    }

    [Fact]
    public void A_place_gated_creature_needs_a_place_for_the_climax()
    {
        var packet = SampleCase();
        packet.Climax.Conditions.Place = null;

        AssertError(Validate(packet), IssueCodes.Beats, "climax");
    }

    [Fact]
    public void Clues_must_sit_in_affordances_the_place_has()
    {
        var packet = SampleCase();
        packet.FindChannel("c4")!.AffordanceId = "sawmill-wine-cellar";

        AssertError(Validate(packet), IssueCodes.AffordanceFit, "channel:c4");
    }

    [Fact]
    public void A_decoy_with_no_reachable_clearing_fact_cannot_be_resolved()
    {
        var packet = SampleCase();
        packet.Channels.RemoveAll(c => c.FactId is "f12" or "f13");

        AssertError(Validate(packet), IssueCodes.Decoy, "decoy:d1");
    }

    [Fact]
    public void The_hook_never_states_the_cause()
    {
        var packet = SampleCase();
        packet.Hook.AssertedFactIds.Add("f5");

        AssertError(Validate(packet), IssueCodes.Opening, "hook");
    }

    [Fact]
    public void A_hidden_hand_case_needs_a_crude_and_a_deep_solve_and_an_honest_clue_to_the_hand()
    {
        var packet = SampleCase();
        packet.Truth.PlotCentre = new PlotCentre { Kind = PlotCentreKinds.HiddenHand, CharacterId = "npc-axel-wester" };
        packet.Truth.HiddenHandMotive = "He wants the whole river for the saw.";
        packet.ResolutionLevers.RemoveAll(l => l.SolveDepth == SolveDepths.Crude);

        var issues = Validate(packet);

        AssertError(issues, IssueCodes.Resolution, "resolutionLevers");
        Assert.DoesNotContain(issues, i => i.Code == IssueCodes.FairPlay);
    }

    [Fact]
    public void Without_a_knowledge_gated_step_the_case_fails()
    {
        var packet = SampleCase();
        foreach (var channel in packet.Channels.Where(c => c.Kind == ChannelKinds.KnowledgeGated))
        {
            channel.Kind = ChannelKinds.Testimony;
        }

        AssertError(Validate(packet), IssueCodes.Giveaway, "channels");
    }

    [Fact]
    public void Access_requirements_parse()
    {
        Assert.Equal(new AccessRequirement("knowsFact", "f7", null), AccessRequirement.Parse("knowsFact:f7"));
        Assert.Equal(new AccessRequirement("trust", "2", 2), AccessRequirement.Parse("trust>=2"));
        Assert.Equal("dusk", AccessRequirement.Parse("time:dusk")!.Value);
        Assert.Null(AccessRequirement.Parse("charm the miller"));
    }
}
