namespace Vaesen.Framework.Core.Security;

/// <summary>The "Auth0" configuration section. Tokens are issued by Auth0; the API only validates them.</summary>
public sealed class Auth0Options
{
    public const string SectionName = "Auth0";

    /// <summary>Scope granted to the game-server machine-to-machine application in Auth0.</summary>
    public const string GameServerScope = "game:server";

    /// <summary>Scope for back-office tools (website admin pages, support scripts).</summary>
    public const string AdminScope = "vaesen:admin";

    /// <summary>Auth0 tenant domain, e.g. "vaesen.eu.auth0.com".</summary>
    public string Domain { get; set; } = "";

    /// <summary>Identifier of the API registered in Auth0, e.g. "https://api.vaesen.game".</summary>
    public string Audience { get; set; } = "";
}

/// <summary>Authorization policy names, used as <c>[Authorize(Policy = Policies.Player)]</c>.</summary>
public static class Policies
{
    /// <summary>A human player signed in (game client or website).</summary>
    public const string Player = "Player";

    /// <summary>A dedicated game server using its machine-to-machine credentials.</summary>
    public const string GameServer = "GameServer";

    /// <summary>Back-office access to any account's data. Open in Development.</summary>
    public const string Admin = "Admin";
}
