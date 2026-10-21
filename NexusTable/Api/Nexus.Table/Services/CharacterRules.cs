using Nexus.Table.Models;

namespace Nexus.Table.Services;

/// <summary>
/// Checks a new character against the creation rules of the Vaesen rulebook (chapter 2):
/// attribute points by age (2-4 each, the archetype's main attribute up to 5), skill points by age
/// (at most 2 each, the main skill 3), skill points spent on Resources within the archetype's range,
/// one of the archetype's three talents, and one item per equipment pick.
/// </summary>
public static class CharacterRules
{
    public const int MaxNameLength = 80;
    public const int MaxTextLength = 2000;

    /// <summary>All problems found, readable for the player; empty when the character is valid.</summary>
    public static List<string> Validate(CreateCharacterRequest request, VaesenRules rules)
    {
        var errors = new List<string>();

        var name = request.Name?.Trim() ?? "";
        if (name.Length is 0 or > MaxNameLength)
        {
            errors.Add($"Give the character a name of at most {MaxNameLength} characters.");
        }

        var archetype = rules.FindArchetype(request.Archetype);
        if (archetype is null)
        {
            errors.Add("Choose an archetype.");
        }

        var ageGroup = rules.FindAgeGroup(request.AgeGroup);
        if (ageGroup is null)
        {
            errors.Add("Choose an age group.");
        }
        else if (request.Age < ageGroup.MinAge || (ageGroup.MaxAge is { } max && request.Age > max))
        {
            errors.Add($"A {ageGroup.Name.ToLowerInvariant()} character is {ageGroup.MinAge}{(ageGroup.MaxAge is { } m ? $"-{m}" : "+")} years old.");
        }

        if (archetype is null || ageGroup is null)
        {
            return errors;
        }

        ValidateAttributes(request, rules, archetype, ageGroup, errors);
        ValidateSkillsAndResources(request, rules, archetype, ageGroup, errors);

        if (!archetype.Talents.Contains(request.Talent, StringComparer.Ordinal))
        {
            errors.Add($"Choose one of the {archetype.Name}'s starting talents.");
        }

        RequireText(request.Motivation, "a motivation", errors);
        RequireText(request.Trauma, "the trauma that gave you the Sight", errors);
        RequireText(request.DarkSecret, "a dark secret", errors);
        RequireText(request.Memento, "a memento", errors);
        if (request.Description?.Length > MaxTextLength)
        {
            errors.Add($"The description can be at most {MaxTextLength} characters.");
        }

        if (request.Equipment.Count != archetype.Equipment.Count
            || request.Equipment.Where((item, i) => !archetype.Equipment[i].Contains(item, StringComparer.Ordinal)).Any())
        {
            errors.Add($"Choose one item for each of the {archetype.Name}'s {archetype.Equipment.Count} equipment picks.");
        }

        if (request.Relationships.Any(r => string.IsNullOrWhiteSpace(r.Name) || r.Text?.Length is null or 0 or > MaxTextLength))
        {
            errors.Add("Every relationship needs the other character's name and how you feel about them.");
        }

        return errors;
    }

    private static void ValidateAttributes(CreateCharacterRequest request, VaesenRules rules, RuleArchetype archetype, RuleAgeGroup ageGroup, List<string> errors)
    {
        var limits = rules.CreationLimits;
        if (request.Attributes.Keys.Any(id => !rules.IsAttribute(id)))
        {
            errors.Add("Unknown attribute.");
            return;
        }

        foreach (var attribute in rules.Attributes)
        {
            var value = request.Attributes.GetValueOrDefault(attribute.Id);
            var max = attribute.Id == archetype.MainAttribute ? limits.MainAttributeMax : limits.AttributeMax;
            if (value < limits.AttributeMin || value > max)
            {
                errors.Add($"{attribute.Name} must be {limits.AttributeMin}-{max}.");
            }
        }

        var spent = request.Attributes.Values.Sum();
        if (spent != ageGroup.AttributePoints)
        {
            errors.Add($"Spend exactly {ageGroup.AttributePoints} attribute points ({spent} spent).");
        }
    }

    private static void ValidateSkillsAndResources(CreateCharacterRequest request, VaesenRules rules, RuleArchetype archetype, RuleAgeGroup ageGroup, List<string> errors)
    {
        var limits = rules.CreationLimits;
        if (request.Skills.Keys.Any(id => rules.FindSkill(id) is null))
        {
            errors.Add("Unknown skill.");
            return;
        }

        foreach (var skill in rules.Skills)
        {
            var value = request.Skills.GetValueOrDefault(skill.Id);
            var max = skill.Id == archetype.MainSkill ? limits.MainSkillMax : limits.SkillMax;
            if (value < 0 || value > max)
            {
                errors.Add($"{skill.Name} must be 0-{max}.");
            }
        }

        if (request.Resources < archetype.Resources.Min || request.Resources > archetype.Resources.Max)
        {
            errors.Add($"A {archetype.Name}'s Resources are {archetype.Resources.Min}-{archetype.Resources.Max}.");
        }

        // Each skill point can instead raise Resources one step above the archetype's starting value.
        var spent = request.Skills.Values.Sum() + Math.Max(0, request.Resources - archetype.Resources.Min);
        if (spent != ageGroup.SkillPoints)
        {
            errors.Add($"Spend exactly {ageGroup.SkillPoints} skill points, Resources above {archetype.Resources.Min} included ({spent} spent).");
        }
    }

    private static void RequireText(string? value, string what, List<string> errors)
    {
        if (string.IsNullOrWhiteSpace(value) || value.Length > MaxTextLength)
        {
            errors.Add($"Write {what} (at most {MaxTextLength} characters).");
        }
    }

    /// <summary>The sheet of a valid request: every attribute and skill present, texts trimmed.</summary>
    public static Character Build(CreateCharacterRequest request, VaesenRules rules, Account account, string societyId) => new()
    {
        AccountId = account.Id,
        SocietyId = societyId,
        Name = request.Name.Trim(),
        Archetype = request.Archetype,
        AgeGroup = request.AgeGroup,
        Age = request.Age,
        Attributes = rules.Attributes.ToDictionary(a => a.Id, a => request.Attributes.GetValueOrDefault(a.Id)),
        Skills = rules.Skills.ToDictionary(s => s.Id, s => request.Skills.GetValueOrDefault(s.Id)),
        Resources = request.Resources,
        Talents = [request.Talent],
        Motivation = request.Motivation.Trim(),
        Trauma = request.Trauma.Trim(),
        DarkSecret = request.DarkSecret.Trim(),
        Relationships = request.Relationships.Select(r => new CharacterRelationship
        {
            CharacterId = r.CharacterId,
            Name = r.Name.Trim(),
            Text = r.Text.Trim(),
        }).ToList(),
        Memento = request.Memento.Trim(),
        Equipment = [.. request.Equipment],
        Description = request.Description?.Trim() ?? "",
        PortraitUrl = string.IsNullOrWhiteSpace(request.PortraitUrl) ? null : request.PortraitUrl.Trim(),
        CreatedBy = account.Id,
    };

    /// <summary>
    /// Marks a Condition. Already having every Condition of that type means the character becomes Broken instead.
    /// Returns false if the condition id is unknown.
    /// </summary>
    public static bool AddCondition(Character character, string condition, VaesenRules rules)
    {
        var physical = rules.IsPhysicalCondition(condition);
        if (!physical && !rules.IsMentalCondition(condition))
        {
            return false;
        }

        var ofType = physical ? rules.Conditions.Physical : rules.Conditions.Mental;
        if (ofType.All(c => character.Conditions.Contains(c.Id)))
        {
            if (physical)
            {
                character.PhysicallyBroken = true;
            }
            else
            {
                character.MentallyBroken = true;
            }

            return true;
        }

        if (!character.Conditions.Contains(condition))
        {
            character.Conditions.Add(condition);
        }

        return true;
    }

    /// <summary>Heals a Condition; healing any Condition of a type ends being Broken of that type.</summary>
    public static bool RemoveCondition(Character character, string condition, VaesenRules rules)
    {
        var physical = rules.IsPhysicalCondition(condition);
        if (!physical && !rules.IsMentalCondition(condition))
        {
            return false;
        }

        character.Conditions.Remove(condition);
        if (physical)
        {
            character.PhysicallyBroken = false;
        }
        else
        {
            character.MentallyBroken = false;
        }

        return true;
    }
}
