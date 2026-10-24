using System.Text.RegularExpressions;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Mechanical fixes done by code, not a model (review 2, item 2): a rename ("split pine" to "cleft pine"), a date, a
/// wrong fact id. Each replacement is exact text, applied everywhere in the case or only inside the issue's targets,
/// in the packet and in every step's kept output, so a later patch does not bring the old text back.
/// </summary>
public static class TextReplacements
{
    /// <summary>The keys that name an item in the packet and the outputs, to find a target's object.</summary>
    private static readonly string[] IdKeys = ["characterId", "id", "threadId", "vaesenId"];

    /// <summary>Applies the issue's replacements; returns how many places changed.</summary>
    public static int Apply(MysteryDraft draft, CoherenceIssue issue)
    {
        // An id is never replaced (review 3): ids are fixed, however they read, and a critic once "fixed" role ids into names.
        var ids = Ids(draft.Packet);
        if (issue.Replacements?.Where(r => !ids.Contains(r.From.Trim())).ToList() is not { Count: > 0 } replacements)
        {
            return 0;
        }

        var scope = Scope(issue);
        var total = 0;
        string Rewrite(string json)
        {
            var root = JToken.Parse(json);
            foreach (var node in Nodes(root, scope))
            {
                foreach (var value in All(node).OfType<JValue>().Where(v => v.Type == JTokenType.String).ToList())
                {
                    var text = (string)value.Value!;
                    foreach (var r in replacements)
                    {
                        text = Replace(text, r.From, r.To, ref total);
                    }

                    value.Value = text;
                }
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

        return total;
    }

    /// <summary>Every id in the case: people, offstage people, facts, channels, events, beats, levers and decoy threads.</summary>
    private static HashSet<string> Ids(CasePacket packet) =>
        packet.Cast.Select(c => c.CharacterId)
            .Concat(packet.OffstagePeople.Select(o => o.Id))
            .Concat(packet.Facts.Select(f => f.Id))
            .Concat(packet.Channels.Select(c => c.Id))
            .Concat(packet.Truth.Escalation.Select(e => e.Id))
            .Concat(packet.Beats.Select(b => b.Id))
            .Concat(packet.ResolutionLevers.Select(l => l.Id))
            .Concat(packet.Decoys.Select(d => d.ThreadId))
            .ToHashSet(StringComparer.Ordinal);

    /// <summary>Whether any replaced text is still there, in the same scope: the check that the fix held.</summary>
    public static bool Holds(MysteryDraft draft, CoherenceIssue issue)
    {
        var root = JToken.Parse(JsonConvert.SerializeObject(draft.Packet));
        ((JObject)root).Remove("validation");
        var froms = (issue.Replacements ?? []).Select(r => r.From)
            .SelectMany(f => f.Length > 0 && char.IsLower(f[0]) ? new[] { f, char.ToUpperInvariant(f[0]) + f[1..] } : [f])
            .ToList();
        return !Nodes(root, Scope(issue))
            .SelectMany(n => All(n).OfType<JValue>())
            .Where(v => v.Type == JTokenType.String)
            .Any(v => froms.Exists(f => Pattern(f).IsMatch((string)v.Value!)));
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

    private static IEnumerable<JToken> Nodes(JToken root, HashSet<string>? scope)
    {
        if (scope is null)
        {
            yield return root;
            yield break;
        }

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
