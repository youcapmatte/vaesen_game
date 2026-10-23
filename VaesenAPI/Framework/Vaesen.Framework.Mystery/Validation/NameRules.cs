using System.Text.RegularExpressions;

namespace Vaesen.Framework.Mystery.Validation;

/// <summary>Text rules for a game played by voice: names that sound alike, and lies that say the truth.</summary>
public static partial class NameRules
{
    private const string Vowels = "aeiouyåäöéAEIOUYÅÄÖÉ";

    /// <summary>
    /// People named in prose who are not in the cast: a first name followed by a Swedish-looking surname ("Jon Persson",
    /// "Anna Lindqvist"), the form the truth and the events use for the dead and the absent.
    /// </summary>
    public static IEnumerable<string> Mentioned(string? text) =>
        string.IsNullOrEmpty(text) ? [] : MentionedName().Matches(text).Select(m => m.Value);

    /// <summary>Why two names sound alike, or null: a shared first name or first syllable, or one's first name in the other's surname.</summary>
    public static string? Clash(string a, string b)
    {
        var (aFirst, aLast) = Split(a);
        var (bFirst, bLast) = Split(b);
        if (aFirst.Length == 0 || bFirst.Length == 0 || string.Equals(a.Trim(), b.Trim(), StringComparison.OrdinalIgnoreCase))
        {
            return null;
        }

        if (string.Equals(aFirst, bFirst, StringComparison.OrdinalIgnoreCase))
        {
            return "the same first name";
        }

        if (string.Equals(FirstSyllable(aFirst), FirstSyllable(bFirst), StringComparison.OrdinalIgnoreCase))
        {
            return $"both begin '{FirstSyllable(aFirst)}'";
        }

        if (aFirst.Length >= 3 && bLast.Contains(aFirst, StringComparison.OrdinalIgnoreCase))
        {
            return $"'{aFirst}' is in '{bLast}'";
        }

        if (bFirst.Length >= 3 && aLast.Contains(bFirst, StringComparison.OrdinalIgnoreCase))
        {
            return $"'{bFirst}' is in '{aLast}'";
        }

        return null;
    }

    /// <summary>True when a lie's story says what the fact says: most of the fact's words, in the story.</summary>
    public static bool SaysTheSame(string story, string fact)
    {
        var factWords = Words(fact);
        if (factWords.Count < 3)
        {
            return false;
        }

        var storyWords = Words(story);
        return factWords.Count(storyWords.Contains) >= 0.8 * factWords.Count && storyWords.Count <= factWords.Count * 1.5;
    }

    /// <summary>The leading consonants and the first vowels: "Pe" for Per and Persson, "Jo" for Johan and Jonas, "E" for Erik and Elin.</summary>
    private static string FirstSyllable(string name)
    {
        var i = 0;
        while (i < name.Length && !Vowels.Contains(name[i]))
        {
            i++;
        }

        while (i < name.Length && Vowels.Contains(name[i]))
        {
            i++;
        }

        return name[..i];
    }

    private static (string First, string Last) Split(string name)
    {
        var parts = name.Split(' ', StringSplitOptions.RemoveEmptyEntries).Where(p => char.IsUpper(p[0])).ToList();
        return parts.Count switch
        {
            0 => ("", ""),
            1 => (parts[0], ""),
            _ => (parts[0], parts[^1]),
        };
    }

    private static HashSet<string> Words(string text) =>
        WordPattern().Matches(text.ToLowerInvariant()).Select(m => m.Value).Where(w => w.Length > 3).ToHashSet(StringComparer.Ordinal);

    [GeneratedRegex(@"\b[A-ZÅÄÖ][a-zåäöé]+ [A-ZÅÄÖ][a-zåäöé]*(?:sson|son|dotter|ström|strom|berg|lund|qvist|kvist|gren|holm|blad|man|ers|ing|ell)\b")]
    private static partial Regex MentionedName();

    [GeneratedRegex(@"[a-zåäöé]+")]
    private static partial Regex WordPattern();
}
