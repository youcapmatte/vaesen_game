using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A reusable shape of dispute (spec 5.5): an industry intrudes on a domain, a bargain comes due, an unjust death,
/// a curse laid in anger. It names the human roles it creates and what its provocation needs from the level.
/// </summary>
public sealed class ConflictArchetype : ContentRecord
{
    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>Matched against <see cref="VaesenDefinition.CompatibleConflictTags"/>.</summary>
    [JsonProperty("tags")]
    public List<string> Tags { get; set; } = [];

    /// <summary>The reason kinds this shape can carry (<see cref="ReasonKinds"/>).</summary>
    [JsonProperty("reasonKinds")]
    public List<string> ReasonKinds { get; set; } = [];

    /// <summary>The level must offer at least one of these for the provocation to exist ("forest" for logging).</summary>
    [JsonProperty("provocationAffordances")]
    public List<string> ProvocationAffordances { get; set; } = [];

    [JsonProperty("roleSlots")]
    public List<RoleSlot> RoleSlots { get; set; } = [];

    /// <summary>Ideas for the decoy reading players will likely reach first.</summary>
    [JsonProperty("decoyIdeas")]
    public List<string> DecoyIdeas { get; set; } = [];
}

public sealed class RoleSlot
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("required")]
    public bool Required { get; set; } = true;
}
