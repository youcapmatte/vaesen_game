using System.Text.RegularExpressions;
using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// Renames people everywhere in a case (fixes 4, section 4): when a step renames someone after names existed, the old
/// name goes from the truth, the events, the facts, the channels and every step's kept output, so later fixes do not
/// bring it back.
/// </summary>
public static class Renames
{
    /// <summary>After a step that names people passed: rename everyone it renamed, everywhere, and log it.</summary>
    public static void AfterPass(MysteryDraft draft, CasePacket before, int caseAttempt, string step)
    {
        var renames = before.Cast
            .Join(draft.Packet.Cast, b => b.CharacterId, a => a.CharacterId, (b, a) => (Old: b.Name, New: a.Name))
            .Where(r => r.Old.Length > 0 && r.Old != r.New)
            .ToList();
        if (renames.Count == 0)
        {
            return;
        }

        var places = Apply(draft, renames);
        draft.Log.Add(new GenerationLogEntry
        {
            CaseAttempt = caseAttempt,
            Step = step,
            Kind = LogEntryKinds.Rename,
            Passed = true,
            Issues = renames.Select(r => CoherenceIssue.Warning(IssueCodes.Consistency, "cast", $"Renamed {r.Old} to {r.New} in {places} place(s) across the case.")).ToList(),
        });
    }

    /// <summary>Replaces each old name (whole name, then first name and surname where they changed); returns how many places.</summary>
    public static int Apply(MysteryDraft draft, IReadOnlyList<(string Old, string New)> renames)
    {
        var total = 0;
        string Rewrite(string json)
        {
            foreach (var (old, @new) in renames)
            {
                json = Replace(json, old, @new, ref total);
                var (oldFirst, oldLast) = Split(old);
                var (newFirst, newLast) = Split(@new);
                if (oldFirst.Length > 0 && oldFirst != newFirst)
                {
                    json = Replace(json, oldFirst, newFirst, ref total);
                }

                if (oldLast.Length > 0 && newLast.Length > 0 && oldLast != newLast)
                {
                    json = Replace(json, oldLast, newLast, ref total);
                }
            }

            return json;
        }

        draft.Packet = JsonConvert.DeserializeObject<CasePacket>(Rewrite(JsonConvert.SerializeObject(draft.Packet)))!;
        foreach (var (step, output) in draft.Outputs.ToList())
        {
            draft.Outputs[step] = JsonConvert.DeserializeObject(Rewrite(JsonConvert.SerializeObject(output)), output.GetType())!;
        }

        return total;
    }

    private static string Replace(string text, string old, string @new, ref int total)
    {
        var count = 0;
        var result = Regex.Replace(text, $@"(?<![\p{{L}}-]){Regex.Escape(old)}(?![\p{{L}}-])", _ =>
        {
            count++;
            return @new;
        });
        total += count;
        return result;
    }

    private static (string First, string Last) Split(string name)
    {
        var parts = name.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        return parts.Length switch
        {
            0 => ("", ""),
            1 => (parts[0], ""),
            _ => (parts[0], parts[^1]),
        };
    }
}
