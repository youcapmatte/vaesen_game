using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>The Society's persistent world: seeded once, read by every case, written back to when a case closes.</summary>
public sealed class WorldTests
{
    private static readonly SocietyHistory History = new() { SocietyId = "society-test", SocietyName = "Test Society" };

    private static GenerationRequest Request(bool store = true) => new()
    {
        History = History, Month = 10, Seed = PipelineTests.MatchingSeed(new MysteryOptions()), VaesenId = "nack", LevelId = "level-forsby-october", Store = store,
    };

    [Fact]
    public async Task The_first_mystery_seeds_the_world_once_with_every_canonical_character()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, _) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);

        await generator.GenerateAsync(Request(store: false));
        await generator.GenerateAsync(Request(store: false));

        Assert.Single(model.Calls, c => c == WorldSeeder.StepName);
        var world = (await worlds.GetAsync("society-test")).Result!;
        var pack = ContentTests.LoadPack();
        Assert.All(pack.Characters, c => Assert.Contains(world.Characters, i => i.DefinitionId == c.Id && i.Tier == WorldTiers.Canonical));
        Assert.Equal(2, world.Characters.Count(c => c.Tier == WorldTiers.Core));
        Assert.Equal(1, world.Characters.Count(c => c.Tier == WorldTiers.Background));
        Assert.Equal(-2, world.Find("constable-hagg")!.Attitude);

        // Cases are offered the world's people, with their attitudes.
        var caseContext = model.Contexts.Last(c => c.Contains("# Persistent characters", StringComparison.Ordinal));
        Assert.Contains("npc-sara-lindqvist", caseContext, StringComparison.Ordinal);
        Assert.Contains("constable-hagg", caseContext, StringComparison.Ordinal);
    }

    [Fact]
    public async Task A_seed_whose_ties_point_nowhere_is_rerolled_with_the_issue()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var broken = PipelineTests.ScriptedModel.Seed();
        broken.Core[0].Relationships.Add(new WorldRelationship { CharacterId = "npc-nobody", Nature = "ghost", Sentiment = 0 });
        model.FirstSeed = broken;
        var (generator, _, _) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);

        await generator.GenerateAsync(Request(store: false));

        Assert.Equal(2, model.Calls.Count(c => c == WorldSeeder.StepName));
        Assert.Contains("npc-nobody", model.Instructions[1], StringComparison.Ordinal);
        Assert.True((await worlds.GetAsync("society-test")).Success);
    }

    [Fact]
    public void A_dead_character_cannot_be_cast_again()
    {
        var packet = ValidatorTests.SampleCase();
        packet.Cast.Add(new CastMember
        {
            CharacterId = "constable-hagg", Name = "Karl Edvard Hägg", InstanceRef = InstanceRefs.Persistent, PlotRole = "official", Importance = CastImportance.Significant,
        });
        var context = ValidatorTests.ContextFor(packet);
        var dead = new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            UnavailableCharacterIds = ["constable-hagg"],
        };

        var issues = new CaseValidator().Validate(packet, dead, CheckScope.Cast);

        Assert.Contains(issues, i => i.IsError && i.Code == IssueCodes.CastCoherence && i.Location == "cast:constable-hagg");
        Assert.DoesNotContain(new CaseValidator().Validate(packet, context, CheckScope.Cast), i => i.IsError && i.Location == "cast:constable-hagg");
    }

    [Fact]
    public void A_new_character_cannot_take_the_id_of_someone_in_the_world()
    {
        var packet = ValidatorTests.SampleCase();
        var context = ValidatorTests.ContextFor(packet);
        var withWorld = new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            SocietyCharacterIds = ["npc-per-holm"],
        };

        var issues = new CaseValidator().Validate(packet, withWorld, CheckScope.Cast);

        Assert.Contains(issues, i => i.IsError && i.Location == "cast:npc-per-holm");
    }

    [Fact]
    public async Task Closing_a_mystery_writes_attitudes_fates_memories_and_new_people_back()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, store) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);
        var generated = await generator.GenerateAsync(Request());
        Assert.True(generated.Success, generated.ErrorMessage);
        var stored = generated.Result!.Case;

        // Give the deep solve a measurable cost, as a generated case would.
        var lever = stored.Packet.ResolutionLevers.Single(l => l.Id == "r2");
        lever.Consequences.Attitudes.Single(a => a.CharacterId == "npc-axel-wester").Delta = -2;
        lever.Consequences.Fates.Add(new CharacterFate { CharacterId = "npc-lars-spelman", Status = CharacterStatuses.Departed, Reason = "Left for Uppsala to play for money." });
        await store.SaveAsync(stored);

        var closed = await worlds.CloseMysteryAsync("society-test", stored.Packet.MysteryId,
            new CloseMysteryRequest { Outcome = CaseStatuses.Solved, LeverId = "r2", Notes = "Played on a Thursday." });

        Assert.True(closed.Success, closed.ErrorMessage);
        Assert.Equal(CaseStatuses.Solved, closed.Result!.Status);
        Assert.Equal("deep", closed.Result.Closing!.SolveDepth);
        Assert.NotEmpty(closed.Result.Closing.Changes);

        var world = (await worlds.GetAsync("society-test")).Result!;
        var anna = world.Find("npc-anna-lind")!;
        Assert.Equal(WorldTiers.Core, anna.Tier);
        Assert.Equal(1, anna.Attitude);
        Assert.Contains(anna.Memories, m => m.Text.Contains("Played on a Thursday.", StringComparison.Ordinal));
        Assert.Contains(anna.Knowledge, k => k.FactId == "f2");
        Assert.Equal(-2, world.Find("npc-axel-wester")!.Attitude);
        Assert.Equal(CharacterStatuses.Departed, world.Find("npc-lars-spelman")!.Status);
        Assert.Equal(WorldTiers.LocationResident, world.Find("npc-greta-forsberg")!.Tier);
        Assert.Equal("level-forsby-october/inn", world.Find("npc-greta-forsberg")!.Home);
        Assert.Single(world.Overlay);
        Assert.Contains(stored.Packet.MysteryId, world.ClosedMysteryIds);
        Assert.DoesNotContain(WorldValidator.Validate(world, ContentTests.LoadPack()), i => i.IsError);

        var again = await worlds.CloseMysteryAsync("society-test", stored.Packet.MysteryId, new CloseMysteryRequest());
        Assert.False(again.Success);
    }

    [Fact]
    public async Task The_next_case_is_offered_the_promoted_people_but_not_the_departed()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, store) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out var worlds);
        var stored = (await generator.GenerateAsync(Request())).Result!.Case;
        stored.Packet.ResolutionLevers.Single(l => l.Id == "r2").Consequences.Fates.Add(
            new CharacterFate { CharacterId = "npc-lars-spelman", Status = CharacterStatuses.Departed, Reason = "Gone." });
        await store.SaveAsync(stored);
        var closed = await worlds.CloseMysteryAsync("society-test", stored.Packet.MysteryId, new CloseMysteryRequest { LeverId = "r2" });
        Assert.True(closed.Success, closed.ErrorMessage);

        await generator.GenerateAsync(Request(store: false));

        var context = model.Contexts.Last(c => c.Contains("# Persistent characters", StringComparison.Ordinal));
        var offered = context[context.IndexOf("# Persistent characters", StringComparison.Ordinal)..];
        Assert.Contains("npc-anna-lind", offered, StringComparison.Ordinal);
        Assert.Contains("# Dead or gone", offered, StringComparison.Ordinal);
        Assert.Contains("Lars Spelman (npc-lars-spelman): departed", offered, StringComparison.Ordinal);
        Assert.Contains("# What the Society's past mysteries have changed", offered, StringComparison.Ordinal);
    }

    [Fact]
    public async Task Prompts_and_instructions_are_kept_with_the_case_for_review()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        var (generator, _, store) = PipelineTests.Build(new MysteryOptions { UseCritic = false }, model, out _);

        var packet = (await generator.GenerateAsync(Request())).Result!.Case.Packet;
        var stored = (await store.GetAsync("society-test", packet.MysteryId)).Result!;

        Assert.Contains("Principles every case must keep", stored.Prompts.System, StringComparison.Ordinal);
        Assert.Contains("# The creature", stored.Prompts.Context, StringComparison.Ordinal);
        Assert.All(stored.Log.Where(e => e.Kind != LogEntryKinds.Check), e => Assert.False(string.IsNullOrWhiteSpace(e.Instruction)));
        Assert.Contains(stored.Log, e => e.Step == "facts" && e.Instruction!.Contains("# Step: facts", StringComparison.Ordinal));
    }
}
