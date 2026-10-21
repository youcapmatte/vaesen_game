using Nexus.Table.Models;
using Nexus.Table.Services;

namespace Nexus.Table.Tests;

public sealed class CharacterRulesTests
{
    private static readonly VaesenRules Rules = new RulesService(new FixedDice()).Rules;

    [Fact]
    public void Rules_file_is_complete()
    {
        Assert.Equal(10, Rules.Archetypes.Count);
        Assert.Equal(12, Rules.Skills.Count);
        Assert.Equal(36, Rules.Mementos.Count);
        Assert.All(Rules.Archetypes, a =>
        {
            Assert.Equal(3, a.Talents.Count);
            Assert.All(a.Talents, t => Assert.Contains(Rules.Talents, talent => talent.Id == t));
            Assert.NotNull(Rules.FindSkill(a.MainSkill));
            Assert.True(Rules.IsAttribute(a.MainAttribute));
        });
    }

    [Fact]
    public void A_character_built_by_the_book_is_valid() =>
        Assert.Empty(CharacterRules.Validate(Requests.Academic(), Rules));

    [Fact]
    public void Attribute_points_must_match_the_age()
    {
        var request = Requests.Academic();
        request.Attributes["empathy"] = 3;

        Assert.Contains(CharacterRules.Validate(request, Rules), e => e.Contains("15 attribute points"));
    }

    [Fact]
    public void Only_the_main_attribute_may_be_5()
    {
        var request = Requests.Academic();
        request.Attributes = new() { ["physique"] = 5, ["precision"] = 2, ["logic"] = 4, ["empathy"] = 4 };

        Assert.Contains(CharacterRules.Validate(request, Rules), e => e.StartsWith("Physique must be 2-4"));
    }

    [Fact]
    public void Only_the_main_skill_may_be_3()
    {
        var request = Requests.Academic();
        request.Skills = new() { ["learning"] = 2, ["investigation"] = 3, ["observation"] = 2, ["vigilance"] = 2, ["inspiration"] = 1 };

        Assert.Contains(CharacterRules.Validate(request, Rules), e => e.StartsWith("Investigation must be 0-2"));
    }

    [Fact]
    public void Skill_points_can_raise_resources_up_to_the_archetype_maximum()
    {
        var request = Requests.Academic();
        request.Skills["inspiration"] = 0;
        request.Resources = 5;
        Assert.Empty(CharacterRules.Validate(request, Rules));

        request.Skills["vigilance"] = 0;
        request.Resources = 7;
        Assert.Contains(CharacterRules.Validate(request, Rules), e => e.Contains("Resources are 4-6"));
    }

    [Fact]
    public void Talent_and_equipment_come_from_the_archetype()
    {
        var request = Requests.Academic();
        request.Talent = "tough";
        request.Equipment = ["Rifle", "Writing utensils", "Liquor"];

        var errors = CharacterRules.Validate(request, Rules);

        Assert.Contains(errors, e => e.Contains("starting talents"));
        Assert.Contains(errors, e => e.Contains("equipment picks"));
    }

    [Fact]
    public void A_fourth_condition_of_a_type_breaks_the_character()
    {
        var character = new Character();
        foreach (var condition in new[] { "angry", "frightened", "hopeless" })
        {
            CharacterRules.AddCondition(character, condition, Rules);
        }

        Assert.False(character.MentallyBroken);
        CharacterRules.AddCondition(character, "angry", Rules);
        Assert.True(character.MentallyBroken);
        Assert.False(character.PhysicallyBroken);
    }
}
