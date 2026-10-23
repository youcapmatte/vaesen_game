using System.Collections;
using System.Reflection;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Repairs near misses in fixed-value fields before validation, in code, instead of spending a model call on one word:
/// stray spaces and capitals, and a small table of obvious synonyms per list ("surface" for crude). Every repair is
/// reported as a warning with the original and the new value, so the synonym table can be reviewed.
/// </summary>
public static class EnumRepair
{
    /// <summary>Synonyms per value list. Fact kinds need judgement, so they have none (a patch call asks instead).</summary>
    private static readonly Dictionary<Type, Dictionary<string, string>> Synonyms = new()
    {
        [typeof(SolveDepths)] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["surface"] = SolveDepths.Crude,
            ["shallow"] = SolveDepths.Crude,
            ["full"] = SolveDepths.Deep,
            ["answersGrievance"] = SolveDepths.Deep,
            ["root"] = SolveDepths.Deep,
            ["half"] = SolveDepths.Partial,
        },
        [typeof(CastImportance)] = new(StringComparer.OrdinalIgnoreCase)
        {
            ["core"] = CastImportance.Significant,
            ["canonical"] = CastImportance.Significant,
            ["background"] = CastImportance.Resident,
        },
    };

    /// <summary>Repairs every [OneOf] field reachable from <paramref name="output"/>; returns one warning per repair.</summary>
    public static List<CoherenceIssue> Normalize(object output)
    {
        var issues = new List<CoherenceIssue>();
        Walk(output, "", issues, new HashSet<object>(ReferenceEqualityComparer.Instance));
        return issues;
    }

    /// <summary>The allowed value <paramref name="value"/> plainly means, or null.</summary>
    public static string? Repair(string? value, OneOfAttribute oneOf)
    {
        var trimmed = value?.Trim() ?? "";
        var exact = oneOf.Values.FirstOrDefault(v => string.Equals(v, trimmed, StringComparison.OrdinalIgnoreCase));
        if (exact is not null)
        {
            return exact;
        }

        return Synonyms.TryGetValue(oneOf.ValuesType, out var table) && table.TryGetValue(trimmed, out var mapped) ? mapped : null;
    }

    private static void Walk(object? node, string path, List<CoherenceIssue> issues, HashSet<object> seen)
    {
        if (node is null or string || node.GetType().IsValueType || !seen.Add(node))
        {
            return;
        }

        if (node is IEnumerable items)
        {
            var i = 0;
            foreach (var item in items)
            {
                Walk(item, $"{path}[{ItemLabel(item, i)}]", issues, seen);
                i++;
            }

            return;
        }

        if (node.GetType().Namespace?.StartsWith("Vaesen.Framework.Mystery", StringComparison.Ordinal) != true)
        {
            return;
        }

        foreach (var property in node.GetType().GetProperties(BindingFlags.Public | BindingFlags.Instance))
        {
            if (!property.CanRead || property.GetIndexParameters().Length > 0)
            {
                continue;
            }

            var at = path.Length == 0 ? property.Name : $"{path}.{property.Name}";
            var oneOf = property.GetCustomAttribute<OneOfAttribute>();
            if (oneOf is not null && property.PropertyType == typeof(string) && property.CanWrite)
            {
                var value = (string?)property.GetValue(node);
                if (value is null || oneOf.Values.Contains(value))
                {
                    continue;
                }

                var repaired = Repair(value, oneOf);
                if (repaired is not null)
                {
                    property.SetValue(node, repaired);
                    issues.Add(CoherenceIssue.Warning(IssueCodes.Repaired, at, $"'{value}' was read as '{repaired}'."));
                }

                continue;
            }

            Walk(property.GetValue(node), at, issues, seen);
        }
    }

    /// <summary>An item's id when it has one ("r3"), so a warning says which lever it was; otherwise its index.</summary>
    private static string ItemLabel(object? item, int index) =>
        item?.GetType().GetProperty("Id")?.GetValue(item) as string
        ?? item?.GetType().GetProperty("CharacterId")?.GetValue(item) as string
        ?? item?.GetType().GetProperty("ThreadId")?.GetValue(item) as string
        ?? index.ToString(System.Globalization.CultureInfo.InvariantCulture);
}
