using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// Fixes 7: parts go to the step whose answer holds their item, and one it cannot hold fails alone; list edits are
/// code; every path the critic writes resolves, the creature's knowledge too; the plot is checked for repeats right
/// after the conflict; every critic check gets an answer; a patch changes only what it was asked to.
/// </summary>
public sealed class FixesSevenTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();

    private static MysteryDraft Draft(CasePacket? packet = null, string hookChannel = "contact-letter")
    {
        packet ??= ValidatorTests.SampleCase();
        return new MysteryDraft
        {
            Brief = new AssignmentBrief { HookChannelId = hookChannel }, Pack = Pack, History = new SocietyHistory(), Level = Pack.FindLevel(packet.Brief.LevelId)!,
            Vaesen = Pack.FindVaesen("nack")!, Conflicts = [], Packet = packet, SystemPrompt = "system",
        };
    }

    [Theory]
    [InlineData("npc-case-maid.knows", "minds", "cast:npc-case-maid")]
    [InlineData("cast[id=npc-case-maid].knows[factId=f2].when", "minds", "cast:npc-case-maid")]
    [InlineData("npc-case-maid.thinks", "minds", "cast:npc-case-maid")]
    [InlineData("npc-case-maid.lies", "minds", "cast:npc-case-maid")]
    [InlineData("npc-case-maid.identity.rank", "humans", "cast:npc-case-maid")]
    [InlineData("cast[id=npc-case-maid].identity.rank", "humans", "cast:npc-case-maid")]
    [InlineData("npc-case-maid.secrets", "humans", "cast:npc-case-maid")]
    [InlineData("npc-case-maid.relationships", "humans", "cast:npc-case-maid")]
    [InlineData("c5", "minds", "channel:c5")]
    [InlineData("channels[id=c5].access.requires", "minds", "channel:c5")]
    [InlineData("creatureMinds[vaesenId=nack].knows[factId=f9].when", "creature", "creatureMinds:mind")]
    [InlineData("creatureMinds[vaesenId=nack].thinks[0]", "creature", "creatureMinds:mind")]
    [InlineData("nack.thinks", "creature", "creatureMinds:mind")]
    [InlineData("facts[id=f1].statement", "facts", "fact:f1")]
    [InlineData("f1", "facts", "fact:f1")]
    [InlineData("level.sceneDescriptions.mill", "hook", "scene:mill")]
    [InlineData("resolutionLevers[id=r1].requiredDeeds[id=r1-d1].performedBy", "arc", "lever:r1")]
    [InlineData("r1-d1", "arc", "lever:r1")]
    [InlineData("truth.summary", "conflict", "truth.summary")]
    public void Every_target_goes_to_the_step_that_writes_it(string target, string step, string location)
    {
        Assert.Equal((step, location), CriticRouting.Owner(target, ValidatorTests.SampleCase()));
    }

    [Fact]
    public void The_ownership_table_is_read_from_the_steps_schemas()
    {
        Assert.True(CriticRouting.MindParts.SetEquals(["knows", "thinks", "lies"]));
        Assert.Contains("cast.knows", CriticRouting.Owns[MindsStep.StepName]);
        Assert.Contains("channels", CriticRouting.Owns[MindsStep.StepName]);
        Assert.Contains("cast.identity", CriticRouting.Owns[HumansStep.StepName]);
        Assert.Contains("cast.secrets", CriticRouting.Owns[HumansStep.StepName]);
        Assert.DoesNotContain("cast.knows", CriticRouting.Owns[HumansStep.StepName]);
        Assert.All(["knows", "thinks", "blames", "terms", "redLines", "reactions"], part => Assert.Contains("creatureMinds." + part, CriticRouting.Owns[CreatureStep.StepName]));
        Assert.Contains("resolutionLevers", CriticRouting.Owns[ArcStep.StepName]);
    }

    [Theory]
    [InlineData("cast[id=npc-case-maid].identity.rank")]
    [InlineData("cast[id=npc-case-maid].knows[factId=f2].when")]
    [InlineData("creatureMinds[vaesenId=nack].knows[factId=f9].when")]
    [InlineData("creatureMinds[vaesenId=nack].thinks[0]")]
    [InlineData("channels[id=c5].access.requires")]
    [InlineData("facts[id=f1].statement")]
    [InlineData("level.sceneDescriptions.mill")]
    [InlineData("resolutionLevers[id=r1].requiredDeeds[id=r1-d1].performedBy")]
    public void Every_path_the_critic_writes_resolves(string path)
    {
        var root = JToken.FromObject(ValidatorTests.SampleCase());

        Assert.NotNull(CasePaths.Resolve(root, path));
        Assert.NotNull(CaseItems.Json(root, path));
    }

    [Fact]
    public void The_creature_s_knowledge_takes_a_when_and_its_step_keeps_it()
    {
        var draft = Draft();
        draft.Outputs[CreatureStep.StepName] = new CreatureOutput { CreatureMinds = JsonConvert.DeserializeObject<List<CreatureMind>>(JsonConvert.SerializeObject(draft.Packet.CreatureMinds))! };

        var set = TextReplacements.SetField(draft, new FieldSet { Path = "creatureMinds[vaesenId=nack].knows[factId=f9].when", Value = "the day of the flood" });

        Assert.True(set.Ok, set.ToString());
        Assert.Equal("the day of the flood", draft.Packet.CreatureMinds[0].Knows.Single(k => k.FactId == "f9").When);
        Assert.Equal("the day of the flood", ((CreatureOutput)draft.Outputs[CreatureStep.StepName]).CreatureMinds[0].Knows.Single(k => k.FactId == "f9").When);
    }

    [Fact]
    public void A_field_the_path_selects_by_is_set_and_read_back()
    {
        var draft = Draft();
        var (from, to) = (draft.Packet.Cast[0].CharacterId, draft.Packet.Cast[1].CharacterId);
        draft.Packet.CreatureMinds[0].Blames = [new CreatureBlame { CharacterId = from, Why = "The dam." }];

        var set = TextReplacements.SetField(draft, new FieldSet { Path = $"creatureMinds[vaesenId=nack].blames[characterId={from}].characterId", Value = to });

        Assert.True(set.Ok, set.ToString());
        Assert.Equal(to, Assert.Single(draft.Packet.CreatureMinds[0].Blames).CharacterId);
    }

    [Fact]
    public void A_fact_is_added_to_what_someone_knows_by_code_and_the_minds_answer_keeps_it()
    {
        var draft = Draft();
        var giver = draft.Packet.Cast.First(c => !c.Knows.Exists(k => k.FactId == "f4"));
        draft.Outputs[MindsStep.StepName] = new MindsOutput { Channels = draft.Packet.Channels };

        var added = TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddTo, Target = $"cast[id={giver.CharacterId}].knows", Value = "f4", Source = "told:press:upsala", When = "in the autumn" });

        Assert.True(added.Ok, added.ToString());
        var knows = draft.Packet.FindCast(giver.CharacterId)!.Knows.Single(k => k.FactId == "f4");
        Assert.Equal("told:press:upsala", knows.Source);
        // A person with no mind in the minds answer gets one, or the next minds patch would take the fact away again.
        Assert.Contains(((MindsOutput)draft.Outputs[MindsStep.StepName]).Minds, m => m.CharacterId == giver.CharacterId && m.Knows.Exists(k => k.FactId == "f4"));

        // The same fact twice, a fact the case lacks and a source that does not read are refused.
        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddTo, Target = $"cast[id={giver.CharacterId}].knows", Value = "f4" }).Ok);
        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddTo, Target = $"cast[id={giver.CharacterId}].knows", Value = "f99" }).Ok);
        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddTo, Target = $"cast[id={giver.CharacterId}].knows", Value = "f5", Source = "rumour" }).Ok);
        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddTo, Target = $"cast[id={giver.CharacterId}].knows", Value = "f5", Source = "told:npc-nobody" }).Ok);
    }

    [Fact]
    public void A_requirement_replaces_a_weaker_one_of_its_kind_and_one_can_be_removed()
    {
        var draft = Draft();
        Assert.Contains("trust>=1", draft.Packet.FindChannel("c5")!.Access.Requires);

        var stronger = TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddRequirement, Target = "channels[id=c5]", Value = "trust>=2" });
        var bare = TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddRequirement, Target = "c1", Value = "knowsFact:f2" });
        var removed = TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.RemoveFrom, Target = "channels[id=c2].access.requires", Value = "time:night" });

        Assert.True(stronger.Ok, stronger.ToString());
        Assert.Equal(["trust>=2"], draft.Packet.FindChannel("c5")!.Access.Requires.Where(r => r.StartsWith("trust", StringComparison.Ordinal)));
        Assert.True(bare.Ok, bare.ToString());
        Assert.Contains("knowsFact:f2", draft.Packet.FindChannel("c1")!.Access.Requires);
        Assert.True(removed.Ok, removed.ToString());
        Assert.DoesNotContain("time:night", draft.Packet.FindChannel("c2")!.Access.Requires);

        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.AddRequirement, Target = "c1", Value = "the vicar likes them" }).Ok);
        Assert.False(TextReplacements.Edit(draft, new ListEdit { Op = ListEditOps.RemoveFrom, Target = "channels[id=c2].access.requires", Value = "time:night" }).Ok);
    }

    [Fact]
    public void The_critic_s_list_edits_are_code_parts()
    {
        var packet = ValidatorTests.SampleCase();
        var parts = MysteryGenerator.PartsOf(packet, new CoherenceIssue
        {
            Code = "critic." + CriticChecks.FairClues, Source = IssueSources.Critic, SuggestedFix = "The vicar's trust.",
            ListEdits = [new ListEdit { Op = ListEditOps.AddRequirement, Target = "channels[id=c5]", Value = "trust>=2" }],
        }, light: true);

        var part = Assert.Single(parts);
        Assert.Equal(FixPartKinds.ListEdit, part.Kind);
        Assert.Equal("code", part.Owner);
    }

    [Fact]
    public void A_person_with_no_mind_entry_yet_and_the_creature_s_mind_are_held_by_their_steps()
    {
        var draft = Draft();
        draft.Outputs[MindsStep.StepName] = new MindsOutput { Channels = draft.Packet.Channels };
        draft.Outputs[CreatureStep.StepName] = new CreatureOutput { CreatureMinds = draft.Packet.CreatureMinds };
        var options = new MysteryOptions();
        var minds = new MindsStep(null!, new CaseValidator(), options);
        var creature = new CreatureStep(null!, new CaseValidator(), options);

        Assert.True(minds.Holds(draft, "cast:npc-case-maid"));
        Assert.True(minds.Holds(draft, "channel:c5"));
        Assert.True(minds.Holds(draft, "new:channel"));
        Assert.False(minds.Holds(draft, "cast:npc-nobody"));
        Assert.False(minds.Holds(draft, "creatureMinds:mind"));
        Assert.True(creature.Holds(draft, "creatureMinds:mind"));
    }

    [Fact]
    public void A_patch_changes_only_the_items_it_was_asked_about()
    {
        var original = new List<CaseChannel> { new() { Id = "c1", Reveal = "old one" }, new() { Id = "c2", Reveal = "old two" } };
        var answer = new List<CaseChannel> { new() { Id = "c1", Reveal = "new one" }, new() { Id = "c2", Reveal = "new two" } };

        Patch.Merge([new PatchList("channel", original, c => ((CaseChannel)c).Id)], [new PatchList("channel", answer, c => ((CaseChannel)c).Id)],
            only: new HashSet<string> { "channel:c1" });

        Assert.Equal(["new one", "old two"], original.Select(c => c.Reveal));
    }

    [Fact]
    public void A_hook_giver_who_forwards_a_cutting_read_it_in_the_paper()
    {
        var draft = Draft(hookChannel: "newspaper");
        var packet = draft.Packet;
        var giver = packet.FindCast(packet.Hook.HookGiverId)!;
        var fact = packet.Facts.First(f => !giver.Knows.Exists(k => k.FactId == f.Id)).Id;
        packet.Hook.AssertedFactIds = [fact];

        HookStep.GiverKnows(draft, packet);

        Assert.Equal(KnowledgeSources.Press("newspaper"), giver.Knows.Single(k => k.FactId == fact).Source);
        Assert.True(KnowledgeSources.IsHearsay(KnowledgeSources.Press("upsala")));
        Assert.Null(KnowledgeSources.Problem(packet, "told:press:upsala"));
        Assert.DoesNotContain(new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), CheckScope.Full),
            i => i.Code == IssueCodes.Provenance && i.Explanation.Contains("unreadable", StringComparison.Ordinal));
    }

    [Fact]
    public void A_plot_that_repeats_an_earlier_one_is_caught_by_its_signature()
    {
        PlotSignature Sign(string vaesen, string provocation, string bond, string victim, string cure) =>
            new() { VaesenId = vaesen, Provocation = provocation, Bond = bond, Victim = victim, Cure = cure, OneLine = "An earlier plot." };
        var earlier = new[]
        {
            new EarlierPlot("m6", "The Lad Given to the Forest", "skogsra", "level-forsby-october", 1873, 6, "bargain-comes-due", "…", false,
                Sign("skogsra", PlotProvocations.BargainSought, PlotBonds.Gift, PlotVictims.YoungMan, PlotCures.Release)),
        };

        // The same creature provoked the same way is a repeat, whatever else differs.
        var same = NoveltyCheck.Overlaps(Sign("skogsra", PlotProvocations.BargainSought, PlotBonds.Child, PlotVictims.Child, PlotCures.Church), earlier);
        Assert.Contains("The Lad Given to the Forest", Assert.Single(same).Explanation, StringComparison.Ordinal);
        Assert.Equal(NoveltyCheck.Code, same[0].Code);

        // Another provocation with the same creature, and two shared axes, is a new story.
        Assert.Empty(NoveltyCheck.Overlaps(Sign("skogsra", PlotProvocations.HarmToItsPlace, PlotBonds.Gift, PlotVictims.YoungMan, PlotCures.Rite), earlier));

        // Another creature with three axes the same is the same story told again.
        Assert.Single(NoveltyCheck.Overlaps(Sign("nack", PlotProvocations.BargainSought, PlotBonds.Gift, PlotVictims.YoungMan, PlotCures.Rite), earlier));
    }

    [Fact]
    public async Task A_check_the_critic_leaves_out_is_asked_again_alone()
    {
        var draft = Draft();
        var model = new CriticScript();

        var result = await new CriticStep(model, new MysteryOptions()).RunAsync(draft, 0, CancellationToken.None);

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(2, model.Instructions.Count);
        Assert.Contains("# Only these checks", model.Instructions[1], StringComparison.Ordinal);
        Assert.Contains(CriticChecks.HonestRedHerrings, model.Instructions[1], StringComparison.Ordinal);
        Assert.Contains(CriticChecks.Novelty, model.Instructions[1], StringComparison.Ordinal);

        // The re-asked herring check was answered; the novelty check was left out twice and blocks as not answered.
        Assert.DoesNotContain(result.Result!, i => i.Code == "critic." + CriticChecks.HonestRedHerrings);
        var unanswered = Assert.Single(result.Result!, i => i.Code == "critic." + CriticChecks.Novelty);
        Assert.True(unanswered.Blocking);
        Assert.StartsWith("Not answered", unanswered.Explanation, StringComparison.Ordinal);
    }

    [Fact]
    public void A_month_no_level_is_written_for_takes_the_nearest_month_and_asks_for_a_variant()
    {
        var options = new MysteryOptions();
        var month = Enumerable.Range(1, 12).First(m => !Pack.Levels.Any(l => l.Months.Contains(m)));

        var brief = Assignment.Assign(Pack, new SocietyHistory(), month, 7, options);

        Assert.Equal([$"{brief.LevelId}@{month}"], brief.WantedVariants);
        var vaesen = Pack.FindVaesen(brief.VaesenId)!;
        static int Distance(LevelDefinition l, int month) => l.Months.Min(m => Math.Min(Math.Abs(m - month), 12 - Math.Abs(m - month)));
        var chosen = Distance(Pack.FindLevel(brief.LevelId)!, month);
        Assert.DoesNotContain(Pack.Levels.Where(l => ContentValidator.LevelFits(l, vaesen, Pack.Vocabulary)), l => Distance(l, month) < chosen);
    }

    /// <summary>A critic that leaves out two checks, and on the re-ask answers only one of them.</summary>
    private sealed class CriticScript : IMysteryModel
    {
        public List<string> Instructions { get; } = [];

        public Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default) where T : class
        {
            Instructions.Add(request.Instruction);
            var output = Instructions.Count == 1
                ? new CriticOutput { Checks = CriticChecks.All.Where(c => c is not (CriticChecks.HonestRedHerrings or CriticChecks.Novelty)).Select(c => new CriticCheck { Check = c, Passed = true }).ToList() }
                : new CriticOutput { Checks = [new CriticCheck { Check = CriticChecks.HonestRedHerrings, Passed = true }] };
            var json = JsonConvert.SerializeObject(output);
            return Task.FromResult(new ResultBase<ModelReply<T>>().Ok(new ModelReply<T> { Value = JsonConvert.DeserializeObject<T>(json)!, RawJson = json, Model = "scripted" }));
        }
    }
}
