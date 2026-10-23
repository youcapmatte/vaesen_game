using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Tests;

public sealed class AssignmentTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();
    private static readonly MysteryOptions Options = new();

    private static SocietyHistory History(params string[] faced) => new()
    {
        SocietyId = "society-test",
        Mysteries = faced.Select((v, i) => new PastMystery { MysteryId = $"m{i}", VaesenIds = [v], HookChannelId = "contact-letter", HookGiverType = "relative" }).ToList(),
    };

    [Fact]
    public void The_same_seed_gives_the_same_brief()
    {
        var a = Assignment.Assign(Pack, History(), 10, 42, Options);
        var b = Assignment.Assign(Pack, History(), 10, 42, Options);

        Assert.Equal(PromptBuilder.Json(a), PromptBuilder.Json(b));
    }

    [Fact]
    public void Every_brief_fits_together()
    {
        for (var seed = 0; seed < 200; seed++)
        {
            var brief = Assignment.Assign(Pack, History(), 10, seed, Options);
            var level = Pack.FindLevel(brief.LevelId)!;
            var vaesen = Pack.FindVaesen(brief.VaesenId)!;

            Assert.Equal(10, level.Month);
            Assert.True(ContentValidator.LevelFits(level, vaesen, Pack.Vocabulary));
            Assert.NotEmpty(brief.ConflictArchetypeIds);
            Assert.Contains(brief.ReasonKind, vaesen.ReasonKinds);
            Assert.All(brief.ConflictArchetypeIds, id => Assert.Contains(brief.ReasonKind, Pack.FindConflict(id)!.ReasonKinds));
            Assert.Equal(brief.HiddenHand, brief.PlotCentreKind == PlotCentreKinds.HiddenHand);
            Assert.Contains(brief.Focus, brief.HiddenHand ? new[] { CaseFocus.WhoWhy, CaseFocus.How } : new[] { CaseFocus.What, CaseFocus.How });
            Assert.NotEqual("", brief.HookChannelId);
        }
    }

    [Fact]
    public void A_creature_faced_recently_is_on_cooldown()
    {
        var history = History("nack", "werewolf", "nisse");

        for (var seed = 0; seed < 50; seed++)
        {
            Assert.Equal("skogsra", Assignment.Assign(Pack, history, 10, seed, Options).VaesenId);
        }
    }

    [Fact]
    public void The_last_hook_channel_is_not_used_twice_in_a_row()
    {
        for (var seed = 0; seed < 50; seed++)
        {
            Assert.NotEqual("contact-letter", Assignment.Assign(Pack, History("nack"), 10, seed, Options).HookChannelId);
        }
    }

    [Fact]
    public void A_month_without_a_fitting_level_falls_back_and_says_so()
    {
        var brief = Assignment.Assign(Pack, History(), 2, 1, Options);

        Assert.Contains(brief.Notes, n => n.Contains("month 2", StringComparison.Ordinal));
    }

    [Fact]
    public void Afflictions_carry_an_origin_and_its_hidden_hand()
    {
        var werewolf = Pack.FindVaesen("werewolf")!;
        var only = Pack.Usable(true);
        only.Vaesen = [werewolf];

        var briefs = Enumerable.Range(0, 100).Select(seed => Assignment.Assign(only, History(), 10, seed, Options)).ToList();
        var afflictions = briefs.Where(b => b.ReasonKind == ReasonKinds.Affliction).ToList();

        Assert.NotEmpty(afflictions);
        Assert.All(afflictions, b =>
        {
            var origin = werewolf.OriginSet.Single(o => o.Id == b.OriginId);
            Assert.Equal(origin.HiddenHand, b.HiddenHand);
        });
    }

    [Theory]
    [InlineData(typeof(ConflictOutput))]
    [InlineData(typeof(EscalationOutput))]
    [InlineData(typeof(HumansOutput))]
    [InlineData(typeof(CluesOutput))]
    [InlineData(typeof(CriticOutput))]
    [InlineData(typeof(VaesenDefinition))]
    [InlineData(typeof(ConflictArchetype))]
    [InlineData(typeof(CharacterDefinition))]
    [InlineData(typeof(WorldSeedOutput))]
    public void Step_outputs_have_schemas_structured_outputs_accept(Type type)
    {
        var schema = StructuredSchema.For(type);
        var json = schema.ToJsonString();

        // Structured outputs take references only into $defs, and every one must be defined there.
        var references = System.Text.RegularExpressions.Regex.Matches(json, "\"\\$ref\":\"([^\"]*)\"").Select(m => m.Groups[1].Value).ToList();
        Assert.All(references, r => Assert.StartsWith("#/$defs/", r, StringComparison.Ordinal));
        Assert.All(references, r => Assert.NotNull(schema["$defs"]?[r["#/$defs/".Length..]]));

        Assert.Equal("object", schema["type"]!.GetValue<string>());
        Assert.DoesNotContain("\"type\":[", json, StringComparison.Ordinal);
        Assert.DoesNotContain("\"additionalProperties\":true", json, StringComparison.Ordinal);
        Assert.DoesNotContain("\"additionalProperties\":{", json, StringComparison.Ordinal);
        Assert.Contains("\"additionalProperties\":false", json, StringComparison.Ordinal);
        Assert.DoesNotContain("isApproved", json, StringComparison.Ordinal);
        Assert.DoesNotContain("allLore", json, StringComparison.Ordinal);
    }

    [Fact]
    public void Fixed_value_fields_offer_the_model_only_their_allowed_values()
    {
        var clues = StructuredSchema.For(typeof(CluesOutput)).ToJsonString();
        var humans = StructuredSchema.For(typeof(HumansOutput)).ToJsonString();

        // A run invented "surface" and "shallow" for solve depths, "lore" for a fact kind and "core" for a cast tier.
        Assert.Contains($"\"enum\":[{string.Join(",", SolveDepths.All.Select(k => $"\"{k}\""))}]", clues, StringComparison.Ordinal);
        Assert.Contains($"\"enum\":[{string.Join(",", FactKinds.All.Select(k => $"\"{k}\""))}]", clues, StringComparison.Ordinal);
        Assert.Contains($"\"enum\":[{string.Join(",", CastImportance.All.Select(k => $"\"{k}\""))}]", humans, StringComparison.Ordinal);
    }

    [Fact]
    public void A_generated_record_is_checked_against_the_pack_it_joins()
    {
        var creature = Newtonsoft.Json.JsonConvert.DeserializeObject<VaesenDefinition>(Newtonsoft.Json.JsonConvert.SerializeObject(Pack.FindVaesen("nack")))!;
        creature.Id = "backahast";
        creature.RequiredAffordances = ["running-water", "no-such-tag"];

        var issues = Vaesen.Framework.Mystery.Services.ContentGenerator.Check(Pack, creature);

        Assert.Contains(issues, i => i.Code == IssueCodes.Vocabulary && i.Location == "vaesen:backahast");
        Assert.All(issues, i => Assert.StartsWith("vaesen:backahast", i.Location));
    }

    [Fact]
    public void Every_prompt_template_is_embedded_and_filled()
    {
        foreach (var name in new[] { "system", "conflict", "escalation", "humans", "clues", "critic" })
        {
            Assert.False(string.IsNullOrWhiteSpace(PromptBuilder.Template(name)));
        }

        Assert.DoesNotContain("{{", PromptBuilder.System(Pack.Setting), StringComparison.Ordinal);
    }
}
