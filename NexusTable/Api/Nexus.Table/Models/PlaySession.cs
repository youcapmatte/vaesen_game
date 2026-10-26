using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Models;

/// <summary>
/// One evening of play on a mystery. A mystery may take several: each session starts from the play state the last
/// one ended with, so the Gamemaster picks up where the table left off. Id: "playsession-{guid}".
/// </summary>
public sealed class PlaySession : BaseCore
{
    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("mysteryTitle")]
    public string MysteryTitle { get; set; } = "";

    /// <summary>1 for the first session of the mystery, then 2, 3...</summary>
    [JsonProperty("number")]
    public int Number { get; set; }

    [JsonProperty("startedAt")]
    public DateTime StartedAt { get; set; }

    [JsonProperty("endedAt", NullValueHandling = NullValueHandling.Ignore)]
    public DateTime? EndedAt { get; set; }

    [JsonIgnore]
    public bool IsOpen => EndedAt is null;

    /// <summary>
    /// The session clock: seconds since the session started, by the API's own clock, so it is the same for every
    /// browser and for the Gamemaster whatever their computers' clocks say.
    /// </summary>
    public int ClockAt(DateTime at) => (int)Math.Max(0, Math.Floor((at - StartedAt).TotalSeconds));

    /// <summary>The time at a reading of the session clock: never before the start, never after now.</summary>
    public DateTime TimeAt(double seconds, DateTime now)
    {
        var at = StartedAt.AddSeconds(Math.Max(0, seconds));
        return at > now ? now : at;
    }

    /// <summary>Where the mystery stands: kept up to date by the Gamemaster's scribe, carried into the next session.</summary>
    [JsonProperty("state")]
    public PlayState State { get; set; } = new();

    /// <summary>Tests the Gamemaster has asked for and nobody has rolled yet.</summary>
    [JsonProperty("pendingTests")]
    public List<TestRequest> PendingTests { get; set; } = [];

    /// <summary>What players may see: no play state (the scribe writes it for the Gamemaster).</summary>
    public PlaySession ForPlayers() => new()
    {
        Id = Id,
        SocietyId = SocietyId,
        MysteryId = MysteryId,
        MysteryTitle = MysteryTitle,
        Number = Number,
        StartedAt = StartedAt,
        EndedAt = EndedAt,
        PendingTests = PendingTests,
        CreatedAt = CreatedAt,
        UpdatedAt = UpdatedAt,
        Version = Version,
    };
}

/// <summary>
/// Where a mystery stands, in the case file's own ids: what the investigators have learned and how, whom they have
/// met, which beats have happened, what is still open. Written by the scribe after each exchange; read by the
/// Gamemaster when it joins, so a new session resumes from here.
/// </summary>
public sealed class PlayState
{
    /// <summary>The story so far, as the investigators lived it: what a recap is made from.</summary>
    [JsonProperty("summary")]
    public string Summary { get; set; } = "";

    /// <summary>Where the investigators are and when, in the world: "the Forsby inn, the second evening".</summary>
    [JsonProperty("scene")]
    public string Scene { get; set; } = "";

    [JsonProperty("factsFound")]
    public List<FoundFact> FactsFound { get; set; } = [];

    /// <summary>Channels the investigators have used, by id ("c12").</summary>
    [JsonProperty("channelsUsed")]
    public List<string> ChannelsUsed { get; set; } = [];

    [JsonProperty("peopleMet")]
    public List<MetPerson> PeopleMet { get; set; } = [];

    /// <summary>Escalation events and beats that have happened, by id ("e3", "b2").</summary>
    [JsonProperty("beatsFired")]
    public List<string> BeatsFired { get; set; } = [];

    /// <summary>Lever deeds done, by id ("r1-d1").</summary>
    [JsonProperty("deedsDone")]
    public List<string> DeedsDone { get; set; } = [];

    /// <summary>Leads the investigators have and haven't followed, promises made, things left hanging.</summary>
    [JsonProperty("openThreads")]
    public List<string> OpenThreads { get; set; } = [];

    [JsonProperty("updatedAt", NullValueHandling = NullValueHandling.Ignore)]
    public DateTime? UpdatedAt { get; set; }

    /// <summary>The last log event the state takes into account.</summary>
    [JsonProperty("throughEventId", NullValueHandling = NullValueHandling.Ignore)]
    public string? ThroughEventId { get; set; }
}

public sealed class FoundFact
{
    [JsonProperty("factId")]
    public string FactId { get; set; } = "";

    /// <summary>How they learned it: "Halvar the foreman told Elin, after she bought him a drink".</summary>
    [JsonProperty("how")]
    public string How { get; set; } = "";

    [JsonProperty("session")]
    public int Session { get; set; }
}

public sealed class MetPerson
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>How it went, and how they feel about the investigators now.</summary>
    [JsonProperty("note")]
    public string Note { get; set; } = "";
}

/// <summary>A test the Gamemaster asks for: who rolls what, how hard, and how many sixes it takes.</summary>
public sealed class TestRequest
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("characterName")]
    public string CharacterName { get; set; } = "";

    [JsonProperty("skill", NullValueHandling = NullValueHandling.Ignore)]
    public string? Skill { get; set; }

    [JsonProperty("attribute")]
    public string Attribute { get; set; } = "";

    /// <summary>-3 (very hard) to +3 (very easy).</summary>
    [JsonProperty("difficulty")]
    public int Difficulty { get; set; }

    /// <summary>Sixes needed to succeed; usually one.</summary>
    [JsonProperty("successesNeeded")]
    public int SuccessesNeeded { get; set; } = 1;

    [JsonProperty("reason", NullValueHandling = NullValueHandling.Ignore)]
    public string? Reason { get; set; }

    /// <summary>The dice the character has for it, as the table will roll them.</summary>
    [JsonProperty("pool")]
    public List<RollPoolPart> Pool { get; set; } = [];

    [JsonProperty("poolSize")]
    public int PoolSize { get; set; }

    [JsonProperty("at")]
    public DateTime At { get; set; }
}

/// <summary>What the Gamemaster agent sends to ask for a test, by the character's name or id and the skill's name or id.</summary>
public sealed class TestRequestInput
{
    [JsonProperty("character")]
    public string Character { get; set; } = "";

    /// <summary>A skill, or an attribute for an attribute test (Fear tests are Logic or Empathy).</summary>
    [JsonProperty("skill")]
    public string Skill { get; set; } = "";

    [JsonProperty("difficulty")]
    public int Difficulty { get; set; }

    [JsonProperty("successesNeeded")]
    public int SuccessesNeeded { get; set; } = 1;

    [JsonProperty("reason")]
    public string? Reason { get; set; }
}

/// <summary>
/// One thing that happened at the table, in order: what was said to and by the Gamemaster, rolls, tests asked for,
/// notes, the session starting and ending. Together they are the session log. Id: "sessionevent-{guid}".
/// </summary>
public sealed class SessionEvent : BaseCore
{
    [JsonProperty("sessionId")]
    public string SessionId { get; set; } = "";

    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    /// <summary>When it happened at the table. For a note, when what it is about happened: when its writer began it.</summary>
    [JsonProperty("at")]
    public DateTime At { get; set; }

    /// <summary>When it happened on the session clock: seconds since the session started (<see cref="SessionClock"/>).</summary>
    [JsonProperty("sessionTime")]
    public int SessionTime { get; set; }

    /// <summary>A note sent a while after the moment it is about: when it was sent.</summary>
    [JsonProperty("sentAt", NullValueHandling = NullValueHandling.Ignore)]
    public DateTime? SentAt { get; set; }

    /// <summary>One of <see cref="SessionEventKinds"/>.</summary>
    [JsonProperty("kind")]
    public string Kind { get; set; } = "";

    [JsonProperty("accountId", NullValueHandling = NullValueHandling.Ignore)]
    public string? AccountId { get; set; }

    /// <summary>Who: a character, a player, "The Gamemaster", or an NPC's name for a line spoken as them.</summary>
    [JsonProperty("speaker")]
    public string Speaker { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    [JsonProperty("rollId", NullValueHandling = NullValueHandling.Ignore)]
    public string? RollId { get; set; }

    [JsonProperty("test", NullValueHandling = NullValueHandling.Ignore)]
    public TestRequest? Test { get; set; }

    /// <summary>For the host and the Gamemaster only (the scribe's state updates).</summary>
    [JsonProperty("hostOnly")]
    public bool HostOnly { get; set; }
}

public static class SessionEventKinds
{
    public const string SessionStarted = "sessionStarted";
    public const string SessionEnded = "sessionEnded";

    /// <summary>A player spoke to the Gamemaster (push-to-talk), transcribed.</summary>
    public const string PlayerSpoke = "playerSpoke";

    /// <summary>The Gamemaster's reply: narration and NPC lines, as spoken.</summary>
    public const string GmSaid = "gmSaid";

    public const string Roll = "roll";
    public const string TestRequested = "testRequested";

    /// <summary>A note to the Gamemaster: what happened, typed or dictated. The Gamemaster takes it into account.</summary>
    public const string Note = "note";

    public const string GmJoined = "gmJoined";
    public const string GmLeft = "gmLeft";

    /// <summary>The scribe updated the play state.</summary>
    public const string StateUpdated = "stateUpdated";

    public static readonly IReadOnlyList<string> FromAgent = [PlayerSpoke, GmSaid, Note, GmJoined, GmLeft];
}

/// <summary>The session clock as people and the Gamemaster read it.</summary>
public static class SessionClock
{
    /// <summary>"0:41:55": hours, minutes and seconds since the session started.</summary>
    public static string Stamp(int seconds)
    {
        var t = TimeSpan.FromSeconds(Math.Max(0, seconds));
        return $"{(int)t.TotalHours}:{t.Minutes:D2}:{t.Seconds:D2}";
    }
}

public sealed class NoteRequest
{
    [JsonProperty("text")]
    public string Text { get; set; } = "";

    /// <summary>
    /// How long ago, in seconds, the writer began the note, by their own browser's clock: the note is placed at that
    /// moment on the session clock. Only a duration crosses, so a computer whose clock is wrong still places it right.
    /// </summary>
    [JsonProperty("secondsAgo")]
    public double? SecondsAgo { get; set; }
}

/// <summary>Events the agent logs, in the order they happened.</summary>
public sealed class AgentEventsRequest
{
    [JsonProperty("events")]
    public List<AgentEvent> Events { get; set; } = [];
}

public sealed class AgentEvent
{
    [JsonProperty("kind")]
    public string Kind { get; set; } = "";

    [JsonProperty("at")]
    public DateTime? At { get; set; }

    /// <summary>When on the session clock (seconds since it started), as the agent keeps it; wins over <see cref="At"/>.</summary>
    [JsonProperty("sessionTime")]
    public double? SessionTime { get; set; }

    [JsonProperty("accountId")]
    public string? AccountId { get; set; }

    [JsonProperty("speaker")]
    public string Speaker { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}
