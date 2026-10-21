using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Models;

/// <summary>
/// A skill test, rolled on the server so nobody can cheat, then shown to the whole table.
/// The pool is attribute + skill + modifiers (at least one die); every six is a success.
/// Id: "roll-{guid}".
/// </summary>
public sealed class Roll : BaseCore
{
    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("characterName")]
    public string CharacterName { get; set; } = "";

    /// <summary>The skill tested, or null for a plain attribute test (e.g. a Fear test with Logic or Empathy).</summary>
    [JsonProperty("skill", NullValueHandling = NullValueHandling.Ignore)]
    public string? Skill { get; set; }

    [JsonProperty("attribute")]
    public string Attribute { get; set; } = "";

    /// <summary>How the pool was built, in order, e.g. Logic +3, Learning +2, Frightened -1.</summary>
    [JsonProperty("pool")]
    public List<RollPoolPart> Pool { get; set; } = [];

    [JsonProperty("poolSize")]
    public int PoolSize { get; set; }

    /// <summary>The faces of the first throw.</summary>
    [JsonProperty("dice")]
    public List<int> Dice { get; set; } = [];

    /// <summary>The faces after pushing (the sixes kept, the rest thrown again); null if not pushed.</summary>
    [JsonProperty("pushedDice", NullValueHandling = NullValueHandling.Ignore)]
    public List<int>? PushedDice { get; set; }

    [JsonProperty("successes")]
    public int Successes { get; set; }

    [JsonProperty("pushed")]
    public bool Pushed { get; set; }

    /// <summary>The Condition suffered for pushing, e.g. "exhausted".</summary>
    [JsonProperty("pushCondition", NullValueHandling = NullValueHandling.Ignore)]
    public string? PushCondition { get; set; }

    /// <summary>Why the roll was made, as said at the table, e.g. "Searching the vicarage library".</summary>
    [JsonProperty("reason", NullValueHandling = NullValueHandling.Ignore)]
    public string? Reason { get; set; }

    [JsonIgnore]
    public bool Succeeded => Successes > 0;
}

public sealed class RollPoolPart
{
    [JsonProperty("label")]
    public string Label { get; set; } = "";

    [JsonProperty("dice")]
    public int Dice { get; set; }
}

public sealed class RollRequest
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    /// <summary>A skill id; the attribute it belongs to is added. Leave null and set <see cref="Attribute"/> for an attribute test.</summary>
    [JsonProperty("skill")]
    public string? Skill { get; set; }

    [JsonProperty("attribute")]
    public string? Attribute { get; set; }

    /// <summary>The Gamemaster's difficulty, -3 (very hard) to +3 (very easy).</summary>
    [JsonProperty("difficulty")]
    public int Difficulty { get; set; }

    /// <summary>Bonus dice from gear or talents.</summary>
    [JsonProperty("bonus")]
    public int Bonus { get; set; }

    /// <summary>Uses the character's Advantage: +2 dice.</summary>
    [JsonProperty("useAdvantage")]
    public bool UseAdvantage { get; set; }

    [JsonProperty("reason")]
    public string? Reason { get; set; }
}

/// <summary>Pushes a roll: rethrows every die that isn't a six, at the cost of a Condition.</summary>
public sealed class PushRollRequest
{
    /// <summary>The Condition to suffer; it must be of the roll's type (physical for Physique and Precision, else mental).</summary>
    [JsonProperty("condition")]
    public string Condition { get; set; } = "";
}

/// <summary>A roll with the character's state after it, so the sheet can update without another request.</summary>
public sealed class RollOutcome
{
    [JsonProperty("roll")]
    public Roll Roll { get; set; } = new();

    [JsonProperty("character")]
    public Character Character { get; set; } = new();
}
