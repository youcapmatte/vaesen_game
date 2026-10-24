using System.Globalization;
using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>One problem a validator found, with where it is and how it might be fixed. Fed back to the model on a re-roll.</summary>
public sealed class CoherenceIssue
{
    /// <summary>A stable code from <see cref="IssueCodes"/>, so logs can count issues by kind.</summary>
    [JsonProperty("code")]
    public string Code { get; set; } = "";

    /// <summary>One of <see cref="IssueSeverities"/>. Only errors fail a step.</summary>
    [JsonProperty("severity")]
    public string Severity { get; set; } = IssueSeverities.Error;

    /// <summary>Where: "fact:f3", "channel:c7", "cast:npc-anna", "hook", "content:vaesen/nack".</summary>
    [JsonProperty("location")]
    public string Location { get; set; } = "";

    [JsonProperty("explanation")]
    public string Explanation { get; set; } = "";

    [JsonProperty("suggestedFix")]
    public string? SuggestedFix { get; set; }

    /// <summary>"structural" (the C# checks) or "critic" (the model's judgement).</summary>
    [JsonProperty("source")]
    public string Source { get; set; } = IssueSources.Structural;

    /// <summary>The critic's issues: the ids they touch ("r3", "truth.hiddenHandMotive", "npc-anna-lind.knows"), for routing.</summary>
    [JsonProperty("targets", NullValueHandling = NullValueHandling.Ignore)]
    public List<string>? Targets { get; set; }

    /// <summary>
    /// A fix that was tried and then verified (review 2, item 1): a structural warning that no longer appears, a critic
    /// issue a verifying look found gone, or a replacement whose old text is gone. Never set on trust.
    /// </summary>
    [JsonProperty("applied", DefaultValueHandling = DefaultValueHandling.Ignore)]
    public bool Applied { get; set; }

    /// <summary>A fix was tried and did not hold: what still stands, as the verifying look saw it.</summary>
    [JsonProperty("unresolved", NullValueHandling = NullValueHandling.Ignore)]
    public string? Unresolved { get; set; }

    /// <summary>
    /// Mechanical fixes (review 2, item 2): exact text to replace, done by code and logged, never by a model. A rename,
    /// a date, a wrong fact id.
    /// </summary>
    [JsonProperty("replacements", NullValueHandling = NullValueHandling.Ignore)]
    public List<TextReplacement>? Replacements { get; set; }

    /// <summary>Replace everywhere in the case (a rename); otherwise only inside the targets.</summary>
    [JsonProperty("everywhere", DefaultValueHandling = DefaultValueHandling.Ignore)]
    public bool Everywhere { get; set; }

    /// <summary>Field values to set by code (fixes 5, section 1).</summary>
    [JsonProperty("setFields", NullValueHandling = NullValueHandling.Ignore)]
    public List<FieldSet>? SetFields { get; set; }

    /// <summary>The parts that need a model, each with its own target and instruction (fixes 5, section 3).</summary>
    [JsonProperty("patches", NullValueHandling = NullValueHandling.Ignore)]
    public List<PatchAsk>? Patches { get; set; }

    /// <summary>The fix's parts as the router split them, with their status.</summary>
    [JsonProperty("parts", NullValueHandling = NullValueHandling.Ignore)]
    public List<FixPart>? Parts { get; set; }

    /// <summary>One of <see cref="FixLabels"/> once a fix was tried or found impossible; null when none was.</summary>
    [JsonProperty("label", NullValueHandling = NullValueHandling.Ignore)]
    public string? Label { get; set; }

    /// <summary>Why the critic moved this issue from its check's default (blocking or advisory), if it did.</summary>
    [JsonProperty("impactReason", NullValueHandling = NullValueHandling.Ignore)]
    public string? ImpactReason { get; set; }

    /// <summary>
    /// A critic issue that breaks the case at the table if it stays (review 3, item 3): a scene or opening that gives
    /// away a gated clue, a hook that contradicts the cast or the facts, a period error players would notice, someone
    /// saying what they cannot know. Unfixed, it marks the case as needing review. Otherwise the issue is advisory.
    /// </summary>
    [JsonProperty("blocking", DefaultValueHandling = DefaultValueHandling.Ignore)]
    public bool Blocking { get; set; }

    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public bool IsError => Severity == IssueSeverities.Error;

    public static CoherenceIssue Error(string code, string location, string explanation, string? fix = null) =>
        new() { Code = code, Severity = IssueSeverities.Error, Location = location, Explanation = explanation, SuggestedFix = fix };

    public static CoherenceIssue Warning(string code, string location, string explanation, string? fix = null) =>
        new() { Code = code, Severity = IssueSeverities.Warning, Location = location, Explanation = explanation, SuggestedFix = fix };

    /// <summary>" Allowed: a, b, c." for an issue about a value outside a fixed list, so a retry knows what to pick.</summary>
    public static string Allowed(IEnumerable<string> values) => $" Allowed: {string.Join(", ", values)}.";

    public override string ToString() => $"[{Severity}] {Code} at {Location}: {Explanation}";
}

public sealed class TextReplacement
{
    [JsonProperty("from")]
    public string From { get; set; } = "";

    [JsonProperty("to")]
    public string To { get; set; } = "";
}

/// <summary>A field set by code (fixes 5, section 1): "cast[id=npc-x].identity.rank" to "worker". Never a text replacement.</summary>
public sealed class FieldSet
{
    [JsonProperty("path")]
    public string Path { get; set; } = "";

    [JsonProperty("value")]
    public string Value { get; set; } = "";
}

/// <summary>A part of a critic issue that needs a model: the item to patch and what to change in it.</summary>
public sealed class PatchAsk
{
    [JsonProperty("target")]
    public string Target { get; set; } = "";

    [JsonProperty("fix")]
    public string Fix { get; set; } = "";
}

/// <summary>
/// One part of an issue's fix with its owner and status (fixes 5, section 3): an issue is fixed only when every part
/// is, and a part is done only when its post-condition holds (the data changed), never because a call succeeded.
/// </summary>
public sealed class FixPart
{
    /// <summary>"replace", "setField" or "patch".</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = "";

    /// <summary>"code", or the step that patches it.</summary>
    [JsonProperty("owner")]
    public string Owner { get; set; } = "";

    /// <summary>The item it is about ("c66", "npc-x.knows"), or the pair or path for code.</summary>
    [JsonProperty("target")]
    public string Target { get; set; } = "";

    /// <summary>What to change: the pair, the value, or the patch's instruction.</summary>
    [JsonProperty("detail")]
    public string Detail { get; set; } = "";

    /// <summary>One of <see cref="FixPartStatuses"/>.</summary>
    [JsonProperty("status")]
    public string Status { get; set; } = FixPartStatuses.Pending;

    /// <summary>What happened: matches, copies left, why it failed.</summary>
    [JsonProperty("note", NullValueHandling = NullValueHandling.Ignore)]
    public string? Note { get; set; }
}

public static class FixPartKinds
{
    public const string Replace = "replace";
    public const string SetField = "setField";
    public const string Patch = "patch";
}

public static class FixPartStatuses
{
    public const string Pending = "pending";

    /// <summary>Its post-condition holds: the data changed as asked.</summary>
    public const string Done = "done";

    /// <summary>It matched nothing, failed its post-condition, or its step's patch did not pass.</summary>
    public const string Failed = "failed";

    /// <summary>It broke a structural check, one repair did not hold, and it was undone.</summary>
    public const string Undone = "undone";

    /// <summary>No step owns it: no target, or a target the case does not have.</summary>
    public const string Unrouted = "unrouted";
}

/// <summary>The only four labels a fixed issue carries (fixes 5, section 4).</summary>
public static class FixLabels
{
    /// <summary>Every part passed its post-condition, and the scoped re-check said it is gone.</summary>
    public const string Verified = "fixed (verified)";

    /// <summary>Every part passed its post-condition; no re-check ran.</summary>
    public const string Unverified = "fixed (unverified)";

    /// <summary>A part broke a structural check and was undone.</summary>
    public const string DidNotHold = "did not hold";

    /// <summary>A part matched nothing, failed its post-condition or had no route, or the re-check said it is still there.</summary>
    public const string NotFixed = "not fixed";

    public static bool IsFixed(string? label) => label is Verified or Unverified;
}

public static class IssueSeverities
{
    public const string Error = "error";
    public const string Warning = "warning";
}

public static class IssueSources
{
    public const string Structural = "structural";
    public const string Critic = "critic";
}

/// <summary>The issue codes of the structural checks (spec 7.10). The critic uses its own codes, prefixed "critic.".</summary>
public static class IssueCodes
{
    public const string Schema = "schema";
    public const string UnknownReference = "unknownReference";
    public const string Vocabulary = "vocabulary";
    public const string Unreachable = "unreachable";
    public const string Redundancy = "redundancy";
    public const string SinglePointOfFailure = "singlePointOfFailure";
    public const string EntryStyle = "entryStyle";
    public const string Provenance = "provenance";
    public const string Decoy = "decoy";
    public const string FairPlay = "fairPlay";
    public const string AffordanceFit = "affordanceFit";
    public const string CastCoherence = "castCoherence";
    public const string Giveaway = "giveaway";
    public const string Opening = "opening";
    public const string Resolution = "resolution";
    public const string Beats = "beats";
    public const string Content = "content";

    /// <summary>A step gave up, so everything after it is missing (reported once, not as every check on blank data).</summary>
    public const string Incomplete = "incomplete";

    /// <summary>A near miss in a fixed-value field was repaired in code (a warning; see EnumRepair).</summary>
    public const string Repaired = "repaired";

    /// <summary>An item built on something a fix revised, sent to its step to agree with it.</summary>
    public const string Consistency = "consistency";
}

/// <summary>A parsed channel access requirement (see <see cref="ChannelAccess"/>).</summary>
public sealed record AccessRequirement(string Kind, string Value, int? Threshold)
{
    public const string KnowsFact = "knowsFact";
    public const string Trust = "trust";
    public const string Roll = "roll";
    public const string Time = "time";
    public const string Place = "place";
    public const string Item = "item";

    /// <summary>The channel is the aftermath of a beat and opens once it has fired ("afterBeat:b4").</summary>
    public const string AfterBeat = "afterBeat";

    public static readonly IReadOnlyList<string> Kinds = [KnowsFact, Trust, Roll, Time, Place, Item, AfterBeat];

    /// <summary>The skills a roll can use.</summary>
    public static readonly IReadOnlyList<string> Rolls =
        ["investigation", "inspection", "observation", "learning", "manipulation", "force", "agility", "stealth", "medicine", "survival", "vigilance"];

    /// <summary>The times a channel can be bound to.</summary>
    public static readonly IReadOnlyList<string> Times = ["dawn", "morning", "midday", "day", "evening", "dusk", "night", "sunday", "weekday"];

    /// <summary>Every requirement a channel in a case with these facts and places can have: the closed list for its schema.</summary>
    public static List<string> Allowed(IEnumerable<string> factIds, IEnumerable<string> placeIds, IEnumerable<string>? beatIds = null) =>
        factIds.Select(f => $"{KnowsFact}:{f}")
            .Concat((beatIds ?? []).Select(b => $"{AfterBeat}:{b}"))
            .Concat(Enumerable.Range(0, 4).Select(n => $"{Trust}>={n}"))
            .Concat(Rolls.Select(r => $"{Roll}:{r}"))
            .Concat(Times.Select(t => $"{Time}:{t}"))
            .Concat(placeIds.Select(p => $"{Place}:{p}"))
            .ToList();

    /// <summary>"knowsFact:f7", "roll:investigation", "time:night", "place:mill", "item:key", "trust&gt;=2".</summary>
    public static AccessRequirement? Parse(string? text)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return null;
        }

        var trimmed = text.Trim();
        if (trimmed.StartsWith(Trust, StringComparison.Ordinal))
        {
            var rest = trimmed[Trust.Length..].TrimStart('>', '=', ':', ' ');
            return int.TryParse(rest, NumberStyles.Integer, CultureInfo.InvariantCulture, out var level)
                ? new AccessRequirement(Trust, rest, level)
                : null;
        }

        var colon = trimmed.IndexOf(':', StringComparison.Ordinal);
        if (colon <= 0 || colon == trimmed.Length - 1)
        {
            return null;
        }

        var kind = trimmed[..colon];
        return Kinds.Contains(kind) ? new AccessRequirement(kind, trimmed[(colon + 1)..].Trim(), null) : null;
    }
}
