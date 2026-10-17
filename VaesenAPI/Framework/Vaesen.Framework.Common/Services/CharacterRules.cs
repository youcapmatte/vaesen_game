using System.Text.RegularExpressions;

namespace Vaesen.Framework.Common.Services;

/// <summary>What a valid character looks like. Used when creating and when updating characters.</summary>
public static partial class CharacterRules
{
    public const int MaxHealthPoints = 100_000;

    /// <summary>First or last name: required; 1-32 letters, with single spaces, apostrophes or hyphens between words.</summary>
    public static string? ValidatePersonName(string? value, string field) =>
        value is null ? $"{field} is required."
        : PersonNamePattern().IsMatch(value) ? null
        : $"{field} must be 1-32 letters, optionally with single spaces, apostrophes or hyphens between them.";

    /// <summary>Class: optional; must be one of <paramref name="classes"/> (case-insensitive).</summary>
    public static string? ValidateClass(string? value, IReadOnlyList<string> classes) =>
        value is null || NormalizeClass(value, classes) is not null ? null : $"Class must be one of: {string.Join(", ", classes)}.";

    /// <summary>The class as written in <paramref name="classes"/> ("occultist" -> "Occultist"), or null if it isn't one.</summary>
    public static string? NormalizeClass(string? value, IReadOnlyList<string> classes) =>
        value is null ? null : classes.FirstOrDefault(c => string.Equals(c, value, StringComparison.OrdinalIgnoreCase));

    public static string? ValidateHealthPoints(int? value) =>
        value is null or (>= 0 and <= MaxHealthPoints) ? null : $"Health points must be between 0 and {MaxHealthPoints}.";

    /// <summary>Trims; empty or whitespace becomes null (field not set / cleared).</summary>
    public static string? Clean(string? value) => string.IsNullOrWhiteSpace(value) ? null : value.Trim();

    [GeneratedRegex(@"^(?=.{1,32}$)\p{L}+(?:[ '-]\p{L}+)*$")]
    private static partial Regex PersonNamePattern();
}
