using Newtonsoft.Json;

namespace Vaesen.Framework.GameLift.Models;

/// <summary>A world players can travel to (GET api/sessions/worlds).</summary>
public sealed class WorldSummary
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";
}

/// <summary>
/// Result of POST api/sessions/worlds/{worldId}/join.
/// "ready": connect to <see cref="ConnectUrl"/>. "starting": the world's server is starting; call again after <see cref="RetryAfterSeconds"/>.
/// </summary>
public sealed class JoinWorldResponse
{
    public const string Ready = "ready";
    public const string Starting = "starting";

    [JsonProperty("status")]
    public string Status { get; set; } = "";

    [JsonProperty("worldId")]
    public string WorldId { get; set; } = "";

    [JsonProperty("map")]
    public string Map { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";

    /// <summary>The instance entered (its game session name), e.g. "gyllencreutz-guild-{guid}".</summary>
    [JsonProperty("instance", NullValueHandling = NullValueHandling.Ignore)]
    public string? Instance { get; set; }

    [JsonProperty("ipAddress")]
    public string? IpAddress { get; set; }

    [JsonProperty("dnsName")]
    public string? DnsName { get; set; }

    [JsonProperty("port")]
    public int? Port { get; set; }

    [JsonProperty("playerSessionId")]
    public string? PlayerSessionId { get; set; }

    /// <summary>Pass straight to ClientTravel/open: "{host}:{port}?PlayerSessionId={id}". The server validates the id with GameLift.</summary>
    [JsonProperty("connectUrl")]
    public string? ConnectUrl { get; set; }

    [JsonProperty("retryAfterSeconds")]
    public int? RetryAfterSeconds { get; set; }
}

/// <summary>POST api/sessions/enter: the character to play (the caller must hold its claim, i.e. pressed Play).</summary>
public sealed class EnterWorldRequest
{
    [JsonProperty("characterId")]
    public string? CharacterId { get; set; }
}

/// <summary>POST api/sessions/server/heartbeat: the characters in a game server's world right now.</summary>
public sealed class ServerHeartbeatRequest
{
    [JsonProperty("characterIds")]
    public List<string> CharacterIds { get; set; } = [];
}
