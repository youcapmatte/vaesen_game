using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>
/// Review 2 (case 06): fixes are verified before they are called applied, mechanical fixes are done by code, one
/// person fetches the Society, every record is playable, 1872 names, house names, and a cap on a lever's facts.
/// </summary>
public sealed class ReviewTwoTests
{
    private static List<CoherenceIssue> Validate(CasePacket packet, CheckScope scope = CheckScope.Full) =>
        new CaseValidator().Validate(packet, ValidatorTests.ContextFor(packet), scope);

    private static Task<ResultOf> Generate(PipelineTests.ScriptedModel model, bool strict = false)
    {
        var options = new MysteryOptions { UnresolvedFixIsError = strict };
        var (generator, _, _) = PipelineTests.Build(options, model, out _);
        return generator.GenerateAsync(new GenerationRequest
        {
            History = new SocietyHistory { SocietyId = "society-test" }, Month = 10, Seed = PipelineTests.MatchingSeed(options),
            VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        }).ContinueWith(t => new ResultOf(t.Result.Success, t.Result.ErrorMessage, t.Result.Result?.Case));
    }

    private sealed record ResultOf(bool Success, string Error, Data.StoredCase? Case);

    [Fact]
    public async Task A_rename_is_done_by_code_everywhere_and_marked_applied_only_once_it_holds()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.RenameWarning = true;

        var result = await Generate(model);

        Assert.True(result.Success, result.Error);
        var packet = result.Case!.Packet;
        // The case's text, not its issue list (which keeps the critic's words).
        var text = JsonConvert.SerializeObject(new { packet.Truth, packet.Facts, packet.Channels, packet.Cast, packet.Hook, packet.Level });
        Assert.DoesNotContain("mill race", text, StringComparison.Ordinal);
        Assert.Contains("millrace", text, StringComparison.Ordinal);
        Assert.True(Assert.Single(packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.Timeline).Applied);
        Assert.Contains(result.Case.Log, e => e.Kind == LogEntryKinds.Replace && e.Passed);

        // No model was asked to do it; only the scoped re-check looked, to call it verified (fixes 5, section 4).
        Assert.Equal(["critic", FixVerifier.StepName], model.Calls[^2..]);
        Assert.Equal(FixLabels.Verified, Assert.Single(packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.Timeline).Label);
    }

    [Fact]
    public async Task A_fix_that_does_not_take_is_retried_then_marked_unresolved_never_applied()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.TargetedCritic = true;
        model.FixesDoNotTake = true;

        var result = await Generate(model);

        Assert.True(result.Success, result.Error);
        Assert.Equal(2, model.Calls.Count(c => c == FixVerifier.StepName));
        var issue = Assert.Single(result.Case!.Packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.Solves);
        Assert.False(issue.Applied);
        Assert.Equal("Still there.", issue.Unresolved);
    }

    [Fact]
    public async Task With_the_strict_option_a_fix_that_does_not_take_fails_the_case_visibly()
    {
        var model = PipelineTests.ScriptedModel.FromSample(criticFailsFirst: false);
        model.TargetedCritic = true;
        model.FixesDoNotTake = true;

        var result = await Generate(model, strict: true);

        Assert.False(result.Success);
        Assert.Contains("did not hold", result.Error, StringComparison.Ordinal);
        Assert.Equal(2, model.Calls.Count(c => c == FixVerifier.StepName));
        var issue = Assert.Single(result.Case!.Packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.Solves);
        Assert.False(issue.Applied);
        Assert.Equal("Still there.", issue.Unresolved);
    }

    [Fact]
    public void Someone_else_whose_record_says_they_fetched_the_Society_is_flagged_for_a_fix()
    {
        var packet = ValidatorTests.SampleCase();
        var other = packet.Cast.First(c => c.CharacterId != packet.Hook.HookGiverId && c.Importance == CastImportance.Significant);
        other.Secrets.Add("Drove south himself to find the Society.");

        var issue = Assert.Single(Validate(packet), i => i.Location == $"cast:{other.CharacterId}" && i.Code == IssueCodes.Opening);
        Assert.Contains($"{other.CharacterId}.secrets", issue.Targets!);
    }

    [Fact]
    public void A_fact_id_in_free_text_must_exist_and_be_known()
    {
        var packet = ValidatorTests.SampleCase();
        var member = packet.Cast.First(c => c.Importance == CastImportance.Significant && c.Knows.Count > 0);
        var unknown = packet.Facts.First(f => !member.Knows.Exists(k => k.FactId == f.Id)).Id;
        member.Secrets.Add($"Knows the wheel was stopped ({member.Knows[0].FactId}).");
        Assert.DoesNotContain(Validate(packet), i => i.Location == $"cast:{member.CharacterId}" && i.Explanation.Contains("cites", StringComparison.Ordinal));

        member.Secrets.Add($"Saw it ({unknown}), and also f999.");
        var cited = Validate(packet).Where(i => i.Location == $"cast:{member.CharacterId}" && i.Explanation.Contains("cites", StringComparison.Ordinal)).ToList();
        Assert.Contains(cited, i => i.Explanation.Contains("f999, which is no fact", StringComparison.Ordinal));
        Assert.Contains(cited, i => i.Explanation.Contains($"{unknown}, which they do not know", StringComparison.Ordinal));
    }

    [Fact]
    public void Everyone_significant_has_drives_fears_a_routine_and_something_in_mind()
    {
        var packet = ValidatorTests.SampleCase();
        var member = packet.Cast.First(c => c.Importance == CastImportance.Significant);
        member.Wants.Fears.Clear();
        member.Routine = "";
        Assert.Contains(Validate(packet, CheckScope.Cast), i => i.IsError && i.Location == $"cast:{member.CharacterId}" && i.Explanation.Contains("no fears, a routine", StringComparison.Ordinal));

        member.Knows.Clear();
        member.Thinks.Clear();
        Assert.Contains(Validate(packet, CheckScope.Minds), i => i.IsError && i.Location == $"cast:{member.CharacterId}" && i.Explanation.Contains("knows nothing", StringComparison.Ordinal));
    }

    [Fact]
    public void Patronymics_follow_1872_and_a_parent_and_child_are_not_called_alike()
    {
        var packet = ValidatorTests.SampleCase();
        var father = packet.Cast.First(c => c.CharacterId == "npc-case-miller");
        father.Name = "Erik Ersson";
        var wife = packet.Cast.First(c => c.CharacterId == "npc-case-miller-wife");
        wife.Name = "Kaisa Ersson";
        var son = packet.Cast.First(c => c.CharacterId == "npc-case-sawmill-manager");
        son.Name = "Vilho Erikson";
        son.Identity!.Age = 12;
        father.Identity!.Age = 40;
        son.Relationships.Add(new CastRelationship { CharacterId = father.CharacterId, Nature = "his father", Sentiment = 2 });

        var issues = Validate(packet, CheckScope.Cast);

        Assert.Contains(issues, i => i.Location == "cast:npc-case-sawmill-manager" && i.Explanation.Contains("Eriksson", StringComparison.Ordinal));
        Assert.Contains(issues, i => i.Location == "cast:npc-case-miller-wife" && i.Explanation.Contains("keeps her own", StringComparison.Ordinal));

        son.Name = "Vilho Eriksson";
        issues = Validate(packet, CheckScope.Cast);
        Assert.DoesNotContain(issues, i => i.Location == "cast:npc-case-sawmill-manager" && (i.Explanation.Contains("patronymic", StringComparison.Ordinal) || i.Explanation.Contains("is in", StringComparison.Ordinal)));
    }

    [Fact]
    public void A_persistent_child_s_fixed_name_is_not_asked_to_change_but_the_roster_writes_it_right()
    {
        var packet = ValidatorTests.SampleCase();
        var father = packet.Cast.First(c => c.CharacterId == "npc-case-miller");
        father.Name = "Petter Olsson";
        father.Identity!.Age = 40;
        var son = packet.Cast.First(c => c.CharacterId == "npc-case-sawmill-manager");
        son.Name = "Halvar Petersson";
        son.Identity!.Age = 12;
        son.InstanceRef = InstanceRefs.Persistent;
        son.Relationships.Add(new CastRelationship { CharacterId = father.CharacterId, Nature = "his father" });
        Assert.DoesNotContain(Validate(packet, CheckScope.Cast), i => i.Location == "cast:npc-case-sawmill-manager" && i.Explanation.Contains("patronymic", StringComparison.Ordinal));

        var roster = PipelineTests.ScriptedModel.Roster();
        var farmer = roster.People.First(p => p.Role == RosterRoles.Owner && p.PlaceId == "farm");
        roster.People.Add(new RosterPerson { Id = "npc-halvar-storgard", Name = "Halvar Ivarson", PlaceId = "farm", Role = RosterRoles.Household, HouseholdOf = farmer.Id, Relation = "eldest son", Drive = "to farm", Fear = "a bad harvest" });
        var issues = RosterBuilder.Validate(roster, ContentTests.LoadPack().FindLevel("level-forsby-october")!, new SocietyWorld());
        Assert.Contains(issues, i => i.IsError && i.Location == "roster:npc-halvar-storgard" && i.Explanation.Contains("Ivarsson", StringComparison.Ordinal));
    }

    [Fact]
    public void A_name_that_only_lives_in_a_house_name_is_not_a_person()
    {
        var packet = ValidatorTests.SampleCase();
        var context = ValidatorTests.ContextFor(packet);
        var level = JsonConvert.DeserializeObject<LevelDefinition>(JsonConvert.SerializeObject(context.Level))!;
        level.Places[0].Name = "Old Ylva's Cottage";
        packet.Truth.Escalation[0].Description += " Old Ylva set the bone.";
        var issues = new CaseValidator().Validate(packet, new ValidationContext { Pack = context.Pack, Level = level, Vaesen = context.Vaesen, Conflict = context.Conflict }, CheckScope.Hook);

        var issue = Assert.Single(issues, i => i.Explanation.Contains("only the name of Old Ylva's Cottage", StringComparison.Ordinal));
        Assert.Contains(packet.Truth.Escalation[0].Id, issue.Targets!);
    }

    [Fact]
    public void A_lever_may_require_at_most_eight_facts()
    {
        var packet = ValidatorTests.SampleCase();
        var lever = packet.ResolutionLevers[0];
        lever.RequiresFactIds = packet.Facts.Take(9).Select(f => f.Id).ToList();

        Assert.Contains(Validate(packet, CheckScope.Arc), i => i.IsError && i.Location == $"lever:{lever.Id}" && i.Explanation.Contains("at most 8", StringComparison.Ordinal));
    }

    [Fact]
    public void A_creature_target_goes_to_the_creature_step_as_a_patch()
    {
        Assert.Equal((CreatureStep.StepName, $"creatureMinds:{CriticRouting.CreatureItem}"), CriticRouting.Owner("nack creatureMinds.blames"));
        Assert.Equal((CreatureStep.StepName, $"creatureMinds:{CriticRouting.CreatureItem}"), CriticRouting.Owner("creatureMinds"));
    }

    [Fact]
    public void A_scoped_replacement_changes_only_its_targets()
    {
        var packet = ValidatorTests.SampleCase();
        var level = ContentTests.LoadPack().FindLevel(packet.Brief.LevelId)!;
        var draft = new MysteryDraft
        {
            Brief = new AssignmentBrief(), Pack = ContentTests.LoadPack(), History = new SocietyHistory(), Level = level, Vaesen = ContentTests.LoadPack().FindVaesen("nack")!, Conflicts = [], Packet = packet,
        };
        var target = packet.Facts.First(f => f.Statement.Contains("wheel", StringComparison.Ordinal));
        var issue = new CoherenceIssue { Targets = [target.Id], Replacements = [new TextReplacement { From = "wheel", To = "WHEEL" }] };

        Assert.True(TextReplacements.Apply(draft, issue).TrueForAll(r => r.Ok));
        Assert.Contains("WHEEL", draft.Packet.FindFact(target.Id)!.Statement, StringComparison.Ordinal);
        Assert.Contains(draft.Packet.Facts, f => f.Id != target.Id && f.Statement.Contains("wheel", StringComparison.Ordinal));
        Assert.True(TextReplacements.Holds(draft, issue));
    }
}
