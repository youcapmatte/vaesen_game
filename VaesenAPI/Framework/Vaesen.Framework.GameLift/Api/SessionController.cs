using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.GameLift.Models;
using Vaesen.Framework.GameLift.Services;

namespace Vaesen.Framework.GameLift.Api;

/// <summary>Worlds and getting into them via Amazon GameLift.</summary>
[Authorize(Policy = Policies.Player)]
[ApiController]
[Route("api/sessions")]
public sealed class SessionController : ControllerBase
{
    private const int StartingRetryAfterSeconds = 2;

    /// <summary>Subject prefix of dev login tokens: "dev|{accountId}".</summary>
    private const string DevSubjectPrefix = "dev|";

    private readonly WorldSessionService _sessions;
    private readonly IAccountStore _accounts;

    public SessionController(WorldSessionService sessions, IAccountStore accounts)
    {
        _sessions = sessions;
        _accounts = accounts;
    }

    [HttpGet("worlds")]
    public ActionResult<ResultBase<IReadOnlyList<WorldSummary>>> Worlds()
    {
        IReadOnlyList<WorldSummary> worlds = _sessions.Worlds
            .Select(w => new WorldSummary { Id = w.Id, DisplayName = w.DisplayName })
            .ToList();
        return Ok(new ResultBase<IReadOnlyList<WorldSummary>>().Ok(worlds));
    }

    /// <summary>
    /// Gets the player into a shared world: 200 "ready" with a connect URL, 202 "starting" (retry after Retry-After
    /// seconds), 409 world full, 403 no active subscription, 404 unknown world.
    /// </summary>
    [HttpPost("worlds/{worldId}/join")]
    public async Task<ActionResult<ResultBase<JoinWorldResponse>>> Join([FromRoute] string worldId, CancellationToken cancellationToken)
    {
        var result = await _sessions.JoinAsync(worldId, User.GetSubject(), cancellationToken).ConfigureAwait(false);
        return ToResponse(result, $"Unknown world '{worldId}'.");
    }

    /// <summary>
    /// Puts the character the player just claimed (Play) into the world: back into the instance it just left if that's
    /// still running, else its guild's hub. Same answers as join; 403 if the caller doesn't hold the character's claim.
    /// </summary>
    [HttpPost("enter")]
    public async Task<ActionResult<ResultBase<JoinWorldResponse>>> Enter([FromBody] EnterWorldRequest? request, CancellationToken cancellationToken)
    {
        var accountId = await GetAccountIdAsync(cancellationToken).ConfigureAwait(false);
        if (accountId is null)
        {
            return StatusCode(StatusCodes.Status403Forbidden, new ResultBase<JoinWorldResponse>().Fail("No account for this sign-in.", ErrorCodes.Forbidden));
        }

        // The dev login token for a character carries it; an explicit one in the body wins.
        var characterId = request?.CharacterId ?? User.FindFirst("character")?.Value ?? "";
        var result = await _sessions.EnterWorldAsync(accountId, characterId, cancellationToken).ConfigureAwait(false);
        return ToResponse(result, "Character not found.");
    }

    /// <summary>The caller's account: from a dev login token, or the account linked to the Auth0 user.</summary>
    private async Task<string?> GetAccountIdAsync(CancellationToken cancellationToken)
    {
        var subject = User.GetSubject();
        if (subject.StartsWith(DevSubjectPrefix, StringComparison.Ordinal))
        {
            return subject[DevSubjectPrefix.Length..];
        }

        return (await _accounts.GetByAuth0UserIdAsync(subject, cancellationToken).ConfigureAwait(false))?.Id;
    }

    private ActionResult<ResultBase<JoinWorldResponse>> ToResponse(JoinWorldResult result, string notFoundMessage)
    {
        var ret = new ResultBase<JoinWorldResponse>();
        switch (result.Status)
        {
            case JoinWorldStatus.Ready:
                var world = result.World!;
                var session = result.PlayerSession!;
                var host = string.IsNullOrEmpty(session.DnsName) ? session.IpAddress : session.DnsName;
                return Ok(ret.Ok(new JoinWorldResponse
                {
                    Status = JoinWorldResponse.Ready,
                    WorldId = world.Id,
                    DisplayName = world.DisplayName,
                    Map = world.Map,
                    Instance = result.SessionName,
                    IpAddress = session.IpAddress,
                    DnsName = session.DnsName,
                    Port = session.Port,
                    PlayerSessionId = session.PlayerSessionId,
                    ConnectUrl = $"{host}:{session.Port}?PlayerSessionId={session.PlayerSessionId}",
                }));

            case JoinWorldStatus.Starting:
                // The instance's server process is still activating; the client calls again.
                Response.Headers.RetryAfter = StartingRetryAfterSeconds.ToString(System.Globalization.CultureInfo.InvariantCulture);
                return Accepted(ret.Ok(new JoinWorldResponse
                {
                    Status = JoinWorldResponse.Starting,
                    WorldId = result.World!.Id,
                    DisplayName = result.World.DisplayName,
                    Map = result.World.Map,
                    RetryAfterSeconds = StartingRetryAfterSeconds,
                }));

            case JoinWorldStatus.Full:
                return Conflict(ret.Fail("World is full.", "world_full"));

            case JoinWorldStatus.NoSubscription:
                return StatusCode(StatusCodes.Status403Forbidden, ret.Fail("An active subscription is required.", "subscription_required"));

            case JoinWorldStatus.NotYourCharacter:
                return StatusCode(StatusCodes.Status403Forbidden, ret.Fail("You aren't playing this character. Press Play on it first.", ErrorCodes.Forbidden));

            case JoinWorldStatus.NoHub:
                return StatusCode(StatusCodes.Status500InternalServerError, ret.Fail("No guild hub world is configured (Sessions:Worlds, Kind \"GuildHub\").", "no_hub"));

            default:
                return NotFound(ret.Fail(notFoundMessage, ErrorCodes.NotFound));
        }
    }
}
