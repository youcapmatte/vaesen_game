using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Models;

/// <summary>
/// A player character, built as in the Vaesen rulebook (chapter 2). Belongs to an account and to the Society.
/// Attribute, skill, talent, archetype and condition ids are the ids in vaesen-rules.json.
/// Id: "character-{guid}".
/// </summary>
public sealed class Character : BaseCore
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("archetype")]
    public string Archetype { get; set; } = "";

    /// <summary>"young", "middleAged" or "old"; decides the attribute and skill points.</summary>
    [JsonProperty("ageGroup")]
    public string AgeGroup { get; set; } = "";

    [JsonProperty("age")]
    public int Age { get; set; }

    /// <summary>physique, precision, logic, empathy: 2-4, the archetype's main attribute up to 5.</summary>
    [JsonProperty("attributes")]
    public Dictionary<string, int> Attributes { get; set; } = [];

    /// <summary>The twelve skills, 0-5 (at creation at most 2, the main skill 3).</summary>
    [JsonProperty("skills")]
    public Dictionary<string, int> Skills { get; set; } = [];

    /// <summary>Standard of living, 1 (destitute) to 8 (filthy rich).</summary>
    [JsonProperty("resources")]
    public int Resources { get; set; }

    [JsonProperty("talents")]
    public List<string> Talents { get; set; } = [];

    [JsonProperty("motivation")]
    public string Motivation { get; set; } = "";

    /// <summary>The event that gave the character the Sight.</summary>
    [JsonProperty("trauma")]
    public string Trauma { get; set; } = "";

    [JsonProperty("darkSecret")]
    public string DarkSecret { get; set; } = "";

    [JsonProperty("relationships")]
    public List<CharacterRelationship> Relationships { get; set; } = [];

    [JsonProperty("memento")]
    public string Memento { get; set; } = "";

    [JsonProperty("equipment")]
    public List<string> Equipment { get; set; } = [];

    /// <summary>How the character looks and comes across, as told to the table.</summary>
    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>Ids of the Conditions the character suffers now (exhausted, battered, wounded, angry, frightened, hopeless).</summary>
    [JsonProperty("conditions")]
    public List<string> Conditions { get; set; } = [];

    /// <summary>All physical Conditions and then one more: needs medical attention.</summary>
    [JsonProperty("physicallyBroken")]
    public bool PhysicallyBroken { get; set; }

    [JsonProperty("mentallyBroken")]
    public bool MentallyBroken { get; set; }

    [JsonProperty("experience")]
    public int Experience { get; set; }

    /// <summary>The once-per-mystery Advantage (+2 dice once per session), if the character has one.</summary>
    [JsonProperty("advantage", NullValueHandling = NullValueHandling.Ignore)]
    public string? Advantage { get; set; }

    /// <summary>Lasting effects of critical injuries and mental trauma.</summary>
    [JsonProperty("insightsAndDefects")]
    public List<string> InsightsAndDefects { get; set; } = [];

    [JsonProperty("portraitUrl", NullValueHandling = NullValueHandling.Ignore)]
    public string? PortraitUrl { get; set; }

    [JsonProperty("notes")]
    public string Notes { get; set; } = "";
}

/// <summary>How a character feels about another player character.</summary>
public sealed class CharacterRelationship
{
    /// <summary>The other character, once it exists; null for one written before the other player made theirs.</summary>
    [JsonProperty("characterId", NullValueHandling = NullValueHandling.Ignore)]
    public string? CharacterId { get; set; }

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}

/// <summary>A new character: the choices of the rulebook's twelve creation steps.</summary>
public sealed class CreateCharacterRequest
{
    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("archetype")]
    public string Archetype { get; set; } = "";

    [JsonProperty("ageGroup")]
    public string AgeGroup { get; set; } = "";

    [JsonProperty("age")]
    public int Age { get; set; }

    [JsonProperty("attributes")]
    public Dictionary<string, int> Attributes { get; set; } = [];

    [JsonProperty("skills")]
    public Dictionary<string, int> Skills { get; set; } = [];

    [JsonProperty("resources")]
    public int Resources { get; set; }

    [JsonProperty("talent")]
    public string Talent { get; set; } = "";

    [JsonProperty("motivation")]
    public string Motivation { get; set; } = "";

    [JsonProperty("trauma")]
    public string Trauma { get; set; } = "";

    [JsonProperty("darkSecret")]
    public string DarkSecret { get; set; } = "";

    [JsonProperty("relationships")]
    public List<CharacterRelationship> Relationships { get; set; } = [];

    [JsonProperty("memento")]
    public string Memento { get; set; } = "";

    /// <summary>One item per equipment pick of the archetype, in order.</summary>
    [JsonProperty("equipment")]
    public List<string> Equipment { get; set; } = [];

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("portraitUrl")]
    public string? PortraitUrl { get; set; }
}

/// <summary>Changes during play: the free-text parts of the sheet. Fields left null are kept.</summary>
public sealed class UpdateCharacterRequest
{
    [JsonProperty("motivation")]
    public string? Motivation { get; set; }

    [JsonProperty("darkSecret")]
    public string? DarkSecret { get; set; }

    [JsonProperty("relationships")]
    public List<CharacterRelationship>? Relationships { get; set; }

    [JsonProperty("memento")]
    public string? Memento { get; set; }

    [JsonProperty("equipment")]
    public List<string>? Equipment { get; set; }

    [JsonProperty("description")]
    public string? Description { get; set; }

    [JsonProperty("advantage")]
    public string? Advantage { get; set; }

    [JsonProperty("portraitUrl")]
    public string? PortraitUrl { get; set; }

    [JsonProperty("notes")]
    public string? Notes { get; set; }
}

/// <summary>Marks or clears one Condition, e.g. { "condition": "frightened", "active": true }.</summary>
public sealed class SetConditionRequest
{
    [JsonProperty("condition")]
    public string Condition { get; set; } = "";

    [JsonProperty("active")]
    public bool Active { get; set; }
}
