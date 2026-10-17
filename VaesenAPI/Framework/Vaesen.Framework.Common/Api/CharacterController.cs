using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Api;

/// <summary>
/// Characters: read, change and delete. Characters are created in their guild: POST api/guilds/{guildId}/characters.
/// Back-office endpoints (admin scope), open in Development.
/// </summary>
[Authorize(Policy = Policies.Admin)]
[ApiController]
[Route("api/characters")]
public sealed class CharacterController : ControllerBase
{
    private readonly ICharacterService _characters;

    public CharacterController(ICharacterService characters)
    {
        _characters = characters;
    }

    /// <summary>The classes a character can have (for character creation).</summary>
    [AllowAnonymous]
    [HttpGet("classes")]
    public ActionResult<ResultBase<IReadOnlyList<string>>> Classes() =>
        Ok(new ResultBase<IReadOnlyList<string>>().Ok(_characters.GetClasses()));

    [HttpGet("{characterId}")]
    public async Task<ActionResult<ResultBase<Character>>> Get([FromRoute] string characterId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false));

    [HttpGet("by-name/{name}")]
    public async Task<ActionResult<ResultBase<Character>>> GetByName([FromRoute] string name, CancellationToken cancellationToken) =>
        this.ToActionResult(await _characters.GetByNameAsync(name, cancellationToken).ConfigureAwait(false));

    /// <summary>
    /// Changes first/last name (which renames the character to "{firstName} {lastName}"), class or health.
    /// Fields left out stay as they are; "" clears the class. First and last name can't be cleared.
    /// </summary>
    [HttpPatch("{characterId}")]
    public async Task<ActionResult<ResultBase<Character>>> Update(
        [FromRoute] string characterId, [FromBody] UpdateCharacterRequest? request, CancellationToken cancellationToken) =>
        this.ToActionResult(await _characters.UpdateAsync(characterId, request ?? new UpdateCharacterRequest(), cancellationToken).ConfigureAwait(false));

    /// <summary>Deletes the character: it leaves its guild, whoever played it stops, and the name can be used again.</summary>
    [HttpDelete("{characterId}")]
    public async Task<ActionResult<ResultBase<Character>>> Delete([FromRoute] string characterId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _characters.DeleteAsync(characterId, cancellationToken).ConfigureAwait(false));
}
