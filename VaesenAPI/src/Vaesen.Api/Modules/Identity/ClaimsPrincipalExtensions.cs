using System.Security.Claims;

namespace Vaesen.Api.Modules.Identity;

public static class ClaimsPrincipalExtensions
{
    /// <summary>
    /// The player's stable id: the Auth0 "sub" claim (e.g. "auth0|65f..."). Also used as the GameLift PlayerId.
    /// </summary>
    public static string GetPlayerId(this ClaimsPrincipal user) =>
        user.FindFirstValue("sub") ?? throw new InvalidOperationException("Token has no 'sub' claim.");

    /// <summary>
    /// True for tokens issued to a person. Auth0 machine-to-machine tokens use the client-credentials grant.
    /// </summary>
    public static bool IsPlayer(this ClaimsPrincipal user) =>
        user.HasClaim(c => c.Type == "sub") && user.FindFirstValue("gty") != "client-credentials";

    public static bool HasScope(this ClaimsPrincipal user, string scope) =>
        user.FindAll("scope").Any(c => c.Value.Split(' ').Contains(scope));
}
