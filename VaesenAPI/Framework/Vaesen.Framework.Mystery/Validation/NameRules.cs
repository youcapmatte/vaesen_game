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

    /// <summary>Why two names sound alike, or null: a shared first name or first syllable.</summary>
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

        return null;
    }

    /// <summary>
    /// Why one name's first name echoes in the other's surname ("Per" in "Persson"), or null. Milder than <see cref="Clash"/>:
    /// a patronymic is the period's own naming ("Karin Eriksdotter", Erik's daughter), and the two names are still told
    /// apart when called.
    /// </summary>
    public static string? SurnameEcho(string a, string b)
    {
        var (aFirst, aLast) = Split(a);
        var (bFirst, bLast) = Split(b);
        if (aFirst.Length == 0 || bFirst.Length == 0 || string.Equals(a.Trim(), b.Trim(), StringComparison.OrdinalIgnoreCase))
        {
            return null;
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

    /// <summary>
    /// Why a first name is too close to one already used in the Society's world, or null (fixes 3, section 7): the same
    /// name, or the same first three letters ("Henning" and "Henrik", "Britta" and "Brita"). Looser than the rule inside
    /// one case: a world of thirty people shares an opening syllable with nearly every common name.
    /// </summary>
    public static string? FirstNameClash(string a, string b)
    {
        var (aFirst, _) = Split(a);
        var (bFirst, _) = Split(b);
        if (aFirst.Length == 0 || bFirst.Length == 0)
        {
            return null;
        }

        if (string.Equals(aFirst, bFirst, StringComparison.OrdinalIgnoreCase))
        {
            return "the same first name";
        }

        return aFirst.Length >= 3 && bFirst.Length >= 3 && string.Equals(aFirst[..3], bFirst[..3], StringComparison.OrdinalIgnoreCase)
            ? $"both begin '{aFirst[..3]}'"
            : null;
    }

    /// <summary>The last word of a full name, or "" for a single name.</summary>
    public static string LastName(string name) => Split(name).Last;

    /// <summary>"son" or "dotter" when the surname is a patronymic, otherwise null.</summary>
    public static string? Patronymic(string name)
    {
        var last = Split(name).Last;
        return last.Length < 5 ? null
            : last.EndsWith("dotter", StringComparison.OrdinalIgnoreCase) ? "dotter"
            : last.EndsWith("son", StringComparison.OrdinalIgnoreCase) ? "son"
            : null;
    }

    /// <summary>A child's patronymic from the father's first name: Erik → Eriksson, Eriksdotter; Anders → Andersson.</summary>
    public static string PatronymicOf(string fatherFirst, string suffix) =>
        (fatherFirst.EndsWith('s') ? fatherFirst : fatherFirst + "s") + suffix;

    /// <summary>The first name of a full name ("Per" for "Per Ers").</summary>
    public static string FirstName(string name) => Split(name).First;

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
