using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.GameLift.Models;
using Vaesen.Framework.GameLift.Services;

namespace Vaesen.Framework.GameLift.Api;

/// <summary>
/// What dedicated game servers report about the characters in their worlds. GameServer scope (open in Development).
/// </summary>
[Authorize(Policy = Policies.GameServer)]
[ApiController]
[Route("api/sessions/server")]
public sealed class GameServerController : ControllerBase
{
    private readonly WorldSessionService _sessions;
    private readonly ICharacterService _characters;

    public GameServerController(WorldSessionService sessions, ICharacterService characters)
    {
        _sessions = sessions;
        _characters = characters;
    }

    /// <summary>A character joining the server's world: what the server needs to spawn it (its appearance).</summary>
    [HttpGet("characters/{characterId}")]
    public async Task<ActionResult<ResultBase<Character>>> GetCharacter([FromRoute] string characterId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false));

    /// <summary>
    /// A character left the server's world (logged out or lost the connection): records when, so Play can bring it
    /// back while the instance runs, and frees the character for the guild's other members.
    /// </summary>
    [HttpPost("characters/{characterId}/left")]
    public async Task<ActionResult<ResultBase>> CharacterLeft([FromRoute] string characterId, CancellationToken cancellationToken)
    {
        var found = await _sessions.CharacterLeftAsync(characterId, cancellationToken).ConfigureAwait(false);
        return found ? Ok(new ResultBase().Ok()) : NotFound(new ResultBase().Fail("Character not found.", ErrorCodes.NotFound));
    }

    /// <summary>The characters in the server's world right now; keeps their players' claims from timing out. Send every few minutes.</summary>
    [HttpPost("heartbeat")]
    public async Task<ActionResult<ResultBase>> Heartbeat([FromBody] ServerHeartbeatRequest? request, CancellationToken cancellationToken)
    {
        await _sessions.HeartbeatAsync(request?.CharacterIds ?? [], cancellationToken).ConfigureAwait(false);
        return Ok(new ResultBase().Ok());
    }
}
