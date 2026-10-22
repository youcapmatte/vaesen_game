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

    [JsonIgnore]
    [System.Text.Json.Serialization.JsonIgnore]
    public bool IsError => Severity == IssueSeverities.Error;

    public static CoherenceIssue Error(string code, string location, string explanation, string? fix = null) =>
        new() { Code = code, Severity = IssueSeverities.Error, Location = location, Explanation = explanation, SuggestedFix = fix };

    public static CoherenceIssue Warning(string code, string location, string explanation, string? fix = null) =>
        new() { Code = code, Severity = IssueSeverities.Warning, Location = location, Explanation = explanation, SuggestedFix = fix };

    public override string ToString() => $"[{Severity}] {Code} at {Location}: {Explanation}";
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

    public static readonly IReadOnlyList<string> Kinds = [KnowsFact, Trust, Roll, Time, Place, Item];

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
