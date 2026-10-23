using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A canonical character (spec 5.4, 6.4): an identity every Society meets, approved once by the owner. Each Society
/// gets its own instance; the identity (personality, drives, voice, background) never changes, so the character stays
/// recognisable, while attitude, knowledge and status are per Society.
/// </summary>
public sealed class CharacterDefinition : ContentRecord
{
    [JsonProperty("identity")]
    public CharacterIdentity Identity { get; set; } = new();

    [JsonProperty("personality")]
    public PersonalityProfile Personality { get; set; } = new();

    [JsonProperty("drives")]
    public List<Drive> Drives { get; set; } = [];

    [JsonProperty("voice")]
    public CharacterVoice Voice { get; set; } = new();

    [JsonProperty("background")]
    public string Background { get; set; } = "";

    /// <summary>Habits, debts, connections and secrets the engine can promote into a plot.</summary>
    [JsonProperty("latentHooks")]
    public List<LatentHook> LatentHooks { get; set; } = [];

    /// <summary>Reach to other characters or institutions, possibly in other towns.</summary>
    [JsonProperty("networkLinks")]
    public List<NetworkLink> NetworkLinks { get; set; } = [];

    /// <summary>How they regard the Society when a Society first meets them (-3 hostile to +3 devoted).</summary>
    [JsonProperty("startingAttitude")]
    public int StartingAttitude { get; set; }

    [JsonProperty("attitudeReason")]
    public string AttitudeReason { get; set; } = "";
}

public sealed class CharacterIdentity
{
    [JsonProperty("age")]
    public int Age { get; set; }

    [JsonProperty("gender")]
    public string Gender { get; set; } = "";

    [JsonProperty("occupation")]
    public string Occupation { get; set; } = "";

    /// <summary>Social rank in the setting's terms ("burgher", "crofter", "clergy", "nobility").</summary>
    [JsonProperty("rank")]
    public string Rank { get; set; } = "";

    [JsonProperty("home")]
    public string Home { get; set; } = "";

    [JsonProperty("appearance")]
    public string Appearance { get; set; } = "";
}

/// <summary>Big Five, each 1 (low) to 5 (high).</summary>
public sealed class PersonalityProfile
{
    [JsonProperty("openness")]
    public int Openness { get; set; } = 3;

    [JsonProperty("conscientiousness")]
    public int Conscientiousness { get; set; } = 3;

    [JsonProperty("extraversion")]
    public int Extraversion { get; set; } = 3;

    [JsonProperty("agreeableness")]
    public int Agreeableness { get; set; } = 3;

    [JsonProperty("neuroticism")]
    public int Neuroticism { get; set; } = 3;
}

public sealed class Drive
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    /// <summary>1 (a preference) to 3 (will act on it at any cost).</summary>
    [JsonProperty("strength")]
    public int Strength { get; set; } = 2;
}

/// <summary>How they speak: what the NPC voice prompt is built from.</summary>
public sealed class CharacterVoice
{
    [JsonProperty("register")]
    public string Register { get; set; } = "";

    [JsonProperty("mannerisms")]
    public List<string> Mannerisms { get; set; } = [];

    [JsonProperty("sampleLines")]
    public List<string> SampleLines { get; set; } = [];
}

public sealed class LatentHook
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>One of <see cref="LatentHookKinds"/>.</summary>
    [OneOf(typeof(LatentHookKinds))]
    [JsonProperty("kind")]
    public string Kind { get; set; } = LatentHookKinds.Secret;

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}

public static class LatentHookKinds
{
    public const string Habit = "habit";
    public const string Debt = "debt";
    public const string Connection = "connection";
    public const string Secret = "secret";
    public const string Ambition = "ambition";

    public static readonly IReadOnlyList<string> All = [Habit, Debt, Connection, Secret, Ambition];
}

public sealed class NetworkLink
{
    /// <summary>A character id or an institution id.</summary>
    [JsonProperty("targetId")]
    public string TargetId { get; set; } = "";

    [JsonProperty("place")]
    public string Place { get; set; } = "";

    [JsonProperty("nature")]
    public string Nature { get; set; } = "";

    /// <summary>What influence it can exert ("can have the police refuse the Society interviews").</summary>
    [JsonProperty("influence")]
    public string Influence { get; set; } = "";
}
