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

        AssertError(issues, IssueCodes.Provenance, "channel:c21");
        AssertError(issues, IssueCodes.Provenance, "cast:npc-brita-holm");
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
    public void A_fact_resting_on_a_giveaway_tell_must_be_marked_giveaway()
    {
        var packet = SampleCase();
        packet.FindFact("f8")!.Giveaway = false;

        AssertError(Validate(packet), IssueCodes.Giveaway, "fact:f8");
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
