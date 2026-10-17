using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.GameLift.Models;
using Vaesen.Framework.GameLift.Services;

namespace Vaesen.Framework.GameLift.Api;

/// <summary>Worlds and joining them via Amazon GameLift.</summary>
[Authorize(Policy = Policies.Player)]
[ApiController]
[Route("api/sessions")]
public sealed class SessionController : ControllerBase
{
    private const int StartingRetryAfterSeconds = 2;

    private readonly WorldSessionService _sessions;

    public SessionController(WorldSessionService sessions)
    {
        _sessions = sessions;
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
    /// Gets the player into a world: 200 "ready" with a connect URL, 202 "starting" (retry after Retry-After seconds),
    /// 409 world full, 403 no active subscription, 404 unknown world.
    /// </summary>
    [HttpPost("worlds/{worldId}/join")]
    public async Task<ActionResult<ResultBase<JoinWorldResponse>>> Join([FromRoute] string worldId, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<JoinWorldResponse>();
        var result = await _sessions.JoinAsync(worldId, User.GetSubject(), cancellationToken).ConfigureAwait(false);

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
                    Map = world.Map,
                    IpAddress = session.IpAddress,
                    DnsName = session.DnsName,
                    Port = session.Port,
                    PlayerSessionId = session.PlayerSessionId,
                    ConnectUrl = $"{host}:{session.Port}?PlayerSessionId={session.PlayerSessionId}",
                }));

            case JoinWorldStatus.Starting:
                // The world's server process is still activating; the client polls this endpoint again.
                Response.Headers.RetryAfter = StartingRetryAfterSeconds.ToString(System.Globalization.CultureInfo.InvariantCulture);
                return Accepted(ret.Ok(new JoinWorldResponse
                {
                    Status = JoinWorldResponse.Starting,
                    WorldId = result.World!.Id,
                    Map = result.World.Map,
                    RetryAfterSeconds = StartingRetryAfterSeconds,
                }));

            case JoinWorldStatus.Full:
                return Conflict(ret.Fail("World is full.", "world_full"));

            case JoinWorldStatus.NoSubscription:
                return StatusCode(StatusCodes.Status403Forbidden, ret.Fail("An active subscription is required.", "subscription_required"));

            default:
                return NotFound(ret.Fail($"Unknown world '{worldId}'.", ErrorCodes.NotFound));
        }
    }
}
