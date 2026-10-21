using Newtonsoft.Json;

namespace Nexus.Table.Models;

/// <summary>
/// The character rules of the Vaesen rulebook (Rules/vaesen-rules.json): archetypes, attributes, skills, talents,
/// Conditions and mementos. The web app builds the character wizard from this; the API validates against it.
/// Descriptions are short paraphrases, the mechanics are as in the book.
/// </summary>
public sealed class VaesenRules
{
    [JsonProperty("attributes")]
    public List<RuleAttribute> Attributes { get; set; } = [];

    [JsonProperty("skills")]
    public List<RuleSkill> Skills { get; set; } = [];

    [JsonProperty("ageGroups")]
    public List<RuleAgeGroup> AgeGroups { get; set; } = [];

    [JsonProperty("creationLimits")]
    public RuleCreationLimits CreationLimits { get; set; } = new();

    [JsonProperty("archetypes")]
    public List<RuleArchetype> Archetypes { get; set; } = [];

    [JsonProperty("talents")]
    public List<RuleTalent> Talents { get; set; } = [];

    [JsonProperty("conditions")]
    public RuleConditions Conditions { get; set; } = new();

    [JsonProperty("resourceLevels")]
    public List<RuleResourceLevel> ResourceLevels { get; set; } = [];

    [JsonProperty("mementos")]
    public List<RuleMemento> Mementos { get; set; } = [];

    [JsonProperty("dice")]
    public RuleDice Dice { get; set; } = new();

    public RuleArchetype? FindArchetype(string? id) => Archetypes.Find(a => a.Id == id);

    public RuleAgeGroup? FindAgeGroup(string? id) => AgeGroups.Find(a => a.Id == id);

    public RuleSkill? FindSkill(string? id) => Skills.Find(s => s.Id == id);

    public bool IsAttribute(string? id) => Attributes.Exists(a => a.Id == id);

    public bool IsPhysicalCondition(string? id) => Conditions.Physical.Exists(c => c.Id == id);

    public bool IsMentalCondition(string? id) => Conditions.Mental.Exists(c => c.Id == id);

    /// <summary>Physique and Precision are hurt by physical Conditions, Logic and Empathy by mental ones.</summary>
    public static bool IsPhysicalAttribute(string attribute) => attribute is "physique" or "precision";
}

public sealed class RuleAttribute
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";
}

public sealed class RuleSkill
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("attribute")]
    public string Attribute { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";
}

public sealed class RuleAgeGroup
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("minAge")]
    public int MinAge { get; set; }

    [JsonProperty("maxAge")]
    public int? MaxAge { get; set; }

    [JsonProperty("attributePoints")]
    public int AttributePoints { get; set; }

    [JsonProperty("skillPoints")]
    public int SkillPoints { get; set; }
}

public sealed class RuleCreationLimits
{
    [JsonProperty("attributeMin")]
    public int AttributeMin { get; set; } = 2;

    [JsonProperty("attributeMax")]
    public int AttributeMax { get; set; } = 4;

    [JsonProperty("mainAttributeMax")]
    public int MainAttributeMax { get; set; } = 5;

    [JsonProperty("skillMax")]
    public int SkillMax { get; set; } = 2;

    [JsonProperty("mainSkillMax")]
    public int MainSkillMax { get; set; } = 3;
}

public sealed class RuleArchetype
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("summary")]
    public string Summary { get; set; } = "";

    [JsonProperty("mainAttribute")]
    public string MainAttribute { get; set; } = "";

    [JsonProperty("mainSkill")]
    public string MainSkill { get; set; } = "";

    /// <summary>The three starting talents to choose one from.</summary>
    [JsonProperty("talents")]
    public List<string> Talents { get; set; } = [];

    [JsonProperty("resources")]
    public RuleRange Resources { get; set; } = new();

    /// <summary>One entry per pick; each lists the items to choose between ("A or B"), one item when there's no choice.</summary>
    [JsonProperty("equipment")]
    public List<List<string>> Equipment { get; set; } = [];

    [JsonProperty("names")]
    public RuleNames Names { get; set; } = new();

    [JsonProperty("motivations")]
    public List<string> Motivations { get; set; } = [];

    [JsonProperty("traumas")]
    public List<string> Traumas { get; set; } = [];

    [JsonProperty("darkSecrets")]
    public List<string> DarkSecrets { get; set; } = [];

    [JsonProperty("relationships")]
    public List<string> Relationships { get; set; } = [];
}

public sealed class RuleRange
{
    [JsonProperty("min")]
    public int Min { get; set; }

    [JsonProperty("max")]
    public int Max { get; set; }
}

public sealed class RuleNames
{
    [JsonProperty("first")]
    public List<string> First { get; set; } = [];

    [JsonProperty("last")]
    public List<string> Last { get; set; } = [];
}

public sealed class RuleTalent
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>The archetype it belongs to, or null for a general talent.</summary>
    [JsonProperty("archetype")]
    public string? Archetype { get; set; }

    [JsonProperty("effect")]
    public string Effect { get; set; } = "";
}

public sealed class RuleConditions
{
    [JsonProperty("physical")]
    public List<RuleCondition> Physical { get; set; } = [];

    [JsonProperty("mental")]
    public List<RuleCondition> Mental { get; set; } = [];
}

public sealed class RuleCondition
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";
}

public sealed class RuleResourceLevel
{
    [JsonProperty("value")]
    public int Value { get; set; }

    [JsonProperty("name")]
    public string Name { get; set; } = "";
}

public sealed class RuleMemento
{
    [JsonProperty("roll")]
    public int Roll { get; set; }

    [JsonProperty("item")]
    public string Item { get; set; } = "";
}

public sealed class RuleDice
{
    [JsonProperty("successOn")]
    public int SuccessOn { get; set; } = 6;

    [JsonProperty("pushing")]
    public string Pushing { get; set; } = "";

    [JsonProperty("fear")]
    public string Fear { get; set; } = "";

    [JsonProperty("notes")]
    public List<string> Notes { get; set; } = [];
}
