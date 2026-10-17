namespace Vaesen.Framework.Common.Models;

/// <summary>The "Characters" configuration section: game rules for characters.</summary>
public sealed class CharacterOptions
{
    public const string SectionName = "Characters";

    /// <summary>Vaesen's archetypes, used when configuration doesn't list classes.</summary>
    public static readonly IReadOnlyList<string> DefaultClasses =
    [
        "Academic",
        "Doctor",
        "Hunter",
        "Occultist",
        "Officer",
        "Priest",
        "Private Detective",
        "Servant",
        "Vagabond",
        "Writer",
    ];

    /// <summary>Classes a character can have. Empty means <see cref="DefaultClasses"/>.</summary>
    public List<string> Classes { get; set; } = [];

    public IReadOnlyList<string> EffectiveClasses => Classes.Count > 0 ? Classes : DefaultClasses;
}
