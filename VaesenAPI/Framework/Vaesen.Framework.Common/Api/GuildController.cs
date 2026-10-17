using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Api;

/// <summary>
/// Guilds: members, the guild's characters and who plays which character.
/// Back-office endpoints (admin scope), open in Development; the acting account is passed in the request until
/// players call with their own Auth0 tokens.
/// </summary>
[Authorize(Policy = Policies.Admin)]
[ApiController]
[Route("api/guilds")]
public sealed class GuildController : ControllerBase
{
    private readonly IGuildService _guilds;

    public GuildController(IGuildService guilds)
    {
        _guilds = guilds;
    }

    /// <summary>Creates a guild; the founding account becomes its first member.</summary>
    [HttpPost]
    public async Task<ActionResult<ResultBase<Guild>>> Create([FromBody] CreateGuildRequest? request, CancellationToken cancellationToken)
    {
        var result = await _guilds.CreateAsync(request ?? new CreateGuildRequest(), cancellationToken).ConfigureAwait(false);
        return result.Success
            ? CreatedAtAction(nameof(Get), new { guildId = result.Result!.Id }, result)
            : this.ToActionResult(result);
    }

    [HttpGet("{guildId}")]
    public async Task<ActionResult<ResultBase<Guild>>> Get([FromRoute] string guildId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.GetAsync(guildId, cancellationToken).ConfigureAwait(false));

    [HttpGet("by-name/{name}")]
    public async Task<ActionResult<ResultBase<Guild>>> GetByName([FromRoute] string name, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.GetByNameAsync(name, cancellationToken).ConfigureAwait(false));

    /// <summary>The guilds an account is a member of.</summary>
    [HttpGet("by-account/{accountId}")]
    public async Task<ActionResult<ResultBase<IReadOnlyList<Guild>>>> ListForAccount([FromRoute] string accountId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.ListForAccountAsync(accountId, cancellationToken).ConfigureAwait(false));

    [HttpPost("{guildId}/members")]
    public async Task<ActionResult<ResultBase<Guild>>> AddMember([FromRoute] string guildId, [FromBody] GuildMemberRequest? request, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.AddMemberAsync(guildId, request?.AccountId ?? "", cancellationToken).ConfigureAwait(false));

    /// <summary>Removes a member; the character they were playing becomes free.</summary>
    [HttpDelete("{guildId}/members/{accountId}")]
    public async Task<ActionResult<ResultBase<Guild>>> RemoveMember([FromRoute] string guildId, [FromRoute] string accountId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.RemoveMemberAsync(guildId, accountId, cancellationToken).ConfigureAwait(false));

    [HttpGet("{guildId}/characters")]
    public async Task<ActionResult<ResultBase<IReadOnlyList<Character>>>> ListCharacters([FromRoute] string guildId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.ListCharactersAsync(guildId, cancellationToken).ConfigureAwait(false));

    /// <summary>Creates a character owned by the guild. The creator must be a member.</summary>
    [HttpPost("{guildId}/characters")]
    public async Task<ActionResult<ResultBase<Character>>> CreateCharacter([FromRoute] string guildId, [FromBody] CreateCharacterRequest? request, CancellationToken cancellationToken)
    {
        var result = await _guilds.CreateCharacterAsync(guildId, request ?? new CreateCharacterRequest(), cancellationToken).ConfigureAwait(false);
        return result.Success ? StatusCode(StatusCodes.Status201Created, result) : this.ToActionResult(result);
    }

    /// <summary>
    /// Makes a member the player of a character. The previous player loses it, and the member stops playing any
    /// other character in this guild.
    /// </summary>
    [HttpPut("{guildId}/characters/{characterId}/player")]
    public async Task<ActionResult<ResultBase<Guild>>> AssignPlayer(
        [FromRoute] string guildId, [FromRoute] string characterId, [FromBody] AssignPlayerRequest? request, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.AssignPlayerAsync(guildId, characterId, request ?? new AssignPlayerRequest(), cancellationToken).ConfigureAwait(false));

    /// <summary>Frees a character: nobody plays it.</summary>
    [HttpDelete("{guildId}/characters/{characterId}/player")]
    public async Task<ActionResult<ResultBase<Guild>>> ReleasePlayer([FromRoute] string guildId, [FromRoute] string characterId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _guilds.ReleasePlayerAsync(guildId, characterId, cancellationToken).ConfigureAwait(false));
}
