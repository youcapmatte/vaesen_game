using System.Text.RegularExpressions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>What one code operation did: a replacement pair or a field set, with its post-condition.</summary>
public sealed class CodeFixResult
{
    /// <summary>"replace" or "setField".</summary>
    public string Op { get; init; } = "";

    /// <summary>The pair ("from" → "to") or the path and value, for the log.</summary>
    public string What { get; init; } = "";

    /// <summary>Matches inside the issue's targets (a replacement), or 1 for a field that was set.</summary>
    public int InScope { get; set; }

    /// <summary>Matches elsewhere in the packet that were replaced too (a name, a date, three words or more).</summary>
    public List<string> OutOfScopeReplaced { get; } = [];

    /// <summary>Matches elsewhere that were left, as too short to replace blind: an advisory for a person.</summary>
    public List<string> OutOfScopeLeft { get; } = [];

    /// <summary>Null when the operation changed the data and its post-condition holds; otherwise why not.</summary>
    public string? Failed { get; set; }

    public bool Ok => Failed is null;

    public override string ToString() =>
        $"{Op} {What}: {(Ok ? "ok" : "FAILED: " + Failed)}; {InScope} in scope"
        + (OutOfScopeReplaced.Count > 0 ? $", also replaced at {string.Join(", ", OutOfScopeReplaced)}" : "")
        + (OutOfScopeLeft.Count > 0 ? $", left at {string.Join(", ", OutOfScopeLeft)}" : "");
}

/// <summary>
/// Mechanical fixes done by code, not a model (review 2, item 2; fixes 5, sections 1 and 2). A replacement is exact
/// prose: it is searched for in the whole packet, replaced inside the issue's targets and, when it is a name, a date or
/// three words or more, everywhere else it occurs. A field value (a rank, a place, a source) is never replaced as text:
/// it is set as a field by path ("cast[id=npc-x].identity.rank"). Each pair and each field reports its own result, and
/// a pair that matched nothing, or left a copy where it should have replaced, has failed. The packet and every step's
/// kept output are changed alike, so a later patch does not bring the old text back.
/// </summary>
public static partial class TextReplacements
{
    /// <summary>The keys that name an item in the packet and the outputs, to find a target's object.</summary>
    private static readonly string[] IdKeys = ["characterId", "id", "threadId", "vaesenId"];

    /// <summary>Applies the issue's replacements, one pair at a time; one result per pair.</summary>
    public static List<CodeFixResult> Apply(MysteryDraft draft, CoherenceIssue issue)
    {
        var results = new List<CodeFixResult>();
        var ids = Ids(draft.Packet);
        foreach (var pair in issue.Replacements ?? [])
        {
            var result = new CodeFixResult { Op = "replace", What = $"\"{pair.From}\" → \"{pair.To}\"" };
            results.Add(result);
            if (Refusal(pair.From, ids) is { } why)
            {
                result.Failed = why;
                continue;
            }

            ApplyPair(draft, issue, pair, result);
        }

        return results;
    }

    /// <summary>Why a replacement may not run, or null: an id, a fragment of JSON, or nothing at all.</summary>
    public static string? Refusal(string from, HashSet<string>? ids = null)
    {
        if (string.IsNullOrWhiteSpace(from))
        {
            return "Refused: nothing to replace.";
        }

        if (from.Contains("\":", StringComparison.Ordinal) || from.Contains('{', StringComparison.Ordinal))
        {
            // A pattern on serialised JSON cannot match a field value; it is a field to set (fixes 5, section 1).
            return "Refused: a replacement on JSON; a field value is set with setField.";
        }

        // An id is never replaced (review 3): ids are fixed, however they read.
        return ids?.Contains(from.Trim()) == true ? "Refused: ids are fixed." : null;
    }

    private static void ApplyPair(MysteryDraft draft, CoherenceIssue issue, TextReplacement pair, CodeFixResult result)
    {
        var scope = Scope(issue);
        var widely = issue.Everywhere || Wide(pair.From);
        var packet = PacketTree(draft.Packet);
        var inScope = scope is null ? null : InScopeValues(packet, scope);
        foreach (var value in Strings(packet))
        {
            var count = Count(value, pair.From);
            if (count == 0)
            {
                continue;
            }

            if (inScope is null || inScope.Contains(value))
            {
                result.InScope += count;
            }
            else if (widely)
            {
                result.OutOfScopeReplaced.Add(value.Path);
            }
            else
            {
                result.OutOfScopeLeft.Add(value.Path);
            }
        }

        if (result.InScope + result.OutOfScopeReplaced.Count == 0)
        {
            result.Failed = result.OutOfScopeLeft.Count > 0
                ? $"Matched nothing in its targets; found only elsewhere ({string.Join(", ", result.OutOfScopeLeft)}), too short to replace blind."
                : "Matched nothing in the case.";
            return;
        }

        var replaceScope = widely ? null : scope;
        string Rewrite(string json)
        {
            var root = JToken.Parse(json);
            var nodes = replaceScope is null ? null : InScopeValues(root, replaceScope);
            foreach (var value in Strings(root).Where(v => nodes is null || nodes.Contains(v)))
            {
                var dummy = 0;
                value.Value = Replace((string)value.Value!, pair.From, pair.To, ref dummy);
            }

            return root.ToString(Formatting.None);
        }

        // The case's own issue list keeps its words: it records what was asked, not the case.
        var validation = draft.Packet.Validation;
        draft.Packet = JsonConvert.DeserializeObject<CasePacket>(Rewrite(JsonConvert.SerializeObject(draft.Packet)))!;
        draft.Packet.Validation = validation;
        foreach (var (step, output) in draft.Outputs.ToList())
        {
            draft.Outputs[step] = JsonConvert.DeserializeObject(Rewrite(JsonConvert.SerializeObject(output)), output.GetType())!;
        }

        // The post-condition: no copy is left where it was to be replaced.
        var after = PacketTree(draft.Packet);
        var afterScope = replaceScope is null ? null : InScopeValues(after, replaceScope);
        var left = Strings(after).Where(v => (afterScope is null || afterScope.Contains(v)) && Count(v, pair.From) > 0).Select(v => v.Path).ToList();
        if (left.Count > 0)
        {
            result.Failed = $"Copies are still there: {string.Join(", ", left)}.";
        }
    }

    /// <summary>A phrase worth replacing wherever it occurs: three words or more, or one with a name or number in it ("late May").</summary>
    private static bool Wide(string from)
    {
        var words = from.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        return words.Length >= 3 || words.Any(w => char.IsUpper(w[0])) || from.Any(char.IsDigit);
    }

    /// <summary>
    /// Sets a field by path (fixes 5, section 1): "cast[id=npc-x].identity.rank", "channels[id=c66].placeId". Items in a
    /// list are found by their id, never their index. A value from a fixed list is checked first; the field is read back.
    /// </summary>
    public static CodeFixResult SetField(MysteryDraft draft, FieldSet set)
    {
        var result = new CodeFixResult { Op = "setField", What = $"{set.Path} = \"{set.Value}\"" };
        if (Allowed(draft, set.Path) is { } allowed && !allowed.Contains(set.Value, StringComparer.Ordinal))
        {
            result.Failed = $"'{set.Value}' is not allowed there.{CoherenceIssue.Allowed(allowed)}";
            return result;
        }

        var validation = draft.Packet.Validation;
        var root = PacketTree(draft.Packet);
        if (Resolve(root, set.Path) is not { } field)
        {
            result.Failed = $"No field at {set.Path}.";
            return result;
        }

        field.Value = Typed(field.Value, set.Value);
        draft.Packet = root.ToObject<CasePacket>()!;
        draft.Packet.Validation = validation;

        // The same field in the step outputs that hold it, so a later patch of that step keeps it.
        foreach (var (step, output) in draft.Outputs.ToList())
        {
            var tree = JToken.FromObject(output);
            var hit = Resolve(tree, set.Path) ?? (set.Path.StartsWith("cast[", StringComparison.Ordinal) ? Resolve(tree, "residents" + set.Path[4..]) : null);
            if (hit is not null)
            {
                hit.Value = Typed(hit.Value, set.Value);
                draft.Outputs[step] = tree.ToObject(output.GetType())!;
            }
        }

        // The post-condition: the field reads back as the new value.
        var back = Resolve(PacketTree(draft.Packet), set.Path);
        result.InScope = 1;
        if (back?.Value is not JValue v || !string.Equals(v.ToString(CultureInfoInvariant), set.Value, StringComparison.Ordinal) && v.Value?.ToString() != set.Value)
        {
            result.Failed = $"{set.Path} does not read back as \"{set.Value}\".";
        }

        return result;
    }

    private static readonly IFormatProvider CultureInfoInvariant = System.Globalization.CultureInfo.InvariantCulture;

    /// <summary>The values a field may take, when it is one from a fixed list.</summary>
    private static IReadOnlyList<string>? Allowed(MysteryDraft draft, string path)
    {
        var field = path[(path.LastIndexOf('.') + 1)..];
        var list = path[..Math.Max(0, path.IndexOf('['))];
        return (list, field) switch
        {
            (_, "rank") => draft.Pack.Setting.PeriodConventions.SocialRanks is { Count: > 0 } ranks ? ranks : null,
            ("cast", "importance") => CastImportance.All,
            ("cast", "instanceRef") => InstanceRefs.All,
            ("channels", "kind") => ChannelKinds.All,
            ("facts", "kind") => FactKinds.All,
            (_, "register") => SignRegisters.All,
            (_, "desperation") => DesperationTiers.All,
            (_, "placeId" or "homePlaceId" or "keeps") => draft.Level.Places.Select(p => p.Id).ToList(),
            (_, "affordanceId" or "homeAffordanceId") => draft.Level.Places.SelectMany(p => p.AllAffordances()).Select(a => a.Id).Concat(draft.Packet.Level.AddedAffordances.Select(a => a.Affordance.Id)).ToList(),
            _ => null,
        };
    }

    /// <summary>The field a path names, or null: "cast[id=npc-x].identity.rank".</summary>
    private static JProperty? Resolve(JToken root, string path)
    {
        JToken? node = root;
        JProperty? last = null;
        foreach (var part in path.Split('.'))
        {
            var m = PathPart().Match(part);
            if (!m.Success || node is not JObject obj)
            {
                return null;
            }

            last = obj.Property(m.Groups["name"].Value, StringComparison.Ordinal);
            node = last?.Value;
            if (m.Groups["id"].Success)
            {
                // [id=x] finds an item by any of its id keys; [factId=f2] a knowledge entry by its fact.
                var id = m.Groups["id"].Value;
                string[] keys = m.Groups["key"].Value == "factId" ? ["factId"] : IdKeys;
                node = (node as JArray)?.OfType<JObject>().FirstOrDefault(o => keys.Any(k => o[k] is JValue { Type: JTokenType.String } v && (string)v.Value! == id));
                last = null;
            }

            if (node is null)
            {
                return null;
            }
        }

        return last;
    }

    [GeneratedRegex(@"^(?<name>[A-Za-z]+)(\[(?<key>id|characterId|factId)=(?<id>[^\]]+)\])?$")]
    private static partial Regex PathPart();

    /// <summary>The new value in the old one's type: a bool stays a bool, a number a number.</summary>
    private static JToken Typed(JToken old, string value) => old.Type switch
    {
        JTokenType.Boolean when bool.TryParse(value, out var b) => new JValue(b),
        JTokenType.Integer when long.TryParse(value, out var i) => new JValue(i),
        JTokenType.Float when double.TryParse(value, System.Globalization.NumberStyles.Float, CultureInfoInvariant, out var d) => new JValue(d),
        _ => new JValue(value),
    };

    /// <summary>Every id in the case: people, offstage people, facts, channels, events, beats, levers and decoy threads.</summary>
    public static HashSet<string> Ids(CasePacket packet) =>
        packet.Cast.Select(c => c.CharacterId)
            .Concat(packet.OffstagePeople.Select(o => o.Id))
            .Concat(packet.Facts.Select(f => f.Id))
            .Concat(packet.Channels.Select(c => c.Id))
            .Concat(packet.Truth.Escalation.Select(e => e.Id))
            .Concat(packet.Beats.Select(b => b.Id))
            .Concat(packet.ResolutionLevers.Select(l => l.Id))
            .Concat(packet.Decoys.Select(d => d.ThreadId))
            .ToHashSet(StringComparer.Ordinal);

    /// <summary>Whether any replaced text is still there, anywhere outside the validation: the check that the fix held.</summary>
    public static bool Holds(MysteryDraft draft, CoherenceIssue issue)
    {
        var froms = (issue.Replacements ?? []).Where(r => Refusal(r.From) is null).Select(r => r.From)
            .SelectMany(f => f.Length > 0 && char.IsLower(f[0]) ? new[] { f, char.ToUpperInvariant(f[0]) + f[1..] } : [f])
            .ToList();
        var root = PacketTree(draft.Packet);
        var scope = issue.Everywhere ? null : Scope(issue);
        var values = scope is null ? Strings(root) : Strings(root).Where(InScopeValues(root, scope).Contains);
        return !values.Any(v => froms.Exists(f => Pattern(f).IsMatch((string)v.Value!)));
    }

    /// <summary>The packet as a tree, without its validation (which keeps the critic's words).</summary>
    private static JToken PacketTree(CasePacket packet)
    {
        var root = JToken.FromObject(packet);
        ((JObject)root).Remove("validation");
        return root;
    }

    private static IEnumerable<JValue> Strings(JToken root) => All(root).OfType<JValue>().Where(v => v.Type == JTokenType.String);

    private static HashSet<JValue> InScopeValues(JToken root, HashSet<string> scope) =>
        // By reference: two equal strings in different items are different places.
        Nodes(root, scope).SelectMany(n => Strings(n)).ToHashSet<JValue>(ReferenceEqualityComparer.Instance);

    private static int Count(JValue value, string from)
    {
        var text = (string)value.Value!;
        var n = Pattern(from).Matches(text).Count;
        return from.Length > 0 && char.IsLower(from[0]) ? n + Pattern(char.ToUpperInvariant(from[0]) + from[1..]).Matches(text).Count : n;
    }

    /// <summary>The target ids to replace inside, or null for everywhere.</summary>
    private static HashSet<string>? Scope(CoherenceIssue issue)
    {
        if (issue.Everywhere || issue.Targets is not { Count: > 0 })
        {
            return null;
        }

        // "npc-anna-lind.secrets" is Anna's record; "f12" a fact; "creatureMinds" the creature's mind.
        return issue.Targets.Select(t => t.Split('.')[0].Split(' ')[^1].Trim()).ToHashSet(StringComparer.Ordinal);
    }

    private static IEnumerable<JToken> Nodes(JToken root, HashSet<string> scope)
    {
        foreach (var obj in All(root).OfType<JObject>())
        {
            if (IdKeys.Any(k => obj[k] is JValue { Type: JTokenType.String } v && scope.Contains((string)v.Value!)))
            {
                yield return obj;
            }
        }

        foreach (var property in All(root).OfType<JProperty>().Where(p => scope.Contains(p.Name)))
        {
            yield return property.Value;
        }
    }

    private static IEnumerable<JToken> All(JToken token) => token is JContainer container ? container.DescendantsAndSelf() : [token];

    private static Regex Pattern(string from) => new($@"(?<![\p{{L}}\p{{N}}]){Regex.Escape(from)}(?![\p{{L}}\p{{N}}])", RegexOptions.CultureInvariant);

    /// <summary>Replaces the whole phrase, and its capitalised form at the start of a sentence.</summary>
    private static string Replace(string text, string from, string to, ref int total)
    {
        var count = 0;
        text = Pattern(from).Replace(text, _ =>
        {
            count++;
            return to;
        });
        if (from.Length > 0 && to.Length > 0 && char.IsLower(from[0]))
        {
            var upper = char.ToUpperInvariant(from[0]) + from[1..];
            text = Pattern(upper).Replace(text, _ =>
            {
                count++;
                return char.ToUpperInvariant(to[0]) + to[1..];
            });
        }

        total += count;
        return text;
    }
}
