using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// The one shared vocabulary of affordance tags (Content/{setting}/vocabulary.json). Place templates declare these
/// tags, Vaesen require them, conflicts need them for their provocation, and clues are hidden in them. Every tag used
/// anywhere is checked against this list.
/// </summary>
public sealed class AffordanceVocabulary
{
    [JsonProperty("version")]
    public int Version { get; set; } = 1;

    [JsonProperty("tags")]
    public List<AffordanceTag> Tags { get; set; } = [];

    public AffordanceTag? Find(string? id) => Tags.Find(t => t.Id == id);

    public bool Contains(string? id) => Find(id) is not null;

    /// <summary>The tag and everything it implies, transitively ("millpond" gives "millpond", "pond", "water").</summary>
    public HashSet<string> Expand(string tag)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        var pending = new Stack<string>();
        pending.Push(tag);
        while (pending.Count > 0)
        {
            var current = pending.Pop();
            if (!result.Add(current))
            {
                continue;
            }

            foreach (var implied in Find(current)?.Implies ?? [])
            {
                pending.Push(implied);
            }
        }

        return result;
    }

    /// <summary>All tags of <paramref name="tags"/> plus everything they imply.</summary>
    public HashSet<string> ExpandAll(IEnumerable<string> tags)
    {
        var result = new HashSet<string>(StringComparer.Ordinal);
        foreach (var tag in tags)
        {
            result.UnionWith(Expand(tag));
        }

        return result;
    }
}

public sealed class AffordanceTag
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>One of <see cref="AffordanceKinds"/>.</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = AffordanceKinds.Feature;

    [JsonProperty("description")]
    public string Description { get; set; } = "";

    /// <summary>Broader tags this one counts as: a "millpond" is also "water".</summary>
    [JsonProperty("implies")]
    public List<string> Implies { get; set; } = [];
}

public static class AffordanceKinds
{
    /// <summary>Landscape around a settlement: forest, lake, marsh, mountain.</summary>
    public const string Terrain = "terrain";

    /// <summary>A kind of place: inn, church, farm, mine.</summary>
    public const string Place = "place";

    /// <summary>Something inside a place: desk, cellar, hayloft, archive.</summary>
    public const string Interior = "interior";

    /// <summary>A container that can hold a clue: drawer, chest, strongbox.</summary>
    public const string Container = "container";

    /// <summary>Anything else a level can offer: a waterwheel, a bridge, a graveyard, a railway line.</summary>
    public const string Feature = "feature";

    public static readonly IReadOnlyList<string> All = [Terrain, Place, Interior, Container, Feature];
}
