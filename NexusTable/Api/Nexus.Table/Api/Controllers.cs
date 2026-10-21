using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Nexus.Table.Api;

/// <summary>Signed-in endpoints: the caller's account is created on first use.</summary>
[Authorize(Policy = Policies.Player)]
[ApiController]
public abstract class NexusControllerBase(IAccountService accounts) : ControllerBase
{
    protected IAccountService Accounts { get; } = accounts;

    /// <summary>Runs <paramref name="action"/> as the caller's account, or returns why there is none.</summary>
    protected async Task<ActionResult<ResultBase<T>>> AsCallerAsync<T>(Func<Account, Task<ResultBase<T>>> action, CancellationToken cancellationToken)
    {
        var account = await Accounts.GetOrCreateAsync(Caller.From(User), cancellationToken).ConfigureAwait(false);
        if (!account.Success)
        {
            return this.ToActionResult(new ResultBase<T>().Fail(account.ErrorMessage, account.ErrorCode));
        }

        return this.ToActionResult(await action(account.Result!).ConfigureAwait(false));
    }
}

/// <summary>The signed-in person.</summary>
[Route("api/me")]
public sealed class MeController(IAccountService accounts) : NexusControllerBase(accounts)
{
    /// <summary>The caller's account; the first call creates it and joins the Society.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<Account>>> Get(CancellationToken cancellationToken) =>
        AsCallerAsync(account => Task.FromResult(new ResultBase<Account>().Ok(account)), cancellationToken);

    /// <summary>Stores the name and avatar from the sign-in (the web app sends them from the Discord profile).</summary>
    [HttpPut("profile")]
    public async Task<ActionResult<ResultBase<Account>>> UpdateProfile([FromBody] UpdateProfileRequest? request, CancellationToken cancellationToken) =>
        this.ToActionResult(await Accounts.UpdateProfileAsync(Caller.From(User), request ?? new UpdateProfileRequest(), cancellationToken).ConfigureAwait(false));
}

/// <summary>The rules the character wizard is built from. Public: they are the same for everyone.</summary>
[AllowAnonymous]
[ApiController]
[Route("api/rules")]
public sealed class RulesController(IRulesService rules) : ControllerBase
{
    [HttpGet]
    public ActionResult<ResultBase<VaesenRules>> Get() => Ok(new ResultBase<VaesenRules>().Ok(rules.Rules));

    /// <summary>A d66 roll on the memento table.</summary>
    [HttpPost("memento")]
    public ActionResult<ResultBase<RuleMemento>> RollMemento() => Ok(new ResultBase<RuleMemento>().Ok(rules.RollMemento()));
}

[Route("api/society")]
public sealed class SocietyController(IAccountService accounts, ISocietyService society) : NexusControllerBase(accounts)
{
    /// <summary>The Society with its members and the characters they play.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<SocietyView>>> Get(CancellationToken cancellationToken) =>
        AsCallerAsync(_ => society.GetViewAsync(cancellationToken), cancellationToken);

    /// <summary>Host only.</summary>
    [HttpPatch]
    public Task<ActionResult<ResultBase<Society>>> Update([FromBody] UpdateSocietyRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => society.UpdateAsync(caller, request ?? new UpdateSocietyRequest(), cancellationToken), cancellationToken);
}

[Route("api/characters")]
public sealed class CharacterController(IAccountService accounts, ICharacterService characters) : NexusControllerBase(accounts)
{
    /// <summary>Every character in the Society.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<IReadOnlyList<Character>>>> List(CancellationToken cancellationToken) =>
        AsCallerAsync(_ => characters.ListAsync(cancellationToken), cancellationToken);

    [HttpGet("{characterId}")]
    public Task<ActionResult<ResultBase<Character>>> Get([FromRoute] string characterId, CancellationToken cancellationToken) =>
        AsCallerAsync(_ => characters.GetAsync(characterId, cancellationToken), cancellationToken);

    /// <summary>Creates the caller's character from the twelve creation steps; it becomes the one they play.</summary>
    [HttpPost]
    public Task<ActionResult<ResultBase<Character>>> Create([FromBody] CreateCharacterRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => characters.CreateAsync(caller, request ?? new CreateCharacterRequest(), cancellationToken), cancellationToken);

    [HttpPatch("{characterId}")]
    public Task<ActionResult<ResultBase<Character>>> Update([FromRoute] string characterId, [FromBody] UpdateCharacterRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => characters.UpdateAsync(caller, characterId, request ?? new UpdateCharacterRequest(), cancellationToken), cancellationToken);

    [HttpPut("{characterId}/conditions")]
    public Task<ActionResult<ResultBase<Character>>> SetCondition([FromRoute] string characterId, [FromBody] SetConditionRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => characters.SetConditionAsync(caller, characterId, request ?? new SetConditionRequest(), cancellationToken), cancellationToken);
}

/// <summary>Skill tests, rolled on the server and shown to the whole table.</summary>
[Route("api/rolls")]
public sealed class RollController(IAccountService accounts, IDiceService dice) : NexusControllerBase(accounts)
{
    /// <summary>The Society's latest rolls, newest first.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<IReadOnlyList<Roll>>>> Recent(CancellationToken cancellationToken) =>
        AsCallerAsync(_ => dice.RecentAsync(cancellationToken), cancellationToken);

    [HttpPost]
    public Task<ActionResult<ResultBase<RollOutcome>>> Roll([FromBody] RollRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => dice.RollAsync(caller, request ?? new RollRequest(), cancellationToken), cancellationToken);

    /// <summary>Pushes a failed roll at the cost of a Condition.</summary>
    [HttpPost("{rollId}/push")]
    public Task<ActionResult<ResultBase<RollOutcome>>> Push([FromRoute] string rollId, [FromBody] PushRollRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => dice.PushAsync(caller, rollId, request ?? new PushRollRequest(), cancellationToken), cancellationToken);
}

/// <summary>Development-only login with seed players. Answers 404 in every other environment.</summary>
[DevelopmentOnly]
[AllowAnonymous]
[ApiController]
[Route("api/dev")]
public sealed class DevController(IDevLoginService devLogin) : ControllerBase
{
    [HttpGet("players")]
    public ActionResult<ResultBase<IReadOnlyList<DevPlayer>>> Players() =>
        Ok(new ResultBase<IReadOnlyList<DevPlayer>>().Ok(devLogin.GetPlayers()));

    [HttpPost("login")]
    public ActionResult<ResultBase<DevLoginResult>> Login([FromBody] DevLoginRequest? request) =>
        this.ToActionResult(devLogin.Login(request ?? new DevLoginRequest()));
}
