using Newtonsoft.Json;

namespace Vaesen.Framework.Common.Models;

public sealed class CreateAccountRequest
{
    [JsonProperty("username")]
    public string? Username { get; set; }

    [JsonProperty("email")]
    public string? Email { get; set; }

    /// <summary>Auth0's user_id, e.g. "auth0|65f1c2...". Optional until Auth0 is in place.</summary>
    [JsonProperty("auth0UserId")]
    public string? Auth0UserId { get; set; }
}
