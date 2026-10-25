using System.Text.RegularExpressions;
using Newtonsoft.Json.Linq;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The one reader of the paths the critic and the checks write (fixes 7, section 3): "cast[id=npc-x].identity.rank",
/// "cast[id=npc-x].knows[factId=f9].when", "creatureMinds[vaesenId=skogsra].knows[factId=f9].when",
/// "creatureMinds[vaesenId=skogsra].thinks[0]", "channels[id=c19].access.requires", "facts[id=f1].statement",
/// "level.sceneDescriptions.mill", "resolutionLevers[id=r1].requiredDeeds[id=r1-d1].performedBy". A selector finds an
/// item in a list by any key it names ([id=…] by any of the id keys), by its text ([trust>=0] in a list of strings),
/// or by its place ([0]). Code operations resolve with it, and routing reads the item a path is about from it.
/// </summary>
public static partial class CasePaths
{
    /// <summary>The keys that name an item: [id=x] finds any of them.</summary>
    public static readonly string[] IdKeys = ["characterId", "id", "threadId", "vaesenId"];

    /// <summary>One step of a path: a property, and the item of its list it selects, if any.</summary>
    public sealed record Segment(string Name, string? Key, string? Value, int? Index);

    /// <summary>The steps of a path; a dot inside a selector does not split it.</summary>
    public static List<Segment>? Parse(string path)
    {
        var segments = new List<Segment>();
        var depth = 0;
        var start = 0;
        var text = path.Trim();
        for (var i = 0; i <= text.Length; i++)
        {
            if (i < text.Length && text[i] == '[')
            {
                depth++;
            }
            else if (i < text.Length && text[i] == ']')
            {
                depth--;
            }
            else if (i == text.Length || (text[i] == '.' && depth == 0))
            {
                if (ParseSegment(text[start..i]) is not { } segment)
                {
                    return null;
                }

                segments.Add(segment);
                start = i + 1;
            }
        }

        return segments.Count == 0 ? null : segments;
    }

    private static Segment? ParseSegment(string part)
    {
        var m = SegmentPattern().Match(part);
        if (!m.Success)
        {
            return null;
        }

        var name = m.Groups["name"].Value;
        if (!m.Groups["sel"].Success)
        {
            return new Segment(name, null, null, null);
        }

        var selector = m.Groups["sel"].Value.Trim();
        if (int.TryParse(selector, out var index))
        {
            return new Segment(name, null, null, index);
        }

        var keyed = KeyedSelector().Match(selector);
        return keyed.Success
            ? new Segment(name, keyed.Groups["key"].Value, keyed.Groups["value"].Value.Trim(), null)
            : new Segment(name, null, selector, null);
    }

    /// <summary>
    /// The token a path names, or null. A list with one item is stepped into when the path goes on by name
    /// ("creatureMinds.knows" for the case's one creature mind).
    /// </summary>
    public static JToken? Resolve(JToken root, string path)
    {
        if (Parse(path) is not { } segments)
        {
            return null;
        }

        JToken? node = root;
        foreach (var segment in segments)
        {
            if (node is JArray { Count: 1 } single)
            {
                node = single[0];
            }

            if (node is not JObject obj)
            {
                return null;
            }

            node = (obj.Property(segment.Name, StringComparison.Ordinal) ?? obj.Property(segment.Name, StringComparison.OrdinalIgnoreCase))?.Value;
            if (segment.Index is { } index)
            {
                node = node is JArray list && index >= 0 && index < list.Count ? list[index] : null;
            }
            else if (segment.Value is { } value)
            {
                node = (node as JArray)?.FirstOrDefault(item => Matches(item, segment.Key, value));
            }

            if (node is null)
            {
                return null;
            }
        }

        return node;
    }

    /// <summary>
    /// The token a path names in a step's output, where a list may go by another name: the cast is "residents" in the
    /// residents step's answer and "minds" in the minds step's.
    /// </summary>
    public static JToken? ResolveInOutput(JToken output, string path)
    {
        if (Resolve(output, path) is { } hit)
        {
            return hit;
        }

        return path.StartsWith("cast[", StringComparison.Ordinal)
            ? Resolve(output, "residents" + path[4..]) ?? Resolve(output, "minds" + path[4..])
            : null;
    }

    /// <summary>Whether a list item is the one a selector names.</summary>
    public static bool Matches(JToken item, string? key, string value)
    {
        if (item is JValue { Type: JTokenType.String } text)
        {
            return key is null && string.Equals(((string)text.Value!).Trim(), value, StringComparison.Ordinal);
        }

        if (item is not JObject obj)
        {
            return false;
        }

        string[] keys = key is null or "id" or "characterId" ? IdKeys : [key];
        return keys.Any(k => obj[k] is JValue { Type: JTokenType.String } v && string.Equals((string)v.Value!, value, StringComparison.Ordinal));
    }

    /// <summary>
    /// The item a target names, in the short form routing reads: "npc-x.knows" for "cast[id=npc-x].knows[factId=f9]",
    /// "c19" for "channels[id=c19].access.requires", "creatureMinds" for any path into a creature's mind or a target on
    /// a creature's own id ("skogsra.thinks"), "scene:mill" for "level.sceneDescriptions.mill", "r1" for a lever's deed.
    /// A target already in short form comes back as it is.
    /// </summary>
    public static string ItemOf(string target, IEnumerable<string>? creatureIds = null)
    {
        var t = target.Trim();
        var creatures = creatureIds?.ToHashSet(StringComparer.Ordinal) ?? [];
        if (Parse(t) is not { Count: > 0 } segments)
        {
            return t;
        }

        var head = segments[0];
        var next = segments.Count > 1 ? segments[1] : null;
        if (head.Value is null && head.Index is null)
        {
            // "skogsra.thinks", "skogsra": the creature, by its own id.
            var id = head.Name.Split(' ')[^1];
            if (creatures.Contains(id))
            {
                return "creatureMinds";
            }

            return head.Name switch
            {
                "level" when next?.Name == "sceneDescriptions" && segments.Count > 2 => "scene:" + segments[2].Name,
                "level" when next?.Name == "placesUsed" => "placesUsed",
                "hook" => "hook",
                "climax" => "climax",
                "creatureMinds" => "creatureMinds",
                "truth" when next is { Name: "escalation", Value: { } e } => e,
                "truth" when next is not null => $"truth.{next.Name}",
                _ => t,
            };
        }

        var value = head.Value ?? "";
        return head.Name switch
        {
            "creatureMinds" => "creatureMinds",
            "cast" or "residents" or "minds" => next is null ? value : $"{value}.{next.Name}",
            _ => value,
        };
    }

    [GeneratedRegex(@"^(?<name>[A-Za-z0-9_\- ]+)(\[(?<sel>[^\]]*)\])?$")]
    private static partial Regex SegmentPattern();

    [GeneratedRegex(@"^(?<key>[A-Za-z]+)=(?<value>.+)$")]
    private static partial Regex KeyedSelector();
}
