using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Api;

/// <summary>
/// Player accounts: username, Auth0 link and subscription.
/// Back-office endpoints (admin scope); open in Development, where the game's fake login looks accounts up by username.
/// </summary>
[Authorize(Policy = Policies.Admin)]
[ApiController]
[Route("api/accounts")]
public sealed class AccountController : ControllerBase
{
    private readonly IAccountService _accounts;

    public AccountController(IAccountService accounts)
    {
        _accounts = accounts;
    }

    [HttpPost]
    public async Task<ActionResult<ResultBase<Account>>> Create([FromBody] CreateAccountRequest? request, CancellationToken cancellationToken)
    {
        var result = await _accounts.CreateAsync(request ?? new CreateAccountRequest(), cancellationToken).ConfigureAwait(false);
        return result.Success
            ? CreatedAtAction(nameof(Get), new { accountId = result.Result!.Id }, result)
            : this.ToActionResult(result);
    }

    [HttpGet("{accountId}")]
    public async Task<ActionResult<ResultBase<Account>>> Get([FromRoute] string accountId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _accounts.GetAsync(accountId, cancellationToken).ConfigureAwait(false));

    [HttpGet("by-username/{username}")]
    public async Task<ActionResult<ResultBase<Account>>> GetByUsername([FromRoute] string username, CancellationToken cancellationToken) =>
        this.ToActionResult(await _accounts.GetByUsernameAsync(username, cancellationToken).ConfigureAwait(false));

    [HttpGet("by-auth0/{auth0UserId}")]
    public async Task<ActionResult<ResultBase<Account>>> GetByAuth0UserId([FromRoute] string auth0UserId, CancellationToken cancellationToken) =>
        this.ToActionResult(await _accounts.GetByAuth0UserIdAsync(auth0UserId, cancellationToken).ConfigureAwait(false));
}
