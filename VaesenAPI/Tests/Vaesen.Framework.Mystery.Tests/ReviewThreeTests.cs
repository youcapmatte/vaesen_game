using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// Review 3 (case 07): a fix that breaks a check is repaired once or undone alone, never the whole round and never
/// sent twice; blocking critic issues mark a case for review; scenes do not give away gated clues; the hook's writer
/// knows what it says and names people as the case does; ids come from roles; Ekeby's map agrees with the real one.
/// </summary>
public sealed class ReviewThreeTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();

    private static List<CoherenceIssue> Validate(CasePacket packet, CheckScope scope = CheckScope.Full) =>
        new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), scope);

    private static async Task<(bool Success, string Error, Data.StoredCase? Case)> Generate(PipelineTests.ScriptedModel model)
    {
        var options = new MysteryOptions();
        var (generator, _, _) = PipelineTests.Build(options, model, out _);
        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = new SocietyHistory { SocietyId = "society-test" }, Month = 10, Seed = PipelineTests.MatchingSeed(options),
            VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });
        return (result.Success, result.ErrorMessage, result.Result?.Case);
    }

    [Fact]
    public async Task A_fix_that_breaks_a_check_is_repaired_once_then_undone_alone_and_the_others_are_kept()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.MixedWarnings = true;
        model.MindsFixesBreak = true;

        var result = await Generate(model);

        Assert.True(result.Success, result.Error);
        var packet = result.Case!.Packet;
        var issues = packet.Validation.Issues;

        // The rename (code) and the lever (the arc) are kept and verified; the clue fix broke f7's ways in and was undone.
        Assert.True(Assert.Single(issues, i => i.Code == "critic." + CriticChecks.Timeline).Applied);
        Assert.True(Assert.Single(issues, i => i.Code == "critic." + CriticChecks.Solves).Applied);
        var clue = Assert.Single(issues, i => i.Code == "critic." + CriticChecks.FairClues);
        Assert.False(clue.Applied);
        Assert.Contains("undone", clue.Unresolved, StringComparison.Ordinal);
        Assert.Contains("knowsFact:f6", packet.FindChannel("c10")!.Access.Requires);
        Assert.Contains("millrace", JsonConvert.SerializeObject(packet.Facts), StringComparison.Ordinal);
        Assert.Contains(result.Case.Log, e => e.Kind == LogEntryKinds.Undo);

        // The minds fix and one repair, then nothing more: an undone fix is not sent again.
        Assert.Equal(3, model.Calls.Count(c => c == MindsStep.StepName));

        // The clue warning was blocking and did not hold: the case passes but is marked for review.
        Assert.True(packet.Validation.Passed);
        Assert.True(packet.Validation.NeedsReview);
    }

    [Fact]
    public async Task A_case_whose_blocking_issues_were_fixed_needs_no_review()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.MixedWarnings = true;

        var result = await Generate(model);

        Assert.True(result.Success, result.Error);
        Assert.True(Assert.Single(result.Case!.Packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.FairClues).Applied);
        Assert.False(result.Case.Packet.Validation.NeedsReview);
    }

    [Fact]
    public void A_scene_that_shows_a_gated_reveal_is_an_error_and_an_ordinary_scene_is_not()
    {
        var packet = ValidatorTests.SampleCase();
        var gated = packet.Channels.First(c => c.Access.Requires.Exists(r => r.StartsWith("roll:", StringComparison.Ordinal)) && c.PlaceId is not null);
        gated.Reveal = "Wedged deep in a gap of the wall, at the height of a woman's hand, a small bundle of grey hair and grave earth.";
        Assert.DoesNotContain(Validate(packet, CheckScope.Hook), i => i.Location.StartsWith("scene:", StringComparison.Ordinal));

        var place = packet.Level.SceneDescriptions.Keys.First();
        packet.Level.SceneDescriptions[place] = "Rain on the slates. In one gap, at the height of a woman's hand, something small and grey is wedged deep, damp and matted.";
        var leak = Assert.Single(Validate(packet, CheckScope.Hook), i => i.Location == $"scene:{place}");
        Assert.True(leak.IsError);
        Assert.Contains(gated.Id, leak.Explanation, StringComparison.Ordinal);
        Assert.Equal(HookStep.StepName, MysteryGenerator.RepairOwner(leak));
    }

    [Fact]
    public void The_hook_names_people_as_the_case_does()
    {
        var packet = ValidatorTests.SampleCase();
        packet.Hook.Text += " My sister Maja, in service in Uppsala, read it to me.";

        var issue = Assert.Single(Validate(packet, CheckScope.Hook), i => i.Location == "hook" && i.Explanation.Contains("Maja", StringComparison.Ordinal));
        Assert.True(issue.IsError);
    }

    [Fact]
    public void The_hook_giver_knows_what_the_hook_states_or_is_flagged()
    {
        var packet = ValidatorTests.SampleCase();
        var giver = packet.FindCast(packet.Hook.HookGiverId)!;
        var stated = packet.Hook.AssertedFactIds.First();
        giver.Knows.RemoveAll(k => k.FactId == stated);

        var issue = Assert.Single(Validate(packet), i => i.Location == $"cast:{giver.CharacterId}" && i.Explanation.Contains("brought the Society", StringComparison.Ordinal));
        Assert.Contains($"{giver.CharacterId}.knows", issue.Targets!);
    }

    [Fact]
    public void A_beat_actor_cannot_put_out_what_someone_else_saw_but_may_do_their_own_deed()
    {
        var packet = ValidatorTests.SampleCase();
        Assert.DoesNotContain(Validate(packet), i => i.Code == IssueCodes.Provenance && i.Location.StartsWith("beat:", StringComparison.Ordinal));

        var beat = packet.Beats.First(b => b.ActorId is not null);
        var actor = packet.FindCast(beat.ActorId)!;
        var other = packet.Facts.First(f => f.Kind != FactKinds.Symptom && !actor.Knows.Exists(k => k.FactId == f.Id)
            && !f.Statement.Contains(NameRules.FirstName(actor.Name), StringComparison.Ordinal) && !f.Statement.Contains(NameRules.LastName(actor.Name), StringComparison.Ordinal));
        beat.EmitsFactIds.Add(other.Id);

        Assert.Single(Validate(packet), i => i.Code == IssueCodes.Provenance && i.Location == $"beat:{beat.Id}");
    }

    [Fact]
    public void A_teller_of_load_bearing_facts_needs_a_fear_and_an_action()
    {
        var packet = ValidatorTests.SampleCase();
        var teller = packet.Channels.Where(c => c.Kind == ChannelKinds.Testimony && packet.FindFact(c.FactId) is { LoadBearing: true })
            .Select(c => packet.FindCast(c.SourceCharacterId)).OfType<CastMember>().First();
        teller.Wants.Fears.Clear();
        teller.Wants.PermittedActions.Clear();

        var issue = Assert.Single(Validate(packet, CheckScope.Minds), i => i.Location == $"cast:{teller.CharacterId}" && i.Explanation.Contains("tells load-bearing facts", StringComparison.Ordinal));
        Assert.Contains($"{teller.CharacterId}.wants", issue.Targets!);
    }

    [Fact]
    public void Ids_come_from_place_and_role_and_a_name_made_id_is_rekeyed_before_use()
    {
        var taken = new HashSet<string>(StringComparer.Ordinal) { "npc-lyckan-crofter" };
        Assert.Equal("npc-lyckan-crofter-2", PersonIds.ForRole("Crofter", "croft-lyckan", taken));
        Assert.Equal("npc-karret-statare", PersonIds.ForRole("Statare", "Kärret", taken));
        Assert.True(PersonIds.NamesSomeone("npc-emil-pettersson", "Rasmus Pettersson"));
        Assert.False(PersonIds.NamesSomeone("npc-karret-statare", "Rasmus Pettersson"));
        Assert.True(PersonIds.NamesSomeone("npc-lyckan-groom-nils-per", "Nils-Per Ersson"));
        Assert.Equal("npc-parsonage-widow", PersonIds.ForRole("widow who takes in washing for the parsonage", "parsonage", taken));
        Assert.Equal("npc-farm-bailiff-wife", PersonIds.ForRole("bailiff's wife, runs the dairy", "farm", taken));

        var people = new List<CastDraft>
        {
            new() { CharacterId = "npc-emil-pettersson", Name = "Emil Pettersson", InstanceRef = InstanceRefs.New, HomePlaceId = "karret", Identity = new CharacterIdentity { Occupation = "statare" } },
            new() { CharacterId = "npc-hulda", Name = "Hulda Svensdotter", InstanceRef = InstanceRefs.New, Relationships = [new CastRelationship { CharacterId = "npc-emil-pettersson" }] },
        };
        var map = PersonIds.Rekey(people, new CasePacket(), []);

        Assert.Equal("npc-karret-statare", people[0].CharacterId);
        Assert.Equal("npc-karret-statare", people[1].Relationships[0].CharacterId);
        Assert.Equal("npc-karret-statare", map["npc-emil-pettersson"]);
    }

    [Fact]
    public void A_replacement_never_renames_an_id()
    {
        var packet = ValidatorTests.SampleCase();
        var draft = new MysteryDraft
        {
            Brief = new AssignmentBrief(), Pack = Pack, History = new SocietyHistory(), Level = Pack.FindLevel(packet.Brief.LevelId)!, Vaesen = Pack.FindVaesen("nack")!, Conflicts = [], Packet = packet,
        };
        var issue = new CoherenceIssue
        {
            Everywhere = true,
            Replacements = [new TextReplacement { From = "npc-case-maid", To = "npc-anna-lind" }, new TextReplacement { From = "mill race", To = "millrace" }],
        };

        Assert.True(TextReplacements.Apply(draft, issue) > 0);
        Assert.NotNull(draft.Packet.FindCast("npc-case-maid"));
        Assert.Null(draft.Packet.FindCast("npc-anna-lind"));
    }

    [Fact]
    public void Roster_people_get_lasting_ids_of_place_and_role()
    {
        var forsby = Pack.FindLevel("level-forsby-october")!;
        var roster = PipelineTests.ScriptedModel.Roster();
        var people = roster.People.Select(p => RosterBuilder.ToCharacter(p, forsby, roster.People)).ToList();
        RosterBuilder.StableIds(people, new SocietyWorld());

        Assert.Contains(people, p => p.Id == "npc-inn-serving-woman" && p.Name == "Greta Forsberg" && p.Fears.Count == 1);
        Assert.All(people, p => Assert.False(PersonIds.NamesSomeone(p.Id, p.Name), p.Id));
        Assert.Equal(people.Count, people.Select(p => p.Id).Distinct().Count());
    }

    [Fact]
    public void Ekeby_s_neighbours_agree_with_the_real_map_and_the_railway_of_April_1873()
    {
        var pack = Pack;
        var ekeby = pack.FindLevel("level-ekeby-april")!;
        Assert.DoesNotContain(ContentValidator.Validate(pack), i => i.Location == $"level:{ekeby.Id}" && (i.Explanation.Contains("straight line", StringComparison.Ordinal) || i.Explanation.Contains("bearing", StringComparison.Ordinal)));

        // Linköping, not Mjölby, is the station in 1873; the line south of it opens in 1874.
        var legs = ekeby.RoutesFor(1873, 4, pack.Setting).SelectMany(r => r.Legs).ToList();
        Assert.Contains(legs, l => l.To == "Linköping" && l.LineId == "ostra-stambanan-katrineholm-linkoping");
        Assert.DoesNotContain(legs, l => l.To == "Mjölby");
        Assert.False(pack.Setting.FindLine("ostra-stambanan-linkoping-nassjo")!.OpenIn(1873, 4));

        // A wrong direction is caught.
        var copy = JsonConvert.DeserializeObject<LevelDefinition>(JsonConvert.SerializeObject(ekeby))!;
        copy.Geography.Neighbours.First(n => n.Name == "Linköping").Distance = "about 25 km west";
        copy.Geography.Neighbours.First(n => n.Name == "Borensberg").Distance = "about 12 km north";
        var issues = ContentValidator.Validate(new ContentPack { Setting = pack.Setting, Vocabulary = pack.Vocabulary, PlaceTemplates = pack.PlaceTemplates, Levels = [copy], Vaesen = pack.Vaesen, Conflicts = pack.Conflicts, Characters = pack.Characters });
        Assert.Contains(issues, i => i.Explanation.Contains("Linköping is said to lie west", StringComparison.Ordinal));
        Assert.Contains(issues, i => i.Explanation.Contains("Borensberg is said to be 12 km away", StringComparison.Ordinal));
    }
}
