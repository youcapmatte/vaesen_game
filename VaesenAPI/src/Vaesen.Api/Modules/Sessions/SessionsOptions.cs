namespace Vaesen.Api.Modules.Sessions;

public sealed class SessionsOptions
{
    public const string SectionName = "Sessions";

    /// <summary>
    /// "GameLift" (real fleet, incl. Anywhere) or "Local" (no AWS: each world is a server you run yourself on 127.0.0.1:LocalPort).
    /// </summary>
    public string Provider { get; set; } = SessionProviders.GameLift;

    public GameLiftOptions GameLift { get; set; } = new();

    /// <summary>The persistent worlds players can travel between. Each world runs as one long-lived GameLift game session.</summary>
    public List<WorldDefinition> Worlds { get; set; } = [];
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
    /// <summary>Stable id, also used as the GameLift game session name, e.g. "town".</summary>
    public string Id { get; set; } = "";

    public string DisplayName { get; set; } = "";

    /// <summary>Unreal map the server loads for this world, e.g. "/Game/Maps/Town".</summary>
    public string Map { get; set; } = "";

    public int MaxPlayers { get; set; } = 50;

    /// <summary>Port of this world's server when Provider is "Local" (see Scripts/RunLocalServer.bat).</summary>
    public int LocalPort { get; set; } = 7777;
}

public static class SessionProviders
{
    public const string GameLift = "GameLift";
    public const string Local = "Local";
}
