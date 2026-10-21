using System.Security.Claims;
using System.Text;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Nexus.Table.Services;

/// <summary>The "Dev" configuration section: seed players for the development login.</summary>
public sealed class DevOptions
{
    public const string SectionName = "Dev";

    /// <summary>HMAC key for dev tokens. Development only; not a secret.</summary>
    public string SigningKey { get; set; } = "";

    public int TokenLifetimeHours { get; set; } = 12;

    public List<DevPlayer> Players { get; set; } = [];

    public SymmetricSecurityKey GetSigningKey() => new(Encoding.UTF8.GetBytes(SigningKey));
}

public sealed class DevPlayer
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    /// <summary>Shown in the login picker; whether they are host comes from Nexus:HostSubjects ("dev|{id}").</summary>
    [JsonProperty("isHost")]
    public bool IsHost { get; set; }
}

public sealed class DevLoginRequest
{
    [JsonProperty("playerId")]
    public string? PlayerId { get; set; }
}

public sealed class DevLoginResult
{
    [JsonProperty("accessToken")]
    public string AccessToken { get; set; } = "";

    [JsonProperty("expiresIn")]
    public int ExpiresIn { get; set; }

    [JsonProperty("subject")]
    public string Subject { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";
}

/// <summary>
/// Development-only login without Auth0: a signed token for a seed player, validated like an Auth0 token
/// (NexusServiceExtensions.AddNexusDevAuthentication), so every endpoint behaves as in production.
/// </summary>
public interface IDevLoginService
{
    IReadOnlyList<DevPlayer> GetPlayers();

    ResultBase<DevLoginResult> Login(DevLoginRequest request);
}

public sealed class DevLoginService : IDevLoginService
{
    public const string Issuer = "nexus-dev";

    private readonly IOptions<DevOptions> _options;
    private readonly IOptions<NexusOptions> _nexus;
    private readonly IOptions<Auth0Options> _auth0;
    private readonly TimeProvider _timeProvider;

    public DevLoginService(IOptions<DevOptions> options, IOptions<NexusOptions> nexus, IOptions<Auth0Options> auth0, TimeProvider timeProvider)
    {
        _options = options;
        _nexus = nexus;
        _auth0 = auth0;
        _timeProvider = timeProvider;
    }

    public IReadOnlyList<DevPlayer> GetPlayers() => _options.Value.Players
        .Select(p => new DevPlayer { Id = p.Id, Name = p.Name, IsHost = _nexus.Value.HostSubjects.Contains(Subject(p.Id)) })
        .ToList();

    public ResultBase<DevLoginResult> Login(DevLoginRequest request)
    {
        var ret = new ResultBase<DevLoginResult>();
        var player = _options.Value.Players.Find(p => string.Equals(p.Id, request.PlayerId, StringComparison.OrdinalIgnoreCase));
        if (player is null)
        {
            return ret.Fail($"Unknown dev player '{request.PlayerId}'.", ErrorCodes.NotFound);
        }

        var now = _timeProvider.GetUtcNow().UtcDateTime;
        var expires = now.AddHours(_options.Value.TokenLifetimeHours);
        var subject = Subject(player.Id);
        var token = new JsonWebTokenHandler().CreateToken(new SecurityTokenDescriptor
        {
            Issuer = Issuer,
            Audience = _auth0.Value.Audience,
            Subject = new ClaimsIdentity([new Claim("sub", subject), new Claim("name", player.Name)]),
            IssuedAt = now,
            NotBefore = now,
            Expires = expires,
            SigningCredentials = new SigningCredentials(_options.Value.GetSigningKey(), SecurityAlgorithms.HmacSha256),
        });

        return ret.Ok(new DevLoginResult
        {
            AccessToken = token,
            ExpiresIn = (int)(expires - now).TotalSeconds,
            Subject = subject,
            Name = player.Name,
        });
    }

    private static string Subject(string playerId) => $"dev|{playerId}";
}
