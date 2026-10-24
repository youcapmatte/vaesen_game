using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// Fixes 4, part B: the in-world calendar, the level cooldown counting queued cases, return visits, month variants,
/// a level's people kept per Society, and public facts a case explains.
/// </summary>
public sealed class LevelContinuityTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();
    private static readonly MysteryOptions Options = new();

    private static PastMystery Past(string id, string level, string vaesen, int year, int month, bool finished = false) => new()
    {
        MysteryId = id, LevelId = level, VaesenIds = [vaesen], HookChannelId = "contact-letter", HookGiverType = "relative", Year = year, Month = month, Finished = finished,
    };

    private static SocietyHistory History(params PastMystery[] past) => new() { SocietyId = "society-test", Mysteries = past.ToList() };

    [Fact]
    public void The_next_case_comes_later_on_another_level_with_another_creature_even_when_the_first_is_only_queued()
    {
        var history = History(Past("m1", "level-gruvbo-october", "werewolf", 1872, 10));
        for (var seed = 0; seed < 50; seed++)
        {
            var brief = Assignment.Assign(Pack, history, null, seed, Options);

            Assert.True(new InWorldDate(brief.Year, brief.Month) > new InWorldDate(1872, 10), $"seed {seed}: {brief.Year}-{brief.Month}");
            Assert.NotEqual("level-gruvbo-october", brief.LevelId);
            Assert.NotEqual("werewolf", brief.VaesenId);
        }
    }

    [Fact]
    public void A_pinned_month_before_the_last_case_moves_to_the_next_year()
    {
        var history = History(Past("m1", "level-gruvbo-october", "werewolf", 1872, 10));

        var brief = Assignment.Assign(Pack, history, 10, 1, Options);

        Assert.Equal((1873, 10), (brief.Year, brief.Month));
    }

    [Fact]
    public void A_level_cools_down_by_finished_mysteries_and_months_and_queued_cases_do_not_count_as_finished()
    {
        var setting = Pack.Setting;
        var date = new InWorldDate(1873, 6);
        var queued = History(
            Past("m1", "level-gruvbo-october", "werewolf", 1872, 10),
            Past("m2", "level-forsby-october", "nack", 1872, 11),
            Past("m3", "level-mossaby-october", "nisse", 1872, 12),
            Past("m4", "level-ekeby-april", "skogsra", 1873, 1));
        Assert.NotNull(Assignment.LevelCooldown("level-gruvbo-october", queued, date, setting, Options));

        foreach (var m in queued.Mysteries.Skip(1))
        {
            m.Finished = true;
        }

        Assert.Null(Assignment.LevelCooldown("level-gruvbo-october", queued, date, setting, Options));
        Assert.NotNull(Assignment.LevelCooldown("level-gruvbo-october", queued, new InWorldDate(1872, 12), setting, Options));
    }

    [Fact]
    public void A_return_visit_goes_back_to_the_level_and_names_the_cases_before()
    {
        var history = History(Past("m1", "level-gruvbo-october", "werewolf", 1872, 10));

        var brief = Assignment.Assign(Pack, history, null, 3, Options, returnLevelId: "level-gruvbo-october");

        Assert.Equal("level-gruvbo-october", brief.LevelId);
        Assert.True(brief.ReturnVisit);
        Assert.Equal(["m1"], brief.PreviousMysteryIds);
        Assert.Contains(brief.Month, Pack.FindLevel("level-gruvbo-october")!.Months);
        Assert.True(new InWorldDate(brief.Year, brief.Month) > new InWorldDate(1872, 10));
    }

    [Fact]
    public void A_month_variant_keeps_the_places_and_owners_and_changes_the_season()
    {
        var gruvbo = Pack.FindLevel("level-gruvbo-october")!;
        var winter = gruvbo.ForMonth(1);

        Assert.Contains(1, gruvbo.Months);
        Assert.Equal(1, winter.Month);
        Assert.StartsWith("January.", winter.SeasonNotes, StringComparison.Ordinal);
        Assert.Equal(gruvbo.Places.Select(p => (p.Id, p.OwnerRole)), winter.Places.Select(p => (p.Id, p.OwnerRole)));
        Assert.NotEqual(gruvbo.FindPlace("mine")!.Description, winter.FindPlace("mine")!.Description);
        Assert.Equal(gruvbo.PublicFacts.Count + 1, winter.PublicFacts.Count);
        Assert.Equal(gruvbo.SeasonNotes, gruvbo.ForMonth(10).SeasonNotes);
    }

    [Fact]
    public void Gruvbo_has_room_for_several_cases_and_every_named_place_an_owner()
    {
        var gruvbo = Pack.FindLevel("level-gruvbo-october")!;

        Assert.True(gruvbo.Places.Count(p => p.TemplateId is "farm" or "croft") >= 3);
        Assert.All(gruvbo.Places.Where(p => p.TemplateId != "church"), p => Assert.False(string.IsNullOrWhiteSpace(p.OwnerRole), p.Id));
        Assert.Contains(gruvbo.FindPlace("mine")!.AllAffordances(), a => a.Tag == Dwellings.WorkersCottages);
        Assert.DoesNotContain(ContentValidator.Validate(Pack), i => i.IsError);
    }

    [Fact]
    public void A_roster_must_give_every_held_place_one_holder_and_names_apart_by_ear()
    {
        var forsby = Pack.FindLevel("level-forsby-october")!;
        var world = new SocietyWorld { SocietyId = "society-test" };
        var roster = PipelineTests.ScriptedModel.Roster();
        Assert.DoesNotContain(RosterBuilder.Validate(roster, forsby, world), i => i.IsError);

        roster.People.RemoveAll(p => p.PlaceId == "mill");
        roster.People.Add(new RosterPerson { Id = "npc-erika-holm", Name = "Erika Holm", PlaceId = "inn", Role = RosterRoles.Keeper });
        var issues = RosterBuilder.Validate(roster, forsby, world);

        Assert.Contains(issues, i => i.IsError && i.Location == "place:mill");
        Assert.Contains(issues, i => i.IsError && i.Location == "place:inn");
        Assert.Contains(issues, i => i.IsError && i.Location == "roster:npc-erika-holm" && i.Explanation.Contains("Erik the smith", StringComparison.Ordinal));
    }

    [Fact]
    public void A_case_may_not_give_a_held_place_a_new_keeper()
    {
        var packet = ValidatorTests.SampleCase();
        var context = ValidatorTests.ContextFor(packet);
        var per = packet.FindCast("npc-per-holm")!;
        per.Keeps = "mill";
        var holders = new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            Holders = new() { ["mill"] = ("npc-viktor-strom", "Viktor Strömberg") },
        };

        Assert.Contains(new CaseValidator().Validate(packet, holders), i => i.IsError && i.Location == "cast:npc-per-holm" && i.Explanation.Contains("Viktor Strömberg", StringComparison.Ordinal));

        per.Keeps = null;
        Assert.DoesNotContain(new CaseValidator().Validate(packet, holders), i => i.Location == "cast:npc-per-holm" && i.Explanation.Contains("holds", StringComparison.Ordinal));
    }

    [Fact]
    public void A_public_fact_already_explained_is_flagged_and_an_unknown_one_is_an_error()
    {
        var packet = ValidatorTests.SampleCase();
        var context = ValidatorTests.ContextFor(packet);
        packet.PublicFactsExplained = [new PublicFactExplanation { PublicFactId = "pf1", Explanation = "It was the miller's doing." }];
        var settled = new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            SettledPublicFacts = [new SettledPublicFact("pf1", "", "A flood, nothing more.", "m1", Played: false)],
        };

        Assert.Contains(new CaseValidator().Validate(packet, settled), i => !i.IsError && i.Location == "publicFactsExplained" && i.Explanation.Contains("(queued)", StringComparison.Ordinal));

        packet.PublicFactsExplained[0].PublicFactId = "pf99";
        Assert.Contains(new CaseValidator().Validate(packet, settled), i => i.IsError && i.Location == "publicFactsExplained");
    }

    [Fact]
    public async Task A_level_s_people_are_written_once_kept_with_the_case_and_drawn_on_next_time()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, _) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);
        var history = new SocietyHistory { SocietyId = "society-test", SocietyName = "Test Society" };
        var request = new GenerationRequest
        {
            History = history, Month = 10, Seed = PipelineTests.MatchingSeed(new MysteryOptions()), VaesenId = "nack", LevelId = "level-forsby-october",
        };

        var first = await generator.GenerateAsync(request);
        Assert.True(first.Success, first.ErrorMessage);
        var world = (await worlds.GetAsync("society-test")).Result!;
        var roster = world.RosterFor("level-forsby-october")!;
        Assert.Equal(first.Result!.Case.Packet.MysteryId, roster.FirstMysteryId);
        Assert.Equal(8, world.PeopleOf("level-forsby-october").Count());
        Assert.Contains(first.Result.Case.Packet.Cast, c => c.CharacterId == "npc-greta-forsberg" && c.InstanceRef == InstanceRefs.Persistent);
        Assert.Contains(first.Result.Case.Log, e => e.Step == ResidentsStep.StepName && e.Kind == LogEntryKinds.Roster);

        var second = await generator.GenerateAsync(request);
        Assert.True(second.Success, second.ErrorMessage);
        Assert.Single(model.Calls, c => c == RosterBuilder.StepName);
        Assert.Contains("# The people of this place", model.Contexts[model.Calls.LastIndexOf(HumansStep.StepName)], StringComparison.Ordinal);
        world = (await worlds.GetAsync("society-test")).Result!;
        Assert.Equal(2, world.RosterFor("level-forsby-october")!.MysteryIds.Count);

        // Deleting the first unplayed case keeps the roster the second drew on; deleting both lets it go.
        Assert.False((await worlds.DiscardRosterAsync("society-test", first.Result.Case.Packet.MysteryId)).Result);
        Assert.NotNull((await worlds.GetAsync("society-test")).Result!.RosterFor("level-forsby-october"));
        Assert.True((await worlds.DiscardRosterAsync("society-test", second.Result!.Case.Packet.MysteryId)).Result);
        Assert.Null((await worlds.GetAsync("society-test")).Result!.RosterFor("level-forsby-october"));
    }

    [Fact]
    public async Task A_discarded_first_case_takes_its_level_s_people_with_it()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, _) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);
        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = new SocietyHistory { SocietyId = "society-test" }, Month = 10, Seed = PipelineTests.MatchingSeed(new MysteryOptions()),
            VaesenId = "nack", LevelId = "level-forsby-october",
        });
        Assert.True(result.Success, result.ErrorMessage);

        var discarded = await worlds.DiscardRosterAsync("society-test", result.Result!.Case.Packet.MysteryId);

        Assert.True(discarded.Result);
        var world = (await worlds.GetAsync("society-test")).Result!;
        Assert.Null(world.RosterFor("level-forsby-october"));
        Assert.Empty(world.PeopleOf("level-forsby-october"));
    }

    [Fact]
    public async Task A_played_case_s_explanation_of_a_public_fact_joins_the_overlay()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, store) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);
        var generated = await generator.GenerateAsync(new GenerationRequest
        {
            History = new SocietyHistory { SocietyId = "society-test" }, Month = 10, Seed = PipelineTests.MatchingSeed(new MysteryOptions()),
            VaesenId = "nack", LevelId = "level-forsby-october",
        });
        var stored = generated.Result!.Case;
        stored.Packet.PublicFactsExplained = [new PublicFactExplanation { PublicFactId = "pf2", Fact = "The flood of '68.", Explanation = "The company opened the sluice." }];
        await store.SaveAsync(stored);

        var closed = await worlds.CloseMysteryAsync("society-test", stored.Packet.MysteryId, new CloseMysteryRequest { LeverId = "r2" });

        Assert.True(closed.Success, closed.ErrorMessage);
        var world = (await worlds.GetAsync("society-test")).Result!;
        var entry = Assert.Single(world.Overlay, o => o.PublicFactId == "pf2");
        Assert.Equal("level-forsby-october", entry.LevelId);
        Assert.Contains("The company opened the sluice.", entry.Text, StringComparison.Ordinal);
    }
}
