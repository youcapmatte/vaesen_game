using Newtonsoft.Json;
using Newtonsoft.Json.Linq;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The item a critic or check target names, as JSON (fixes 5, sections 3 and 4): to tell whether a patch changed it,
/// and to show the re-check what it was and what it is.
/// </summary>
public static class CaseItems
{
    private static readonly string[] IdKeys = ["characterId", "id", "threadId", "vaesenId"];

    /// <summary>The packet as a tree, without its validation, to look items up in many times.</summary>
    public static JToken Tree(CasePacket packet)
    {
        var root = JToken.FromObject(packet);
        ((JObject)root).Remove("validation");
        return root;
    }

    /// <summary>
    /// The item <paramref name="target"/> names: "f2", "c66", "hook", "scene:mill", "truth.summary", "npc-x.knows" (the
    /// whole person: a fix to what they know may rightly change what they think), or null when the case has none.
    /// </summary>
    public static string? Json(JToken root, string target)
    {
        var t = target.Trim();
        JToken? item = t switch
        {
            "" => null,
            "hook" => root["hook"],
            "placesUsed" => root["level"]?["placesUsed"],
            "climax" => root["climax"],
            "channels" or "channel" => root["channels"],
            "eventFacts" => root["truth"]?["escalation"],
            _ when t.StartsWith("scene:", StringComparison.Ordinal) => root["level"]?["sceneDescriptions"]?[t[6..]],
            _ when t.StartsWith("truth.", StringComparison.Ordinal) => root["truth"]?[t[6..]],
            _ when t.Contains("creatureMind", StringComparison.OrdinalIgnoreCase) => root["creatureMinds"],
            _ => ById(root, t.Split('.')[0].Split(' ')[^1]),
        };

        // A fact is shown with the channels that reveal it (review of case 09, problem 2): a fix that adds a way in
        // changes no fact, and the re-check must see the ways in to confirm it.
        if (item is JObject fact && root["facts"] is JArray facts && fact.Parent == facts && root["channels"] is JArray channels)
        {
            var id = (string?)fact["id"];
            return new JObject { ["fact"] = fact, ["channels"] = new JArray(channels.Where(c => (string?)c["factId"] == id)) }.ToString(Formatting.None);
        }

        return item?.ToString(Formatting.None);
    }

    public static string? Json(CasePacket packet, string target) => Json(Tree(packet), target);

    /// <summary>
    /// The item whose own id is <paramref name="id"/>: of every object that carries it, the shallowest (review of case
    /// 09, problem 1). A cast record is at cast[i]; another person's relationship to it, with the same characterId, is
    /// deeper, and taking the first in document order showed the re-check a relationship instead of the person.
    /// </summary>
    private static JToken? ById(JToken root, string id) =>
        root.SelectTokens("$..*").OfType<JObject>()
            .Where(o => IdKeys.Any(k => o[k] is JValue { Type: JTokenType.String } v && (string)v.Value! == id))
            .MinBy(o => o.Ancestors().Count());
}
