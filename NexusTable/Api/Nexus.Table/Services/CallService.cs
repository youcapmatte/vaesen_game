using System.Net.Http.Headers;
using System.Security.Claims;
using System.Text;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

/// <summary>The "LiveKit" configuration section: the call room and the Gamemaster agent.</summary>
public sealed class LiveKitOptions
{
    public const string SectionName = "LiveKit";

    /// <summary>The LiveKit server, e.g. "wss://nexus-table-xxxx.livekit.cloud".</summary>
    public string Url { get; set; } = "";

    public string ApiKey { get; set; } = "";

    /// <summary>Signs join tokens. Server side only.</summary>
    public string ApiSecret { get; set; } = "";

    /// <summary>The agent_name the GM agent registers with (NexusTable/GmAgent).</summary>
    public string AgentName { get; set; } = "nexus-gm";

    public int TokenLifetimeHours { get; set; } = 6;

    /// <summary>Shared secret the GM agent sends in X-Agent-Key to read the table (GET /api/agent/table).</summary>
    public string AgentKey { get; set; } = "";

    public bool IsConfigured => Url.Length > 0 && ApiKey.Length > 0 && ApiSecret.Length > 0;

    /// <summary>The server API over HTTPS (wss:// becomes https://).</summary>
    public string HttpUrl => Url.Replace("wss://", "https://", StringComparison.Ordinal).Replace("ws://", "http://", StringComparison.Ordinal).TrimEnd('/');
}

/// <summary>What a browser needs to join the call.</summary>
public sealed class CallJoin
{
    [JsonProperty("url")]
    public string Url { get; set; } = "";

    [JsonProperty("token")]
    public string Token { get; set; } = "";

    [JsonProperty("room")]
    public string Room { get; set; } = "";

    [JsonProperty("identity")]
    public string Identity { get; set; } = "";
}

/// <summary>Who a participant is, in their LiveKit metadata. Set by the API, so the GM agent can trust it.</summary>
public sealed class CallParticipantMetadata
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";

    [JsonProperty("role")]
    [JsonConverter(typeof(StringEnumConverter))]
    public AccountRole Role { get; set; }

    [JsonProperty("characterId", NullValueHandling = NullValueHandling.Ignore)]
    public string? CharacterId { get; set; }

    [JsonProperty("characterName", NullValueHandling = NullValueHandling.Ignore)]
    public string? CharacterName { get; set; }
}

/// <summary>The table as the Gamemaster sees it: everyone's full sheet, dark secrets included.</summary>
public sealed class GmTable
{
    [JsonProperty("society")]
    public Society Society { get; set; } = new();

    [JsonProperty("players")]
    public List<GmPlayer> Players { get; set; } = [];
}

public sealed class GmPlayer
{
    [JsonProperty("accountId")]
    public string AccountId { get; set; } = "";

    [JsonProperty("displayName")]
    public string DisplayName { get; set; } = "";

    [JsonProperty("role")]
    public AccountRole Role { get; set; }

    [JsonProperty("character", NullValueHandling = NullValueHandling.Ignore)]
    public Character? Character { get; set; }
}

/// <summary>
/// The voice call (LiveKit): join tokens for the table, and bringing the Gamemaster agent into the room.
/// Everyone at the table hears everyone; only push-to-talk speech goes to the Gamemaster (see GmAgent).
/// </summary>
public interface ICallService
{
    /// <summary>The LiveKit room of the Society.</summary>
    string RoomName { get; }

    Task<ResultBase<CallJoin>> JoinAsync(Account caller, CancellationToken cancellationToken = default);

    /// <summary>Host only: dispatches the Gamemaster agent into the room.</summary>
    Task<ResultBase<bool>> StartGamemasterAsync(Account caller, CancellationToken cancellationToken = default);

    Task<ResultBase<GmTable>> GetGmTableAsync(CancellationToken cancellationToken = default);
}

public sealed class CallService : ICallService
{
    public const string HttpClientName = "livekit";
    public const string NotConfigured = "call_not_configured";

    private readonly IOptions<LiveKitOptions> _options;
    private readonly IOptions<NexusOptions> _nexus;
    private readonly ISocietyService _society;
    private readonly ICharacterStore _characters;
    private readonly IAccountStore _accounts;
    private readonly IHttpClientFactory _http;
    private readonly TimeProvider _timeProvider;
    private readonly ILogger<CallService> _logger;

    public CallService(
        IOptions<LiveKitOptions> options,
        IOptions<NexusOptions> nexus,
        ISocietyService society,
        ICharacterStore characters,
        IAccountStore accounts,
        IHttpClientFactory http,
        TimeProvider timeProvider,
        ILogger<CallService> logger)
    {
        _options = options;
        _nexus = nexus;
        _society = society;
        _characters = characters;
        _accounts = accounts;
        _http = http;
        _timeProvider = timeProvider;
        _logger = logger;
    }

    public string RoomName => $"nexus-{_nexus.Value.SocietyId}";

    public async Task<ResultBase<CallJoin>> JoinAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<CallJoin>();
        var options = _options.Value;
        if (!options.IsConfigured)
        {
            return ret.Fail("The call isn't set up yet: LiveKit:Url, ApiKey and ApiSecret are missing.", NotConfigured);
        }

        var character = caller.ActiveCharacterId is null
            ? null
            : await _characters.GetAsync(caller.ActiveCharacterId, cancellationToken).ConfigureAwait(false);
        var metadata = new CallParticipantMetadata
        {
            AccountId = caller.Id,
            DisplayName = caller.DisplayName,
            Role = caller.Role,
            CharacterId = character?.Id,
            CharacterName = character?.Name,
        };

        var grants = new Dictionary<string, object>
        {
            ["room"] = RoomName,
            ["roomJoin"] = true,
            ["canPublish"] = true,
            ["canSubscribe"] = true,
            ["canPublishData"] = true,
        };
        var token = CreateToken(caller.Id, character?.Name ?? caller.DisplayName, JsonConvert.SerializeObject(metadata), grants);
        return ret.Ok(new CallJoin { Url = options.Url, Token = token, Room = RoomName, Identity = caller.Id });
    }

    public async Task<ResultBase<bool>> StartGamemasterAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<bool>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can bring in the Gamemaster.", ErrorCodes.Forbidden);
        }

        var options = _options.Value;
        if (!options.IsConfigured)
        {
            return ret.Fail("The call isn't set up yet: LiveKit:Url, ApiKey and ApiSecret are missing.", NotConfigured);
        }

        // LiveKit's server API is Twirp: JSON over POST, authorized by a token with roomAdmin on the room.
        var adminToken = CreateToken("nexus-api", "Nexus Table API", null, new Dictionary<string, object>
        {
            ["room"] = RoomName,
            ["roomAdmin"] = true,
            ["roomCreate"] = true,
        });
        using var request = new HttpRequestMessage(HttpMethod.Post, $"{options.HttpUrl}/twirp/livekit.AgentDispatchService/CreateDispatch")
        {
            Content = new StringContent(
                JsonConvert.SerializeObject(new
                {
                    agent_name = options.AgentName,
                    room = RoomName,
                    metadata = JsonConvert.SerializeObject(new { societyId = _nexus.Value.SocietyId, startedBy = caller.Id }),
                }),
                Encoding.UTF8,
                "application/json"),
        };
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", adminToken);

        using var response = await _http.CreateClient(HttpClientName).SendAsync(request, cancellationToken).ConfigureAwait(false);
        if (!response.IsSuccessStatusCode)
        {
            var body = await response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false);
            _logger.LogWarning("LiveKit refused the Gamemaster dispatch: {Status} {Body}", (int)response.StatusCode, body);
            return ret.Fail("LiveKit didn't accept the Gamemaster. Check the LiveKit settings and that the GM agent is running.", "dispatch_failed");
        }

        return ret.Ok(true);
    }

    public async Task<ResultBase<GmTable>> GetGmTableAsync(CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<GmTable>();
        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        var accounts = await _accounts.GetManyAsync(society.Result!.MemberAccountIds, cancellationToken).ConfigureAwait(false);
        var characters = (await _characters.ListBySocietyAsync(society.Result.Id, cancellationToken).ConfigureAwait(false))
            .ToDictionary(c => c.Id, StringComparer.Ordinal);
        return ret.Ok(new GmTable
        {
            Society = society.Result,
            Players = accounts.Select(a => new GmPlayer
            {
                AccountId = a.Id,
                DisplayName = a.DisplayName,
                Role = a.Role,
                Character = a.ActiveCharacterId is not null && characters.TryGetValue(a.ActiveCharacterId, out var c) ? c : null,
            }).ToList(),
        });
    }

    /// <summary>A LiveKit access token: HS256 with the API secret, the API key as issuer, grants under "video".</summary>
    private string CreateToken(string identity, string name, string? metadata, Dictionary<string, object> grants)
    {
        var options = _options.Value;
        var now = _timeProvider.GetUtcNow().UtcDateTime;
        var claims = new Dictionary<string, object>
        {
            ["name"] = name,
            ["video"] = grants,
        };
        if (metadata is not null)
        {
            claims["metadata"] = metadata;
        }

        return new JsonWebTokenHandler().CreateToken(new SecurityTokenDescriptor
        {
            Issuer = options.ApiKey,
            Subject = new ClaimsIdentity([new Claim("sub", identity)]),
            Claims = claims,
            NotBefore = now,
            IssuedAt = now,
            Expires = now.AddHours(options.TokenLifetimeHours),
            SigningCredentials = new SigningCredentials(new SymmetricSecurityKey(Encoding.UTF8.GetBytes(options.ApiSecret)), SecurityAlgorithms.HmacSha256),
        });
    }
}
