using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// The review of case 09: items resolve to the item itself, patches are routed by what they change, lever facts are
/// load-bearing, blocking holds unless the critic says why, offstage kin are kin, and the Society's earlier plots are
/// remembered.
/// </summary>
public sealed class FixesSixTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();

    [Fact]
    public void A_person_resolves_to_their_record_not_to_someone_s_relationship_with_them()
    {
        var packet = ValidatorTests.SampleCase();
        var (other, person) = (packet.Cast[0], packet.Cast[2]);
        other.Relationships.Insert(0, new CastRelationship { CharacterId = person.CharacterId, Nature = "The keeper of the old bargain" });

        var json = CaseItems.Json(packet, $"{person.CharacterId}.identity");

        Assert.NotNull(json);
        Assert.Contains(person.Name, json, StringComparison.Ordinal);
        Assert.DoesNotContain("keeper of the old bargain", json, StringComparison.Ordinal);
    }

    [Fact]
    public void A_fact_is_shown_with_the_channels_that_reveal_it()
    {
        var packet = ValidatorTests.SampleCase();
        var channel = packet.Channels.First(c => packet.FindFact(c.FactId) is not null);

        var json = CaseItems.Json(packet, channel.FactId);

        Assert.Contains($"\"id\":\"{channel.Id}\"", json, StringComparison.Ordinal);
    }

    [Fact]
    public void Patches_are_routed_by_what_they_change()
    {
        var packet = ValidatorTests.SampleCase();
        var fact = packet.Facts[0].Id;
        var issue = new CoherenceIssue
        {
            Code = "critic." + CriticChecks.FairClues, Source = IssueSources.Critic, SuggestedFix = "More ways in.",
            Patches =
            [
                // The critic said reword, but a fair-clues fix on a fact is a way in: only a new channel gives one.
                new PatchAsk { Target = fact, Fix = "A trace at the smithy.", Change = PatchChanges.Reword },
                new PatchAsk { Target = "npc-parsonage-vicar", Fix = "The vicar, who keeps the books.", Change = PatchChanges.Reword },
            ],
        };

        var parts = MysteryGenerator.PartsOf(packet, issue, light: true);

        Assert.Equal([PatchChanges.AddChannel, PatchChanges.AddPerson], parts.Select(p => p.Change));
        Assert.Equal([MindsStep.StepName, HumansStep.StepName], parts.Select(p => p.Owner));
        Assert.All(parts, p => Assert.Equal(FixPartStatuses.Pending, p.Status));

        // A reword of a fact for another check stays with the facts step.
        var reword = MysteryGenerator.PartsOf(packet, new CoherenceIssue
        {
            Code = "critic." + CriticChecks.Timeline, Source = IssueSources.Critic, Patches = [new PatchAsk { Target = fact, Fix = "Say May." }],
        }, light: true);
        Assert.Null(Assert.Single(reword).Change);
        Assert.Equal(FactsStep.StepName, reword[0].Owner);
    }

    [Fact]
    public void A_retold_plot_is_never_patched()
    {
        var parts = MysteryGenerator.PartsOf(ValidatorTests.SampleCase(), new CoherenceIssue
        {
            Code = "critic." + CriticChecks.Novelty, Source = IssueSources.Critic, Targets = ["truth.summary"], SuggestedFix = "Another story.",
        }, light: false);

        Assert.Equal(FixPartStatuses.Unrouted, Assert.Single(parts).Status);
        Assert.Contains(CriticChecks.Novelty, CriticChecks.BlockingByDefault);
    }

    [Fact]
    public void A_fact_a_lever_needs_is_load_bearing()
    {
        var packet = ValidatorTests.SampleCase();
        var lever = packet.ResolutionLevers.First(l => l.SolveDepth is SolveDepths.Partial or SolveDepths.Deep && l.RequiresFactIds.Count > 0);
        var fact = packet.FindFact(lever.RequiresFactIds[0])!;
        fact.LoadBearing = false;

        Derived.LoadBearing(packet);

        Assert.True(fact.LoadBearing);
    }

    [Fact]
    public void A_check_keeps_its_default_unless_the_critic_says_why_it_moves()
    {
        Assert.True(CriticStep.BlockingOf(new CriticCheck { Check = CriticChecks.FairClues, Impact = CriticImpacts.Advisory }));
        Assert.True(CriticStep.BlockingOf(new CriticCheck { Check = CriticChecks.FairClues, Impact = CriticImpacts.Default, ImpactReason = "" }));
        Assert.False(CriticStep.BlockingOf(new CriticCheck { Check = CriticChecks.FairClues, Impact = CriticImpacts.Advisory, ImpactReason = "Only a colour fact." }));
        Assert.True(CriticStep.BlockingOf(new CriticCheck { Check = CriticChecks.Mechanisms, Impact = CriticImpacts.Blocking, ImpactReason = "The flood cannot happen." }));
        Assert.False(CriticStep.BlockingOf(new CriticCheck { Check = CriticChecks.Mechanisms, Impact = CriticImpacts.Blocking }));
    }

    [Fact]
    public void An_offstage_son_named_for_his_father_is_not_a_name_clash()
    {
        var packet = ValidatorTests.SampleCase();
        var father = packet.Cast[1];
        father.Name = "Ola Mattsson";
        father.Relationships.Clear();
        packet.OffstagePeople.Add(new OffstagePerson { Id = "vilhelm", Name = "Vilhelm Olasson", Relation = "Ola's son, drowned in the race" });
        bool Echo() => new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), CheckScope.Cast)
            .Exists(i => i.Explanation.Contains("Vilhelm Olasson", StringComparison.Ordinal) && i.Explanation.Contains("Ola Mattsson", StringComparison.Ordinal));

        Assert.False(Echo());

        // Without a parent's word in the relation, the echo is still worth a warning.
        packet.OffstagePeople[^1].Relation = "a navvy from the railway";
        Assert.True(Echo());
    }

    [Fact]
    public void A_creature_the_Society_has_met_comes_with_another_reason_and_conflict()
    {
        var nack = Pack.FindVaesen("nack")!;
        var options = new MysteryOptions();
        var briefs = Enumerable.Range(0, 300)
            .Select(seed => Assignment.Assign(Pack, new SocietyHistory(), 10, seed, options))
            .Where(b => b.VaesenId == nack.Id)
            .ToList();
        Assert.NotEmpty(briefs);
        var first = briefs[0];
        // The näck met longest ago, every other creature since: it is the first one due again.
        var history = new SocietyHistory
        {
            SocietyId = "society-test",
            Mysteries = Pack.Vaesen.Where(v => v.Id != nack.Id)
                .Select((v, i) => new PastMystery { MysteryId = $"m{i + 1}", VaesenIds = [v.Id] })
                .Prepend(new PastMystery { MysteryId = "m0", VaesenIds = [nack.Id], ReasonKind = first.ReasonKind, ConflictArchetypeId = first.ConflictArchetypeIds[0] })
                .ToList(),
        };

        var again = Enumerable.Range(0, 300)
            .Select(seed => Assignment.Assign(Pack, history, 10, seed, options))
            .Where(b => b.VaesenId == nack.Id)
            .ToList();

        Assert.NotEmpty(again);
        if (nack.ReasonKinds.Count > 1)
        {
            Assert.All(again, b => Assert.NotEqual(first.ReasonKind, b.ReasonKind));
        }

        Assert.All(again.Where(b => b.ConflictArchetypeIds.Count > 1), b => Assert.NotEqual(first.ConflictArchetypeIds[0], b.ConflictArchetypeIds[0]));
    }
}
