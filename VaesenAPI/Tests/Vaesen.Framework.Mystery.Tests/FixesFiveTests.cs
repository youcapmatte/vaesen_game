using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// Fixes 5: a code fix proves it changed the data, searches the whole packet, and sets fields as fields; an issue is
/// fixed only when every part is; four labels; blocking issues send a case to review; scenes at a place keep its secrets.
/// </summary>
public sealed class FixesFiveTests
{
    private static readonly ContentPack Pack = ContentTests.LoadPack();

    private static MysteryDraft Draft(CasePacket? packet = null)
    {
        packet ??= ValidatorTests.SampleCase();
        return new MysteryDraft
        {
            Brief = new AssignmentBrief(), Pack = Pack, History = new SocietyHistory(), Level = Pack.FindLevel(packet.Brief.LevelId)!,
            Vaesen = Pack.FindVaesen("nack")!, Conflicts = [], Packet = packet,
        };
    }

    [Fact]
    public void A_rank_is_set_as_a_field_and_read_back_and_a_value_off_the_list_is_refused()
    {
        var draft = Draft();
        var carl = draft.Packet.Cast.First(c => c.Identity is not null);
        carl.Identity!.Rank = "vagrant";

        var set = TextReplacements.SetField(draft, new FieldSet { Path = $"cast[id={carl.CharacterId}].identity.rank", Value = "worker" });

        Assert.True(set.Ok, set.Failed);
        Assert.Equal("worker", draft.Packet.FindCast(carl.CharacterId)!.Identity!.Rank);

        var wrong = TextReplacements.SetField(draft, new FieldSet { Path = $"cast[id={carl.CharacterId}].identity.rank", Value = "pauper-king" });
        Assert.False(wrong.Ok);
        Assert.Equal("worker", draft.Packet.FindCast(carl.CharacterId)!.Identity!.Rank);

        var nowhere = TextReplacements.SetField(draft, new FieldSet { Path = "cast[id=npc-nobody].identity.rank", Value = "worker" });
        Assert.Contains("No field", nowhere.Failed, StringComparison.Ordinal);
    }

    [Fact]
    public void A_knowledge_entry_s_when_is_set_by_its_fact()
    {
        var draft = Draft();
        var member = draft.Packet.Cast.First(c => c.Knows.Count > 0);
        var factId = member.Knows[0].FactId;

        var set = TextReplacements.SetField(draft, new FieldSet { Path = $"cast[id={member.CharacterId}].knows[factId={factId}].when", Value = "mid-May" });

        Assert.True(set.Ok, set.Failed);
        Assert.Equal("mid-May", draft.Packet.FindCast(member.CharacterId)!.Knows[0].When);
    }

    [Fact]
    public void A_pair_that_matches_nothing_fails_and_a_json_pattern_is_refused()
    {
        var draft = Draft();
        var results = TextReplacements.Apply(draft, new CoherenceIssue
        {
            Targets = ["f1"],
            Replacements = [new TextReplacement { From = "no such words here", To = "x" }, new TextReplacement { From = "\"rank\":\"vagrant\"", To = "\"rank\":\"worker\"" }],
        });

        Assert.Contains("Matched nothing", results[0].Failed, StringComparison.Ordinal);
        Assert.Contains("Refused", results[1].Failed, StringComparison.Ordinal);
    }

    [Fact]
    public void A_replacement_finds_copies_outside_its_targets()
    {
        var packet = ValidatorTests.SampleCase();
        var fact = packet.Facts[0];
        fact.Statement += " It began in late May.";
        packet.Decoys[0].Resolution = "The trouble began in late May, and the fiddler had nothing to do with it.";
        packet.CreatureMinds.Add(new CreatureMind { VaesenId = "nack", Knows = [new CastKnowledge { FactId = fact.Id, Source = KnowledgeSources.Witnessed, When = "since late May" }] });
        var draft = Draft(packet);

        var result = Assert.Single(TextReplacements.Apply(draft, new CoherenceIssue { Targets = [fact.Id], Replacements = [new TextReplacement { From = "late May", To = "mid-May" }] }));

        Assert.True(result.Ok, result.Failed);
        Assert.Equal(1, result.InScope);
        Assert.Contains(result.OutOfScopeReplaced, p => p.Contains("decoys", StringComparison.Ordinal));
        Assert.Contains(result.OutOfScopeReplaced, p => p.Contains("creatureMinds", StringComparison.Ordinal));
        Assert.DoesNotContain("late May", JsonConvert.SerializeObject(WithoutValidation(draft.Packet)), StringComparison.Ordinal);
    }

    private static object WithoutValidation(CasePacket packet) => new { packet.Truth, packet.Facts, packet.Channels, packet.Cast, packet.Decoys, packet.CreatureMinds, packet.Hook, packet.Level };

    [Fact]
    public void A_short_phrase_found_only_outside_the_targets_is_left_and_reported()
    {
        var packet = ValidatorTests.SampleCase();
        packet.Decoys[0].Resolution = "The wheel was old.";
        var draft = Draft(packet);
        var target = packet.Facts.First(f => !f.Statement.Contains("was old", StringComparison.Ordinal));

        var result = Assert.Single(TextReplacements.Apply(draft, new CoherenceIssue { Targets = [target.Id], Replacements = [new TextReplacement { From = "was old", To = "was new" }] }));

        Assert.False(result.Ok);
        Assert.Contains(result.OutOfScopeLeft, p => p.Contains("decoys", StringComparison.Ordinal));
        Assert.Equal("The wheel was old.", draft.Packet.Decoys[0].Resolution);
    }

    [Fact]
    public void An_issue_is_split_into_parts_and_a_target_the_case_lacks_is_unrouted()
    {
        var packet = ValidatorTests.SampleCase();
        var issue = new CoherenceIssue
        {
            Code = "critic." + CriticChecks.WorldConsistency, Source = IssueSources.Critic, Targets = ["f2", "c10"], SuggestedFix = "Several things.",
            Replacements = [new TextReplacement { From = "mill race", To = "millrace" }],
            SetFields = [new FieldSet { Path = "cast[id=npc-case-maid].identity.rank", Value = "worker" }],
            Patches = [new PatchAsk { Target = "f2", Fix = "Name the fäbod." }, new PatchAsk { Target = "c10", Fix = "Move it." }, new PatchAsk { Target = "c999", Fix = "Nothing." }],
        };

        var parts = MysteryGenerator.PartsOf(packet, issue, light: true);

        Assert.Equal([FixPartKinds.Replace, FixPartKinds.SetField, FixPartKinds.Patch, FixPartKinds.Patch, FixPartKinds.Patch], parts.Select(p => p.Kind));
        Assert.Equal(["code", "code", FactsStep.StepName, MindsStep.StepName, ""], parts.Select(p => p.Owner));
        Assert.Equal(FixPartStatuses.Unrouted, parts[^1].Status);

        // Nothing to route at all, in the light round: unrouted, so "not fixed".
        var bare = MysteryGenerator.PartsOf(packet, new CoherenceIssue { Code = "critic.solves", Source = IssueSources.Critic }, light: true);
        Assert.Equal(FixPartStatuses.Unrouted, Assert.Single(bare).Status);
    }

    [Fact]
    public async Task A_patch_that_changes_nothing_is_not_fixed_and_a_blocking_issue_left_open_sends_the_case_to_review()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.MixedWarnings = true;
        model.FixesChangeNothing = true;
        var options = new MysteryOptions();
        var (generator, _, store) = PipelineTests.Build(options, model, out _);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = new SocietyHistory { SocietyId = "society-test" }, Month = 10, Seed = PipelineTests.MatchingSeed(options),
            VaesenId = "nack", LevelId = "level-forsby-october",
        });

        Assert.True(result.Success, result.ErrorMessage);
        var issues = result.Result!.Case.Packet.Validation.Issues;
        var clue = Assert.Single(issues, i => i.Code == "critic." + CriticChecks.FairClues);
        Assert.Equal(FixLabels.NotFixed, clue.Label);
        Assert.Contains("as it was", clue.Unresolved, StringComparison.Ordinal);

        // The rename was code's and held: verified by the re-check.
        Assert.Equal(FixLabels.Verified, Assert.Single(issues, i => i.Code == "critic." + CriticChecks.Timeline).Label);

        // Every critic issue carries one of the four labels.
        Assert.All(issues.Where(i => i.Source == IssueSources.Critic && i.Location != "critic"), i => Assert.Contains(i.Label, new[] { FixLabels.Verified, FixLabels.Unverified, FixLabels.DidNotHold, FixLabels.NotFixed }));

        // fairClues blocks by default: the case is kept, but needs review, and is not queued to be played.
        Assert.True(result.Result.Case.Packet.Validation.NeedsReview);
        Assert.Equal(CaseStatuses.NeedsReview, result.Result.Case.Status);
        var listed = Assert.Single((await store.ListAsync("society-test")).Result!);
        Assert.Equal(CaseStatuses.NeedsReview, listed.Status);
        Assert.Contains("needs review, 1 blocking issue(s) open", CaseReport.ToMarkdown(result.Result.Case), StringComparison.Ordinal);
    }

    [Fact]
    public void Checks_block_or_advise_by_default()
    {
        Assert.Contains(CriticChecks.MindsAgree, CriticChecks.BlockingByDefault);
        Assert.Contains(CriticChecks.Scenes, CriticChecks.BlockingByDefault);
        Assert.DoesNotContain(CriticChecks.Mechanisms, CriticChecks.BlockingByDefault);
        Assert.DoesNotContain(CriticChecks.Leverage, CriticChecks.BlockingByDefault);
    }

    [Fact]
    public void A_scene_leaks_only_what_is_gated_at_its_own_place_and_names_the_sentence()
    {
        var packet = ValidatorTests.SampleCase();
        var teller = packet.Cast.First(c => c.HomePlaceId is not null && packet.Channels.Exists(ch => ch.SourceCharacterId == c.CharacterId && ch.Kind == ChannelKinds.Testimony));
        var channel = packet.Channels.First(ch => ch.SourceCharacterId == teller.CharacterId && ch.Kind == ChannelKinds.Testimony);
        channel.Access.Requires = ["trust>=2"];
        channel.Reveal = "She admits the bedclothes come up every Friday gritted with earth and pine needles, and mud dried under his nails.";
        packet.Level.SceneDescriptions[teller.HomePlaceId!] = "Smoke from the chimney. A young wife scrubs bedclothes gritted with earth and needles at the door.";

        var leak = Assert.Single(new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), CheckScope.Hook), i => i.Location == $"scene:{teller.HomePlaceId}" && i.Explanation.Contains(channel.Id, StringComparison.Ordinal));
        Assert.True(leak.IsError);
        Assert.Contains("scrubs bedclothes", leak.SuggestedFix, StringComparison.Ordinal);

        // The same words in a scene at another place are that place's business.
        var other = packet.Level.SceneDescriptions.Keys.First(k => k != teller.HomePlaceId && k != channel.PlaceId);
        packet.Level.SceneDescriptions[teller.HomePlaceId!] = "Smoke from the chimney.";
        packet.Level.SceneDescriptions[other] = "A young wife scrubs bedclothes gritted with earth and needles at the door.";
        Assert.DoesNotContain(new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), CheckScope.Hook), i => i.Location.StartsWith("scene:", StringComparison.Ordinal));
    }

    [Fact]
    public void A_clue_at_a_place_the_case_does_not_use_is_an_error()
    {
        var packet = ValidatorTests.SampleCase();
        var channel = packet.Channels.First(c => c.PlaceId is not null);
        packet.Level.PlacesUsed.Remove(channel.PlaceId!);

        Assert.Contains(new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet)), i => i.IsError && i.Location == $"channel:{channel.Id}" && i.Code == IssueCodes.AffordanceFit);
    }

    [Fact]
    public void A_role_id_stops_at_and_and_a_world_parent_and_child_are_not_mixed_up()
    {
        Assert.Equal("npc-farm-farmhand", PersonIds.ForRole("farmhand and would-be navvy", "farm", new HashSet<string>()));

        var packet = ValidatorTests.SampleCase();
        var (father, son) = (packet.Cast[1], packet.Cast[2]);
        father.Name = "Hans Olsson";
        son.Name = "Ola Hansson";
        father.Relationships.Clear();
        son.Relationships.Clear();
        bool Echo(ValidationContext context) => new CaseValidator().Validate(packet, context, CheckScope.Cast)
            .Exists(i => i.Explanation.Contains("'Hans Olsson' and 'Ola Hansson'", StringComparison.Ordinal) || i.Explanation.Contains("'Ola Hansson' and 'Hans Olsson'", StringComparison.Ordinal));
        var context = ValidatorTests.ContextFor(packet);
        Assert.True(Echo(context));

        var kin = new ValidationContext
        {
            Pack = context.Pack, Level = context.Level, Vaesen = context.Vaesen, Conflict = context.Conflict,
            ParentChild = [ValidationContext.PairKey(father.CharacterId, son.CharacterId)],
        };
        Assert.False(Echo(kin));
    }
}
