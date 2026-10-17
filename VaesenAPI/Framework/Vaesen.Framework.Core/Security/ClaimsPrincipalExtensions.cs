using System.Security.Claims;

namespace Vaesen.Framework.Core.Security;

public static class ClaimsPrincipalExtensions
{
    /// <summary>
    /// The token's subject: the "sub" claim. For Auth0 users this is their user_id (e.g. "auth0|65f..."),
    /// for dev logins "dev|{player}".
    /// </summary>
    public static string GetSubject(this ClaimsPrincipal user) =>
        user.FindFirstValue("sub") ?? throw new InvalidOperationException("Token has no 'sub' claim.");

    /// <summary>True for tokens issued to a person. Auth0 machine-to-machine tokens use the client-credentials grant.</summary>
    public static bool IsPlayer(this ClaimsPrincipal user) =>
        user.HasClaim(c => c.Type == "sub") && !string.Equals(user.FindFirstValue("gty"), "client-credentials", StringComparison.Ordinal);

    public static bool HasScope(this ClaimsPrincipal user, string scope) =>
        user.FindAll("scope").Any(c => c.Value.Split(' ').Contains(scope, StringComparer.Ordinal));
}
