using System.Globalization;
using System.Text;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Ids are not names (review 3, item 5): a person's id comes from their place and role ("npc-lyckan-crofter"), so it
/// still fits when the name check renames them. Emil renamed Rasmus kept npc-emil-pettersson, and every later case
/// and reviewer read the wrong man into it.
/// </summary>
public static class PersonIds
{
    private static readonly HashSet<string> Clause = new(StringComparer.Ordinal) { "who", "whose", "that", "which", "now", "but" };

    private static readonly HashSet<string> Filler = new(StringComparer.Ordinal) { "of", "the", "a", "an", "at", "and", "in", "on", "to", "for", "from", "with" };

    /// <summary>"npc-", the place's last word and the role's first two words, unique among <paramref name="taken"/> (which it joins).</summary>
    public static string ForRole(string? role, string? placeId, ISet<string> taken)
    {
        // "widow who takes in washing" is a widow, "bailiff's wife" a bailiff-wife: the role stops at a clause.
        var words = Slug(role ?? "").Split('-', StringSplitOptions.RemoveEmptyEntries)
            .TakeWhile(w => !Clause.Contains(w))
            .Where(w => !Filler.Contains(w) && w.Length > 1)
            .Take(2)
            .ToList();
        var place = (placeId ?? "").Split('-', StringSplitOptions.RemoveEmptyEntries).LastOrDefault() is { } last ? Slug(last) : "";
        var stem = string.Join('-', new[] { "npc", place }.Concat(words.Count > 0 ? words : ["person"]).Where(w => w.Length > 0));
        var id = stem;
        for (var n = 2; taken.Contains(id); n++)
        {
            id = $"{stem}-{n}";
        }

        taken.Add(id);
        return id;
    }

    /// <summary>Whether an id is built from the person's first name or surname ("npc-emil-pettersson" for Emil Pettersson).</summary>
    public static bool NamesSomeone(string id, string name)
    {
        // A double name ("Nils-Per") is several parts of the id.
        var padded = $"-{id}-";
        return new[] { NameRules.FirstName(name), NameRules.LastName(name) }.Select(Slug).Any(n => n.Length >= 3 && padded.Contains($"-{n}-", StringComparison.Ordinal));
    }

    /// <summary>
    /// Gives the case's new people whose ids are made from their names an id from their role and home, in the drafts and
    /// their ties to each other, before anything else refers to them. Ids already in <paramref name="packet"/> stay.
    /// Returns old id to new.
    /// </summary>
    public static Dictionary<string, string> Rekey(List<CastDraft> people, CasePacket packet, IEnumerable<string> worldIds)
    {
        var existing = packet.Cast.Select(c => c.CharacterId).ToHashSet(StringComparer.Ordinal);
        var taken = existing.Concat(worldIds).Concat(people.Select(p => p.CharacterId)).ToHashSet(StringComparer.Ordinal);
        var map = new Dictionary<string, string>(StringComparer.Ordinal);
        foreach (var person in people.Where(p => p.InstanceRef == InstanceRefs.New && !existing.Contains(p.CharacterId) && NamesSomeone(p.CharacterId, p.Name)))
        {
            map[person.CharacterId] = ForRole(person.Identity?.Occupation is { Length: > 0 } occupation ? occupation : person.PlotRole, person.HomePlaceId, taken);
        }

        foreach (var person in people)
        {
            person.CharacterId = map.GetValueOrDefault(person.CharacterId, person.CharacterId);
            foreach (var tie in person.Relationships)
            {
                tie.CharacterId = map.GetValueOrDefault(tie.CharacterId, tie.CharacterId);
            }
        }

        return map;
    }

    /// <summary>Lower-case ASCII words joined by hyphens: "Kärret Statare" is "karret-statare".</summary>
    public static string Slug(string text)
    {
        var folded = new StringBuilder();
        text = text.Replace("'s ", " ", StringComparison.Ordinal).Replace("’s ", " ", StringComparison.Ordinal);
        foreach (var c in text.ToLowerInvariant().Normalize(NormalizationForm.FormD))
        {
            if (CharUnicodeInfo.GetUnicodeCategory(c) == UnicodeCategory.NonSpacingMark)
            {
                continue;
            }

            folded.Append(c is >= 'a' and <= 'z' or >= '0' and <= '9' ? c : '-');
        }

        return string.Join('-', folded.ToString().Split('-', StringSplitOptions.RemoveEmptyEntries));
    }
}
