using System.Text.RegularExpressions;

namespace Vaesen.Framework.Mystery.Validation;

/// <summary>
/// The distinctive words of a piece of prose, roughly stemmed, for comparing a scene with a gated reveal (review 3,
/// item 4): "wedged" and "wedge", "hands" and "hand" match; "the", "with" and "something" do not count.
/// </summary>
public static partial class SceneWords
{
    /// <summary>A scene leaks a reveal when it shares at least this many of its distinctive words...</summary>
    public const int LeakWords = 3;

    /// <summary>...and at least one in this many of them.</summary>
    public const int LeakShare = 3;

    private static readonly HashSet<string> Stop = new(StringComparer.Ordinal)
    {
        "about", "above", "after", "again", "against", "along", "also", "among", "another", "around", "away", "back", "because", "been", "before",
        "behind", "being", "below", "beside", "between", "both", "but", "came", "come", "comes", "could", "does", "down", "during", "each", "even",
        "ever", "every", "from", "have", "having", "here", "into", "just", "keep", "keeps", "kept", "know", "like", "little", "look", "looks", "made",
        "make", "many", "more", "most", "much", "must", "near", "never", "next", "none", "nothing", "once", "only", "other", "over", "same", "seem",
        "seems", "shall", "should", "since", "some", "someone", "something", "still", "such", "than", "that", "their", "them", "then", "there",
        "these", "they", "thing", "things", "this", "those", "though", "through", "till", "under", "until", "upon", "very", "want", "were", "what",
        "when", "where", "which", "while", "whom", "whose", "with", "within", "without", "would", "your", "yours", "will", "said", "says", "tell",
        "tells", "told", "sees", "seen", "find", "finds", "found", "shows", "show", "anyone", "everyone", "players", "group", "time", "times",
        "the", "and", "for", "not", "one", "two", "his", "her", "him", "she", "its", "out", "off", "any", "all", "who", "how", "why", "has", "had",
        "was", "are", "can", "did", "too", "yet", "old", "new",
    };

    /// <summary>The distinctive stems of <paramref name="text"/>.</summary>
    public static HashSet<string> Of(string? text) =>
        string.IsNullOrEmpty(text)
            ? []
            : Word().Matches(text.ToLowerInvariant())
                .Select(m => m.Value.EndsWith("'s", StringComparison.Ordinal) ? m.Value[..^2] : m.Value)
                .Where(w => w.Length >= 4 && !Stop.Contains(w))
                .Select(Stem)
                .ToHashSet(StringComparer.Ordinal);

    private static string Stem(string word)
    {
        foreach (var ending in new[] { "ing", "ed", "es", "s" })
        {
            if (word.Length - ending.Length >= 4 && word.EndsWith(ending, StringComparison.Ordinal))
            {
                word = word[..^ending.Length];
                break;
            }
        }

        // "wedg(e)" and "wedg(ed)": the stem's last letter is dropped when it is an e.
        return word.Length > 4 && word.EndsWith('e') ? word[..^1] : word;
    }

    [GeneratedRegex(@"[\p{L}']+")]
    private static partial Regex Word();
}
