using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A kind of place (library, inn, farm, mine), authored once (spec 5.3). It declares its interior affordances,
/// each with a condition on the settlement it is instantiated in.
/// </summary>
public sealed class PlaceTemplate : ContentRecord
{
    /// <summary>The place-kind tag this template stands for ("inn"); must be in the vocabulary.</summary>
    [JsonProperty("tag")]
    public string Tag { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>Settlement types it can appear in (<see cref="SettlementTypes"/>). Empty: any.</summary>
    [JsonProperty("settlementTypes")]
    public List<string> SettlementTypes { get; set; } = [];

    /// <summary>Extra tags a place of this kind always offers ("water" for a mill).</summary>
    [JsonProperty("tags")]
    public List<string> Tags { get; set; } = [];

    [JsonProperty("affordances")]
    public List<AffordanceSlot> Affordances { get; set; } = [];
}

/// <summary>An interior affordance a template may have, possibly holding more (a desk with a lockable drawer).</summary>
public sealed class AffordanceSlot
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("tag")]
    public string Tag { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("condition")]
    public AffordanceCondition Condition { get; set; } = new();

    /// <summary>How it changes with the settlement ("a single shelf in a hamlet, a reading room in a city").</summary>
    [JsonProperty("scaling")]
    public string? Scaling { get; set; }

    [JsonProperty("contains")]
    public List<AffordanceSlot> Contains { get; set; } = [];
}

/// <summary>When a slot appears: thresholds on settlement size and wealth (1-5), and how many.</summary>
public sealed class AffordanceCondition
{
    [JsonProperty("minSize")]
    public int MinSize { get; set; } = 1;

    [JsonProperty("minWealth")]
    public int MinWealth { get; set; } = 1;

    [JsonProperty("minCount")]
    public int MinCount { get; set; } = 1;

    [JsonProperty("maxCount")]
    public int MaxCount { get; set; } = 1;

    public bool IsMet(int size, int wealth) => size >= MinSize && wealth >= MinWealth;
}

public static class SettlementTypes
{
    public const string Hamlet = "hamlet";
    public const string Village = "village";
    public const string Town = "town";
    public const string City = "city";
    public const string Estate = "estate";
    public const string Camp = "camp";

    public static readonly IReadOnlyList<string> All = [Hamlet, Village, Town, City, Estate, Camp];
}

/// <summary>
/// An assembled text level (spec 5.3): a settlement and its surroundings, made of places instantiated from templates,
/// tagged with its month. The same structure will later drive the 3D levels.
/// </summary>
public sealed class LevelDefinition : ContentRecord
{
    [JsonProperty("region")]
    public string Region { get; set; } = "";

    [JsonProperty("settlementType")]
    public string SettlementType { get; set; } = SettlementTypes.Village;

    /// <summary>1 (a few houses) to 5 (a city).</summary>
    [JsonProperty("size")]
    public int Size { get; set; } = 2;

    /// <summary>1 (poor) to 5 (rich).</summary>
    [JsonProperty("wealth")]
    public int Wealth { get; set; } = 2;

    /// <summary>The month of the year (1-12) the level is written for; seasonality is baked in.</summary>
    [JsonProperty("month")]
    public int Month { get; set; } = 10;

    /// <summary>The landscape around the settlement ("forest", "lake", "marsh"): terrain tags.</summary>
    [JsonProperty("terrain")]
    public List<string> Terrain { get; set; } = [];

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>Weather, light and work of the month here.</summary>
    [JsonProperty("seasonNotes")]
    public string SeasonNotes { get; set; } = "";

    /// <summary>How the Society gets there from Uppsala, and how long it takes.</summary>
    [JsonProperty("travel")]
    public string Travel { get; set; } = "";

    /// <summary>Things everyone here knows; NPCs and narration may use them freely.</summary>
    [JsonProperty("publicFacts")]
    public List<string> PublicFacts { get; set; } = [];

    [JsonProperty("places")]
    public List<LevelPlace> Places { get; set; } = [];

    public LevelPlace? FindPlace(string? id) => Places.Find(p => p.Id == id);

    /// <summary>A copy of this level as a case uses it: with the homes the case added for its people.</summary>
    public LevelDefinition ForCase(CaseLevel caseLevel)
    {
        var copy = JsonConvert.DeserializeObject<LevelDefinition>(JsonConvert.SerializeObject(this))!;
        foreach (var added in caseLevel.AddedAffordances)
        {
            if (copy.FindPlace(added.PlaceId) is { } place && place.FindAffordance(added.Affordance.Id) is null)
            {
                place.Affordances.Add(added.Affordance);
            }
        }

        return copy;
    }

    /// <summary>Every tag the level offers: terrain, places, their tags and all affordances inside them.</summary>
    public IEnumerable<string> AllTags()
    {
        foreach (var tag in Terrain)
        {
            yield return tag;
        }

        foreach (var place in Places)
        {
            foreach (var tag in place.AllTags())
            {
                yield return tag;
            }
        }
    }
}

public sealed class LevelPlace
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("templateId")]
    public string TemplateId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>A short sensory description, read out on arrival.</summary>
    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>The place-kind tag plus extra tags ("inn", "water").</summary>
    [JsonProperty("tags")]
    public List<string> Tags { get; set; } = [];

    [JsonProperty("affordances")]
    public List<LevelAffordance> Affordances { get; set; } = [];

    /// <summary>An affordance anywhere inside this place, at any depth.</summary>
    public LevelAffordance? FindAffordance(string? id) => AllAffordances().FirstOrDefault(a => a.Id == id);

    public IEnumerable<LevelAffordance> AllAffordances()
    {
        var pending = new Stack<LevelAffordance>(Affordances);
        while (pending.Count > 0)
        {
            var current = pending.Pop();
            yield return current;
            foreach (var inner in current.Contains)
            {
                pending.Push(inner);
            }
        }
    }

    public IEnumerable<string> AllTags() => Tags.Concat(AllAffordances().Select(a => a.Tag));
}

/// <summary>Affordance tags people live in: a trace there belongs to whoever lives there (fixes 3, section 6).</summary>
public static class Dwellings
{
    public const string Dwelling = "dwelling";
    public const string WorkersCottages = "workers-cottages";

    public static readonly IReadOnlySet<string> Tags = new HashSet<string>(StringComparer.Ordinal)
    {
        Dwelling, WorkersCottages, "bunkhouse", "bedroom", "kitchen", "guest-room",
    };

    /// <summary>A minimal home for one person in a place that has none to give them: a room with a bed, a hearth and a chest.</summary>
    public static LevelAffordance For(string placeId, string characterId, string name)
    {
        var id = $"{placeId}-home-{characterId.Replace("npc-", "", StringComparison.Ordinal)}";
        return new LevelAffordance
        {
            Id = id,
            Tag = Dwelling,
            Name = $"{name}'s home",
            Description = $"Where {name} lives.",
            Contains =
            [
                new LevelAffordance { Id = $"{id}-bed", Tag = "bed", Name = "Bed", Description = $"{name}'s bed." },
                new LevelAffordance { Id = $"{id}-hearth", Tag = "hearth", Name = "Hearth", Description = "A small hearth." },
                new LevelAffordance { Id = $"{id}-chest", Tag = "chest", Name = "Chest", Description = $"{name}'s chest.", Lockable = true },
            ],
        };
    }
}

public sealed class LevelAffordance
{
    /// <summary>Unique within the level ("inn-taproom", "inn-office-desk-drawer").</summary>
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("tag")]
    public string Tag { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    [JsonProperty("lockable")]
    public bool Lockable { get; set; }

    [JsonProperty("contains")]
    public List<LevelAffordance> Contains { get; set; } = [];
}
