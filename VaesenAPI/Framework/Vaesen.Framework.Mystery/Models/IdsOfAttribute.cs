namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// The property holds ids of one kind that exist in the case: a fact, a person, a place. Each call's output schema
/// carries the case's ids of that kind as an enum (fixes 3, section 1), so the model cannot write a fact's title where
/// its id belongs or invent one. A call that does not know the ids yet (the step that defines them) leaves it open.
/// </summary>
[AttributeUsage(AttributeTargets.Property)]
public sealed class IdsOfAttribute(string kind) : Attribute
{
    /// <summary>One of <see cref="IdKinds"/>.</summary>
    public string Kind { get; } = kind;
}

/// <summary>The kinds of ids a step's output refers to.</summary>
public static class IdKinds
{
    public const string Fact = "fact";
    public const string Event = "event";
    public const string Cast = "cast";
    public const string Place = "place";
    public const string Affordance = "affordance";

    /// <summary>A deed of a deep resolution lever ("r3-d1").</summary>
    public const string Deed = "deed";

    /// <summary>A channel's access requirement: "knowsFact:f3", "trust>=2", "roll:investigation", "time:night", "place:mill".</summary>
    public const string Requirement = "requirement";
}
