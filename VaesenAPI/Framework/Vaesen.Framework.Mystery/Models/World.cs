using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// One Society's copy of the world (spec 4.2, 6.2, 9): its persistent cast and what it has done to the world. Canon
/// plus this overlay is that Society's world. Nothing here affects another Society. Seeded on first play, read by
/// every mystery, written back to when one closes.
/// </summary>
public sealed class SocietyWorld
{
    public const int CurrentSchemaVersion = 1;

    [JsonProperty("schemaVersion")]
    public int SchemaVersion { get; set; } = CurrentSchemaVersion;

    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("settingId")]
    public string SettingId { get; set; } = "";

    [JsonProperty("seededAt")]
    public DateTime SeededAt { get; set; }

    [JsonProperty("updatedAt")]
    public DateTime UpdatedAt { get; set; }

    /// <summary>Incremented by every save; a save based on an older version is rejected.</summary>
    [JsonProperty("version")]
    public int Version { get; set; }

    [JsonProperty("characters")]
    public List<CharacterInstance> Characters { get; set; } = [];

    /// <summary>Consequences true only for this Society ("the sawmill at Forsby burned").</summary>
    [JsonProperty("overlay")]
    public List<WorldOverlayEntry> Overlay { get; set; } = [];

    /// <summary>Mysteries closed in this world, oldest first.</summary>
    [JsonProperty("closedMysteryIds")]
    public List<string> ClosedMysteryIds { get; set; } = [];

    /// <summary>
    /// The levels whose people this Society has met (fixes 4, section 12.1): written the first time a case uses the
    /// level, and drawn on by every later case there. The people themselves are in <see cref="Characters"/>.
    /// </summary>
    [JsonProperty("levelRosters")]
    public List<LevelRoster> LevelRosters { get; set; } = [];

    /// <summary>How the world was seeded (model, tokens), for review.</summary>
    [JsonProperty("seedLog")]
    public List<string> SeedLog { get; set; } = [];

    public CharacterInstance? Find(string? id) => Characters.Find(c => c.Id == id);

    public IEnumerable<CharacterInstance> Available => Characters.Where(c => c.Status is CharacterStatuses.Alive or CharacterStatuses.Absent);

    public LevelRoster? RosterFor(string? levelId) => LevelRosters.Find(r => r.LevelId == levelId);

    /// <summary>The people who live on a level: its roster and the residents earlier cases left there.</summary>
    public IEnumerable<CharacterInstance> PeopleOf(string levelId) => Characters.Where(c => c.Tier == WorldTiers.LocationResident && c.LivesOn(levelId));

    /// <summary>
    /// The names a new person in a case on this level must not echo: everyone but the people of other levels, whom the
    /// Society meets only there.
    /// </summary>
    public IEnumerable<CharacterInstance> NamesThatMatter(string levelId) =>
        Characters.Where(c => c.Tier != WorldTiers.LocationResident || c.LivesOn(levelId));
}

/// <summary>A level's people for one Society, from its first case there (fixes 4, section 12.1).</summary>
public sealed class LevelRoster
{
    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    /// <summary>The case the roster was written for. Deleting it unplayed discards the roster, if no other case used it.</summary>
    [JsonProperty("firstMysteryId")]
    public string FirstMysteryId { get; set; } = "";

    /// <summary>Every case on the level that drew on the roster.</summary>
    [JsonProperty("mysteryIds")]
    public List<string> MysteryIds { get; set; } = [];

    [JsonProperty("createdAt")]
    public DateTime CreatedAt { get; set; }

    /// <summary>The people's ids, as written (later cases may add residents of their own).</summary>
    [JsonProperty("characterIds")]
    public List<string> CharacterIds { get; set; } = [];

    /// <summary>The model call that wrote it, for review.</summary>
    [JsonProperty("log")]
    public string Log { get; set; } = "";
}

/// <summary>What a roster person is to their place.</summary>
public static class RosterRoles
{
    /// <summary>Keeps a business (inn, smithy, shop, station, mill).</summary>
    public const string Keeper = "keeper";

    /// <summary>Head of the household that owns a farm or manor.</summary>
    public const string Owner = "owner";

    /// <summary>Spouse, child or servant of an owner's or keeper's household.</summary>
    public const string Household = "household";

    public const string Clergy = "clergy";

    /// <summary>The constable, the sheriff's man, the station master as the crown's man.</summary>
    public const string Official = "official";

    public const string Teacher = "teacher";

    /// <summary>The wise woman or cunning man.</summary>
    public const string OldWays = "oldWays";

    /// <summary>Anyone else who lives here, with a latent hook.</summary>
    public const string Resident = "resident";

    public static readonly IReadOnlyList<string> All = [Keeper, Owner, Household, Clergy, Official, Teacher, OldWays, Resident];

    /// <summary>The roles that hold a place: no case may give the place someone else in that role.</summary>
    public static bool Holds(string role) => role is Keeper or Owner or Clergy or OldWays;
}

/// <summary>
/// One Society's copy of a character (spec 6.2, 6.4). The identity (personality, drives, voice, background) is fixed
/// once created, so the character stays recognisable; status, importance, attitude, ties and the mind change.
/// </summary>
public sealed class CharacterInstance
{
    /// <summary>The canonical id for canonical characters, otherwise "npc-{slug}".</summary>
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    /// <summary>The canonical <see cref="CharacterDefinition"/> this is a copy of; null for the Society's own people.</summary>
    [JsonProperty("definitionId")]
    public string? DefinitionId { get; set; }

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>One of <see cref="WorldTiers"/>.</summary>
    [OneOf(typeof(WorldTiers))]
    [JsonProperty("tier")]
    public string Tier { get; set; } = WorldTiers.Background;

    /// <summary>One of <see cref="CharacterStatuses"/>.</summary>
    [JsonProperty("status")]
    public string Status { get; set; } = CharacterStatuses.Alive;

    /// <summary>0 (a face in the crowd) to 3 (central to the Society's story). Rises and falls with the mysteries.</summary>
    [JsonProperty("importance")]
    public int Importance { get; set; } = 1;

    /// <summary>Where they live: a town or a level and place ("Uppsala", "level-forsby-october/mill").</summary>
    [JsonProperty("home")]
    public string Home { get; set; } = "";

    /// <summary>The level they live on, for a location resident; they stay with it (spec 9.4).</summary>
    [JsonProperty("homeLevelId")]
    public string? HomeLevelId { get; set; }

    /// <summary>Their place on that level.</summary>
    [JsonProperty("homePlaceId")]
    public string? HomePlaceId { get; set; }

    /// <summary>What they are to their place (<see cref="RosterRoles"/>): its keeper, its owner, of the household...</summary>
    [OneOf(typeof(RosterRoles))]
    [JsonProperty("rosterRole")]
    public string? RosterRole { get; set; }

    [JsonProperty("identity")]
    public CharacterIdentity Identity { get; set; } = new();

    [JsonProperty("personality")]
    public PersonalityProfile Personality { get; set; } = new();

    [JsonProperty("drives")]
    public List<Drive> Drives { get; set; } = [];

    /// <summary>What they fear in their ordinary life (a level's people; review 3, item 4).</summary>
    [JsonProperty("fears")]
    public List<string> Fears { get; set; } = [];

    [JsonProperty("voice")]
    public CharacterVoice Voice { get; set; } = new();

    [JsonProperty("background")]
    public string Background { get; set; } = "";

    [JsonProperty("latentHooks")]
    public List<LatentHook> LatentHooks { get; set; } = [];

    [JsonProperty("networkLinks")]
    public List<NetworkLink> NetworkLinks { get; set; } = [];

    /// <summary>How they regard the Society, -3 (hostile) to +3 (devoted).</summary>
    [JsonProperty("attitude")]
    public int Attitude { get; set; }

    [JsonProperty("attitudeReason")]
    public string AttitudeReason { get; set; } = "";

    /// <summary>Ties to other characters in this world.</summary>
    [JsonProperty("relationships")]
    public List<WorldRelationship> Relationships { get; set; } = [];

    /// <summary>What happened between them and the Society or others: the "remembers" record.</summary>
    [JsonProperty("memories")]
    public List<MemoryEntry> Memories { get; set; } = [];

    /// <summary>Facts they know from past mysteries, with how they learned them.</summary>
    [JsonProperty("knowledge")]
    public List<WorldKnowledge> Knowledge { get; set; } = [];

    /// <summary>What they think, possibly wrong.</summary>
    [JsonProperty("beliefs")]
    public List<string> Beliefs { get; set; } = [];

    /// <summary>The mysteries they appeared in.</summary>
    [JsonProperty("mysteryIds")]
    public List<string> MysteryIds { get; set; } = [];

    /// <summary>Whether they live on a level: by their level, or for older residents by the level their home names.</summary>
    public bool LivesOn(string levelId) =>
        HomeLevelId == levelId || (HomeLevelId is null && (Home == levelId || Home.StartsWith(levelId + "/", StringComparison.Ordinal)));
}

public static class WorldTiers
{
    /// <summary>Canonical: every Society has them.</summary>
    public const string Canonical = "canonical";

    /// <summary>Important to this Society from the start, with drives, ties and an attitude.</summary>
    public const string Core = "core";

    /// <summary>Named people with no story yet, each with latent hooks.</summary>
    public const string Background = "background";

    /// <summary>Ordinary residents of a mystery's location, met again if the Society returns.</summary>
    public const string LocationResident = "locationResident";

    public static readonly IReadOnlyList<string> All = [Canonical, Core, Background, LocationResident];
}

public static class CharacterStatuses
{
    public const string Alive = "alive";
    public const string Dead = "dead";
    public const string Departed = "departed";

    /// <summary>Away for now; may return.</summary>
    public const string Absent = "absent";

    public static readonly IReadOnlyList<string> All = [Alive, Dead, Departed, Absent];
}

public sealed class WorldRelationship
{
    [JsonProperty("characterId")]
    public string CharacterId { get; set; } = "";

    [JsonProperty("nature")]
    public string Nature { get; set; } = "";

    /// <summary>-3 (hatred) to +3 (love).</summary>
    [JsonProperty("sentiment")]
    public int Sentiment { get; set; }
}

/// <summary>One remembered line of what happened.</summary>
public sealed class MemoryEntry
{
    [JsonProperty("mysteryId")]
    public string? MysteryId { get; set; }

    [JsonProperty("when")]
    public string When { get; set; } = "";

    [JsonProperty("text")]
    public string Text { get; set; } = "";
}

/// <summary>A fact a character knows from a past mystery, and how.</summary>
public sealed class WorldKnowledge
{
    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("factId")]
    public string FactId { get; set; } = "";

    [JsonProperty("statement")]
    public string Statement { get; set; } = "";

    [JsonProperty("source")]
    public string Source { get; set; } = "";
}

public sealed class WorldOverlayEntry
{
    [JsonProperty("mysteryId")]
    public string? MysteryId { get; set; }

    /// <summary>The level's public fact this explains (pf1, pf2...), when a case explained one (fixes 4, section 12.2).</summary>
    [JsonProperty("publicFactId")]
    public string? PublicFactId { get; set; }

    /// <summary>Where it holds: a level id, or null for the wider world.</summary>
    [JsonProperty("levelId")]
    public string? LevelId { get; set; }

    [JsonProperty("text")]
    public string Text { get; set; } = "";

    [JsonProperty("at")]
    public DateTime At { get; set; }
}
