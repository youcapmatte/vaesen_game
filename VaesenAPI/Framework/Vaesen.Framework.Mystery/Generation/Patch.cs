using System.Text;
using Newtonsoft.Json;
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

    /// <summary>
    /// True when every error is field-level and sits on an item of one of <paramref name="lists"/>, and the items they
    /// touch are at most <paramref name="share"/> of all the items.
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

    /// <summary>Replaces each item of <paramref name="original"/> that the patch answered, by id. Others are left alone.</summary>
    public static void Merge(IReadOnlyList<PatchList> original, IReadOnlyList<PatchList> patch)
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
                for (var i = 0; i < target.Items.Count; i++)
                {
                    if (target.Id(target.Items[i]!) == id)
                    {
                        target.Items[i] = item;
                        break;
                    }
                }
            }
        }
    }

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
            text.AppendLine(JsonConvert.SerializeObject(list, Formatting.Indented));
        }

        text.AppendLine();
        text.AppendLine("## Problems");
        foreach (var error in errors)
        {
            text.AppendLine($"- {error.Code} at {error.Location}: {error.Explanation}{(error.SuggestedFix is null ? "" : $" Fix: {error.SuggestedFix}")}");
        }

        text.AppendLine();
        text.AppendLine("Answer with the same JSON shape as before, holding only the corrected versions of these items, with the same ids, in their lists. Leave every other list empty and every other field at its empty default. Fields with listed allowed values take exactly one of those values.");
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
