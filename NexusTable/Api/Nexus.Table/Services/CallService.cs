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
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Services;

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

    /// <summary>The case at the table, whole: what the Gamemaster runs. Null between mysteries.</summary>
    [JsonProperty("mystery", NullValueHandling = NullValueHandling.Ignore)]
    public GmMystery? Mystery { get; set; }

    /// <summary>The open session, with the play state it resumes from. Null when none is open.</summary>
    [JsonProperty("session", NullValueHandling = NullValueHandling.Ignore)]
    public PlaySession? Session { get; set; }

    /// <summary>The open session's latest log lines, oldest first: what the Gamemaster rejoins mid-session with.</summary>
    [JsonProperty("recentLog")]
    public List<string> RecentLog { get; set; } = [];

    /// <summary>
    /// The session clock as this was read, in seconds since the session started: the agent runs it on from here with
    /// its own monotonic clock, so its times are the API's whatever the agent machine's clock says.
    /// </summary>
    [JsonProperty("sessionTime", NullValueHandling = NullValueHandling.Ignore)]
    public double? SessionTime { get; set; }
}

public sealed class GmMystery
{
    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("title")]
    public string Title { get; set; } = "";

    /// <summary>The hook as the Society receives it: how the Gamemaster opens the mystery.</summary>
    [JsonProperty("hookText")]
    public string HookText { get; set; } = "";

    /// <summary>The case file as Markdown (the CaseLab report without the generation log): truth, cast, places, clues.</summary>
    [JsonProperty("caseFile")]
    public string CaseFile { get; set; } = "";

    /// <summary>The names of the case's people, for speech to text.</summary>
    [JsonProperty("castNames")]
    public List<string> CastNames { get; set; } = [];
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

    /// <summary>Takes someone out of the call (the host turned their account away). False if LiveKit refused.</summary>
    Task<bool> RemoveAsync(string accountId, CancellationToken cancellationToken = default);
}

public sealed class CallService : ICallService
{
    public const string HttpClientName = "livekit";
    public const string NotConfigured = "call_not_configured";

    /// <summary>How many of the open session's log lines the Gamemaster rejoins with.</summary>
    public const int RecentLogLines = 40;

    /// <summary>People on the call at once, the host among them; the Gamemaster is the extra seat.</summary>
    public const int MaxPlayers = 5;

    public const string TableFull = "table_full";

    private readonly IOptions<LiveKitOptions> _options;
    private readonly IOptions<NexusOptions> _nexus;
    private readonly ISocietyService _society;
    private readonly ICharacterStore _characters;
    private readonly IAccountStore _accounts;
    private readonly ICasePacketStore _cases;
    private readonly ISessionService _sessions;
    private readonly IHttpClientFactory _http;
    private readonly TimeProvider _timeProvider;
    private readonly ILogger<CallService> _logger;

    public CallService(
        IOptions<LiveKitOptions> options,
        IOptions<NexusOptions> nexus,
        ISocietyService society,
        ICharacterStore characters,
        IAccountStore accounts,
        ICasePacketStore cases,
        ISessionService sessions,
        IHttpClientFactory http,
        TimeProvider timeProvider,
        ILogger<CallService> logger)
    {
        _options = options;
        _nexus = nexus;
        _society = society;
        _characters = characters;
        _accounts = accounts;
        _cases = cases;
        _sessions = sessions;
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

        // Five at the table: someone already on the call (another tab, a reconnect) always gets back in.
        var onCall = await PlayersOnCallAsync(cancellationToken).ConfigureAwait(false);
        if (onCall.Count >= MaxPlayers && !onCall.Contains(caller.Id))
        {
            return ret.Fail($"The table is full: {MaxPlayers} people are on the call. Try again when someone leaves.", TableFull);
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

        // With a case at the table, the Gamemaster plays a session of it: the next one, unless one is open.
        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (society.Result?.AtTable is not null)
        {
            var session = await _sessions.EnsureOpenAsync(caller.Id, cancellationToken).ConfigureAwait(false);
            if (!session.Success)
            {
                return ret.Fail(session.ErrorMessage, session.ErrorCode);
            }
        }

        using var response = await TwirpAsync("livekit.AgentDispatchService/CreateDispatch", new
        {
            agent_name = options.AgentName,
            room = RoomName,
            metadata = JsonConvert.SerializeObject(new { societyId = _nexus.Value.SocietyId, startedBy = caller.Id }),
        }, cancellationToken).ConfigureAwait(false);
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

        GmMystery? mystery = null;
        if (society.Result.AtTable is { } atTable)
        {
            var stored = await _cases.GetAsync(society.Result.Id, atTable.MysteryId, cancellationToken).ConfigureAwait(false);
            if (stored.Success)
            {
                mystery = new GmMystery
                {
                    MysteryId = atTable.MysteryId,
                    Title = stored.Result!.Packet.Title,
                    HookText = stored.Result.Packet.Hook.Text,
                    CaseFile = CaseReport.ToMarkdown(stored.Result.Packet),
                    CastNames = stored.Result.Packet.Cast.Select(c => c.Name).Where(n => n.Length > 0).Distinct().ToList(),
                };
            }
            else
            {
                _logger.LogWarning("The case at the table, {MysteryId}, could not be read: {Error}", atTable.MysteryId, stored.ErrorMessage);
            }
        }

        var open = mystery is null ? null : await _sessions.GetOpenAsync(cancellationToken).ConfigureAwait(false);
        var recent = open is null ? [] : await _sessions.RecentAsync(open.Id, RecentLogLines, cancellationToken).ConfigureAwait(false);
        return ret.Ok(new GmTable
        {
            Society = society.Result,
            Mystery = mystery,
            Session = open,
            SessionTime = open is null ? null : (_timeProvider.GetUtcNow().UtcDateTime - open.StartedAt).TotalSeconds,
            RecentLog = recent.Where(e => !e.HostOnly).Select(e => $"[{SessionClock.Stamp(e.SessionTime)}] {SessionService.Line(e)}").ToList(),
            Players = accounts.Select(a => new GmPlayer
            {
                AccountId = a.Id,
                DisplayName = a.DisplayName,
                Role = a.Role,
                Character = a.ActiveCharacterId is not null && characters.TryGetValue(a.ActiveCharacterId, out var c) ? c : null,
            }).ToList(),
        });
    }

    public async Task<bool> RemoveAsync(string accountId, CancellationToken cancellationToken = default)
    {
        if (!_options.Value.IsConfigured)
        {
            return true;
        }

        try
        {
            using var response = await TwirpAsync("livekit.RoomService/RemoveParticipant", new { room = RoomName, identity = accountId }, cancellationToken).ConfigureAwait(false);
            // Not found: they were not on the call.
            return response.IsSuccessStatusCode || response.StatusCode == System.Net.HttpStatusCode.NotFound;
        }
        catch (HttpRequestException error)
        {
            _logger.LogWarning(error, "Could not take {AccountId} out of the call", accountId);
            return false;
        }
    }

    /// <summary>
    /// The accounts on the call now, by their identity (the account id); the Gamemaster and other agents are not
    /// counted. Empty when LiveKit can't say (no room yet, or not reachable), so a hiccup never locks the table.
    /// </summary>
    private async Task<HashSet<string>> PlayersOnCallAsync(CancellationToken cancellationToken)
    {
        var players = new HashSet<string>(StringComparer.Ordinal);
        try
        {
            using var response = await TwirpAsync("livekit.RoomService/ListParticipants", new { room = RoomName }, cancellationToken).ConfigureAwait(false);
            if (!response.IsSuccessStatusCode)
            {
                return players;
            }

            var body = JsonConvert.DeserializeObject<Newtonsoft.Json.Linq.JObject>(await response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false));
            foreach (var participant in body?["participants"] ?? new Newtonsoft.Json.Linq.JArray())
            {
                var identity = participant.Value<string>("identity") ?? "";
                if (Account.IsValidId<Account>(identity))
                {
                    players.Add(identity);
                }
            }
        }
        catch (Exception error) when (error is HttpRequestException or JsonException)
        {
            _logger.LogWarning(error, "Could not read who is on the call");
        }

        return players;
    }

    /// <summary>LiveKit's server API is Twirp: JSON over POST, authorized by a token with roomAdmin on the room.</summary>
    private async Task<HttpResponseMessage> TwirpAsync(string method, object body, CancellationToken cancellationToken)
    {
        var adminToken = CreateToken("nexus-api", "Nexus Table API", null, new Dictionary<string, object>
        {
            ["room"] = RoomName,
            ["roomAdmin"] = true,
            ["roomCreate"] = true,
        });
        using var request = new HttpRequestMessage(HttpMethod.Post, $"{_options.Value.HttpUrl}/twirp/{method}")
        {
            Content = new StringContent(JsonConvert.SerializeObject(body), Encoding.UTF8, "application/json"),
        };
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", adminToken);
        return await _http.CreateClient(HttpClientName).SendAsync(request, cancellationToken).ConfigureAwait(false);
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
