namespace Vaesen.Framework.GameLift.Models;

public sealed class SessionsOptions
{
    public const string SectionName = "Sessions";

    /// <summary>
    /// "GameLift" (real fleet, incl. Anywhere) or "Local" (no AWS: each world is a server you run yourself on 127.0.0.1:LocalPort).
    /// </summary>
    public string Provider { get; set; } = SessionProviders.GameLift;

    public GameLiftOptions GameLift { get; set; } = new();

    /// <summary>
    /// The worlds (maps) players can be in. A <see cref="WorldKinds.Shared"/> world is one game session everyone joins;
    /// a <see cref="WorldKinds.GuildHub"/> world gets its own game session per guild.
    /// </summary>
    public List<WorldDefinition> Worlds { get; set; } = [];

    /// <summary>
    /// How long after leaving a world (logging out, losing the connection) a character goes back to it on Play, as long
    /// as that world's session is still running. After that, or if it has closed, the character starts in its guild's hub.
    /// </summary>
    public TimeSpan ReturnWindow { get; set; } = TimeSpan.FromMinutes(10);
}

public sealed class GameLiftOptions
{
    /// <summary>Fleet to place world sessions on. Use either FleetId or AliasId (alias preferred in production).</summary>
    public string? FleetId { get; set; }

    public string? AliasId { get; set; }

    /// <summary>Fleet location, e.g. "custom-vaesen-dev" for a GameLift Anywhere fleet. Empty means the fleet's home region.</summary>
    public string? Location { get; set; }
}

public sealed class WorldDefinition
{
    /// <summary>Stable id, e.g. "gyllencreutz". Part of the GameLift game session name (see <see cref="GetSessionName"/>).</summary>
    public string Id { get; set; } = "";

    public string DisplayName { get; set; } = "";

    /// <summary><see cref="WorldKinds.Shared"/> (default) or <see cref="WorldKinds.GuildHub"/>.</summary>
    public string Kind { get; set; } = WorldKinds.Shared;

    /// <summary>Unreal map the server loads for this world, e.g. "/Game/Vaesen/Maps/L_Gyllencreutz".</summary>
    public string Map { get; set; } = "";

    public int MaxPlayers { get; set; } = 50;

    /// <summary>Port of this world's server when Provider is "Local" (see Scripts/RunLocalServer.bat).</summary>
    public int LocalPort { get; set; } = 7777;

    public bool IsGuildHub => string.Equals(Kind, WorldKinds.GuildHub, StringComparison.OrdinalIgnoreCase);

    /// <summary>
    /// The GameLift game session name of this world's instance: the world id for a shared world, "{id}-{guildId}" for a
    /// guild's hub. Whether that session is running is what decides if a character can go back to it.
    /// </summary>
    public string GetSessionName(string? guildId) => IsGuildHub ? $"{Id}-{guildId}" : Id;

    /// <summary>True when <paramref name="sessionName"/> is an instance of this world.</summary>
    public bool OwnsSession(string sessionName) =>
        string.Equals(sessionName, Id, StringComparison.Ordinal)
        || (IsGuildHub && sessionName.StartsWith(Id + "-", StringComparison.Ordinal));
}

public static class WorldKinds
{
    /// <summary>One game session for everyone (e.g. a town).</summary>
    public const string Shared = "Shared";

    /// <summary>One game session per guild: its base (Gyllencreutz).</summary>
    public const string GuildHub = "GuildHub";
}

public static class SessionProviders
{
    public const string GameLift = "GameLift";
    public const string Local = "Local";
}
