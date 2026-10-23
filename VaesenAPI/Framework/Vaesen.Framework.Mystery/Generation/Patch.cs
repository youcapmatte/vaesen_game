using System.Text;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Patch retries (fixes, section 2.2): when a step's only blocking errors are field-level ones on a few items, ask for
/// just those items again and merge them back, instead of regenerating the whole step to fix one word.
/// </summary>
public static class Patch
{
    /// <summary>Errors about one item's own fields. Reachability, redundancy, fair play and the like are structural.</summary>
    private static readonly HashSet<string> FieldLevel =
    [
        IssueCodes.Schema, IssueCodes.UnknownReference, IssueCodes.Vocabulary, IssueCodes.AffordanceFit, IssueCodes.Provenance,
        IssueCodes.CastCoherence, IssueCodes.Decoy,
    ];

    /// <summary>The location of a problem that needs a new item in a list: "new:channel" (a fact no channel teaches yet).</summary>
    public const string NewItem = "new:";

    /// <summary>
    /// True when every error is field-level and sits on an item of one of <paramref name="lists"/> (or asks for a new
    /// item in one), and the items they touch are at most <paramref name="share"/> of all the items.
    /// </summary>
    /// <param name="anyCode">The critic's issues: any code counts as field-level when it names items.</param>
    public static bool Patchable(IReadOnlyList<PatchList> lists, IReadOnlyList<CoherenceIssue> errors, double share, bool anyCode = false)
    {
        if (lists.Count == 0 || errors.Count == 0)
        {
            return false;
        }

        var touched = new HashSet<string>(StringComparer.Ordinal);
        foreach (var error in errors)
        {
            if (anyCode && error.Location.StartsWith(NewItem, StringComparison.Ordinal) && lists.Any(l => l.Prefix == error.Location[NewItem.Length..]))
            {
                continue;
            }

            if ((!anyCode && !FieldLevel.Contains(error.Code)) || Find(lists, error.Location) is null)
            {
                return false;
            }

            touched.Add(error.Location);
        }

        var total = lists.Sum(l => l.Items.Count);
        return touched.Count <= Math.Max(1, Math.Floor(total * share));
    }

    /// <summary>The items the errors are about, by list, as they stand.</summary>
    public static Dictionary<string, List<object>> Items(IReadOnlyList<PatchList> lists, IReadOnlyList<CoherenceIssue> errors)
    {
        var items = new Dictionary<string, List<object>>(StringComparer.Ordinal);
        foreach (var location in errors.Select(e => e.Location).Distinct(StringComparer.Ordinal))
        {
            if (Find(lists, location) is (var list, var item))
            {
                if (!items.TryGetValue(list.Prefix, out var bucket))
                {
                    items[list.Prefix] = bucket = [];
                }

                bucket.Add(item);
            }
        }

        return items;
    }

    /// <summary>
    /// Replaces each item of <paramref name="original"/> that the patch answered, by id, and adds the ones with new ids
    /// (a problem asked for them). Others are left alone.
    /// </summary>
    public static void Merge(IReadOnlyList<PatchList> original, IReadOnlyList<PatchList> patch, bool allowNew = true)
    {
        foreach (var answered in patch)
        {
            var target = original.FirstOrDefault(l => l.Prefix == answered.Prefix);
            if (target is null)
            {
                continue;
            }

            foreach (var item in answered.Items)
            {
                var id = answered.Id(item!);
                var replaced = false;
                for (var i = 0; i < target.Items.Count && !replaced; i++)
                {
                    if (target.Id(target.Items[i]!) == id)
                    {
                        target.Items[i] = item;
                        replaced = true;
                    }
                }

                if (!replaced && allowNew && !string.IsNullOrWhiteSpace(id))
                {
                    target.Items.Add(item);
                }
            }
        }
    }

    /// <summary>"prefix:id" of every item, to see what a fix dropped.</summary>
    public static HashSet<string> AllIds(IReadOnlyList<PatchList> lists) =>
        lists.SelectMany(l => l.Items.OfType<object>().Select(i => $"{l.Prefix}:{l.Id(i)}")).ToHashSet(StringComparer.Ordinal);

    /// <summary>The ids of the items not being patched, so the model knows what else exists without resending it all.</summary>
    public static string OtherIds(IReadOnlyList<PatchList> lists, IReadOnlyList<CoherenceIssue> errors)
    {
        var touched = errors.Select(e => e.Location).ToHashSet(StringComparer.Ordinal);
        return string.Join("; ", lists.Select(l => $"{l.Prefix}: " + string.Join(", ", l.Items.OfType<object>().Select(l.Id).Where(id => !touched.Contains($"{l.Prefix}:{id}")))));
    }

    /// <summary>The patch call's instruction: the items, their problems, and the ids of everything else in the answer.</summary>
    public static string Instruction(string step, Dictionary<string, List<object>> items, IReadOnlyList<CoherenceIssue> errors, string otherIds)
    {
        var text = new StringBuilder();
        text.AppendLine("# Patch");
        text.AppendLine($"Your last answer to the {step} step was nearly right. Only the items below have problems: correct just these.");
        text.AppendLine();
        text.AppendLine("## Items to correct");
        foreach (var (prefix, list) in items)
        {
            text.AppendLine($"{prefix}:");
            text.AppendLine(PromptBuilder.Json(list));
        }

        text.AppendLine();
        text.AppendLine("## Problems");

        // A critic issue split over several items says the same thing for each: say it once, with all its items.
        foreach (var group in errors.GroupBy(e => (e.Code, e.Explanation, e.SuggestedFix)))
        {
            var (code, explanation, fix) = group.Key;
            text.AppendLine($"- {code} at {string.Join(", ", group.Select(e => e.Location).Distinct())}: {explanation}{(fix is null ? "" : $" Fix: {fix}")}");
        }

        var added = errors.Select(e => e.Location).Where(l => l.StartsWith(NewItem, StringComparison.Ordinal)).Select(l => l[NewItem.Length..]).Distinct().ToList();
        text.AppendLine();
        if (added.Count > 0)
        {
            text.AppendLine($"Some problems need something that does not exist yet: add it as a new {string.Join(" or ", added)}, with the next free id after the ones listed below.");
        }

        text.AppendLine("Where a problem says which version to use (a date, a name, a detail), write only that version: do not keep the old one alongside it.");
        text.AppendLine("Answer with the same JSON shape as before, holding only the corrected versions of these items, with the same ids, and any new items the problems ask for, in their lists. Leave every other list empty and every other field at its empty default. Fields with listed allowed values take exactly one of those values.");
        text.AppendLine();
        text.AppendLine("## The other items in your answer, unchanged (ids)");
        text.AppendLine(otherIds);
        return text.ToString();
    }

    private static (PatchList List, object Item)? Find(IReadOnlyList<PatchList> lists, string location)
    {
        var colon = location.IndexOf(':', StringComparison.Ordinal);
        if (colon <= 0)
        {
            return null;
        }

        var prefix = location[..colon];
        var id = location[(colon + 1)..];
        foreach (var list in lists.Where(l => l.Prefix == prefix))
        {
            foreach (var item in list.Items)
            {
                if (item is not null && list.Id(item) == id)
                {
                    return (list, item);
                }
            }
        }

        return null;
    }
}
