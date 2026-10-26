using System.Security.Cryptography;
using System.Text;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Options;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Nexus.Table.Services;
using Vaesen.Framework.Core.Api;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;

namespace Nexus.Table.Api;

/// <summary>Signed-in endpoints: the caller's account is created on first use.</summary>
[Authorize(Policy = Policies.Player)]
[ApiController]
public abstract class NexusControllerBase(IAccountService accounts) : ControllerBase
{
    protected IAccountService Accounts { get; } = accounts;

    public const string NotApproved = "Your account is waiting for the host to let you in.";

    /// <summary>
    /// Runs <paramref name="action"/> as the caller's account, or returns why there is none. An account the host has not
    /// approved gets nothing but its own account (<paramref name="evenIfWaiting"/>).
    /// </summary>
    protected async Task<ActionResult<ResultBase<T>>> AsCallerAsync<T>(Func<Account, Task<ResultBase<T>>> action, CancellationToken cancellationToken, bool evenIfWaiting = false)
    {
        var account = await Accounts.GetOrCreateAsync(Caller.From(User), cancellationToken).ConfigureAwait(false);
        if (!account.Success)
        {
            return this.ToActionResult(new ResultBase<T>().Fail(account.ErrorMessage, account.ErrorCode));
        }

        if (!account.Result!.IsApproved && !evenIfWaiting)
        {
            return this.ToActionResult(new ResultBase<T>().Fail(NotApproved, ErrorCodes.Forbidden));
        }

        return this.ToActionResult(await action(account.Result!).ConfigureAwait(false));
    }
}

/// <summary>The signed-in person.</summary>
[Route("api/me")]
public sealed class MeController(IAccountService accounts) : NexusControllerBase(accounts)
{
    /// <summary>
    /// The caller's account; the first call creates it. Approved, it is a member of the Society; otherwise isApproved is
    /// false and the web app shows that the host has to let them in.
    /// </summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<Account>>> Get(CancellationToken cancellationToken) =>
        AsCallerAsync(account => Task.FromResult(new ResultBase<Account>().Ok(account)), cancellationToken, evenIfWaiting: true);

    /// <summary>Stores the name and avatar from the sign-in (the web app sends them from the Discord profile).</summary>
    [HttpPut("profile")]
    public async Task<ActionResult<ResultBase<Account>>> UpdateProfile([FromBody] UpdateProfileRequest? request, CancellationToken cancellationToken) =>
        this.ToActionResult(await Accounts.UpdateProfileAsync(Caller.From(User), request ?? new UpdateProfileRequest(), cancellationToken).ConfigureAwait(false));
}

/// <summary>Host only: who may use the table. New sign-ins wait here until the host lets them in.</summary>
[Route("api/admin")]
public sealed class AdminController(IAccountService accounts, IAdminService admin) : NexusControllerBase(accounts)
{
    /// <summary>Everyone who has signed in: waiting for approval first, then the Society's members.</summary>
    [HttpGet("accounts")]
    public Task<ActionResult<ResultBase<IReadOnlyList<Account>>>> Accounts(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => admin.ListAccountsAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Lets an account in, or turns it away (it leaves the Society and the call).</summary>
    [HttpPut("accounts/{accountId}/approval")]
    public Task<ActionResult<ResultBase<Account>>> SetApproval([FromRoute] string accountId, [FromBody] ApprovalRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => admin.SetApprovalAsync(caller, accountId, request?.Approved ?? false, cancellationToken), cancellationToken);
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
        AsCallerAsync(caller => society.GetViewAsync(caller, cancellationToken), cancellationToken);

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
        AsCallerAsync(async caller =>
        {
            var list = await characters.ListAsync(cancellationToken).ConfigureAwait(false);
            return list.Success ? new ResultBase<IReadOnlyList<Character>>().Ok(list.Result!.Select(c => c.VisibleTo(caller)).ToList()) : list;
        }, cancellationToken);

    [HttpGet("{characterId}")]
    public Task<ActionResult<ResultBase<Character>>> Get([FromRoute] string characterId, CancellationToken cancellationToken) =>
        AsCallerAsync(async caller =>
        {
            var found = await characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false);
            return found.Success ? new ResultBase<Character>().Ok(found.Result!.VisibleTo(caller)) : found;
        }, cancellationToken);

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

/// <summary>
/// Sessions of play on the mystery at the table: starting and ending them, notes to the Gamemaster, and the log.
/// </summary>
[Route("api/sessions")]
public sealed class SessionController(IAccountService accounts, ISessionService sessions) : NexusControllerBase(accounts)
{
    /// <summary>The open session, or null. Players get it without the play state.</summary>
    [HttpGet("current")]
    public Task<ActionResult<ResultBase<PlaySession?>>> Current(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.GetCurrentAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Host only: the next session of the mystery at the table, resuming where the last one ended.</summary>
    [HttpPost("start")]
    public Task<ActionResult<ResultBase<PlaySession>>> Start(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.StartAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Host only.</summary>
    [HttpPost("{sessionId}/end")]
    public Task<ActionResult<ResultBase<PlaySession>>> End([FromRoute] string sessionId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.EndAsync(caller, sessionId, cancellationToken), cancellationToken);

    /// <summary>A note to the Gamemaster about what happened, logged with its time. The web app also hands it to the Gamemaster.</summary>
    [HttpPost("current/notes")]
    public Task<ActionResult<ResultBase<SessionEvent>>> AddNote([FromBody] NoteRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.AddNoteAsync(caller, request ?? new NoteRequest(), cancellationToken), cancellationToken);

    [HttpGet("{sessionId}/log")]
    public Task<ActionResult<ResultBase<IReadOnlyList<SessionEvent>>>> Log([FromRoute] string sessionId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.GetLogAsync(caller, sessionId, cancellationToken), cancellationToken);

    /// <summary>Host only: a mystery's sessions with their play state.</summary>
    [HttpGet("mystery/{mysteryId}")]
    public Task<ActionResult<ResultBase<IReadOnlyList<PlaySession>>>> ForMystery([FromRoute] string mysteryId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.ListAsync(caller, mysteryId, cancellationToken), cancellationToken);

    /// <summary>Host only: every session of the mystery (or one, with sessionId) as one Markdown play log.</summary>
    [HttpGet("mystery/{mysteryId}/export")]
    public Task<ActionResult<ResultBase<string>>> Export([FromRoute] string mysteryId, [FromQuery] string? sessionId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => sessions.ExportAsync(caller, mysteryId, sessionId, cancellationToken), cancellationToken);
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

/// <summary>The voice call: join tokens, and the host bringing in the Gamemaster.</summary>
[Route("api/call")]
public sealed class CallController(IAccountService accounts, ICallService call) : NexusControllerBase(accounts)
{
    /// <summary>A LiveKit token for the Society's room, as the caller (with their character's name).</summary>
    [HttpPost("join")]
    public Task<ActionResult<ResultBase<CallJoin>>> Join(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => call.JoinAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Host only: dispatches the Gamemaster agent into the room.</summary>
    [HttpPost("gamemaster")]
    public Task<ActionResult<ResultBase<bool>>> StartGamemaster(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => call.StartGamemasterAsync(caller, cancellationToken), cancellationToken);
}

/// <summary>
/// Host only: generating mysteries and reading their case files (spoilers). Players never get here; the case file is
/// the hidden GM file.
/// </summary>
[Route("api/mysteries")]
public sealed class MysteryController(IAccountService accounts, IMysteryService mysteries) : NexusControllerBase(accounts)
{
    /// <summary>Generated cases, newest first (titles only). With lab=true, the lab's cases instead of the Society's.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<List<CaseSummary>>>> List([FromQuery] bool lab, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.ListAsync(caller, lab, cancellationToken), cancellationToken);

    /// <summary>Writes several cases one after another for review, kept apart from the Society's history; poll the job.</summary>
    [HttpPost("lab")]
    public Task<ActionResult<ResultBase<MysteryJob>>> Lab([FromBody] LabRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.StartLabAsync(caller, request ?? new LabRequest(), cancellationToken), cancellationToken);

    /// <summary>Stops a lab run after the case it is writing.</summary>
    [HttpPost("jobs/{jobId}/stop")]
    public Task<ActionResult<ResultBase<MysteryJob>>> StopJob([FromRoute] string jobId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => Task.FromResult(mysteries.StopJob(caller, jobId)), cancellationToken);

    /// <summary>The chosen cases as one file to save: their reports (markdown) or the stored cases (json).</summary>
    [HttpPost("export")]
    public Task<ActionResult<ResultBase<MysteryExport>>> Export([FromBody] ExportRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.ExportAsync(caller, request ?? new ExportRequest(), cancellationToken), cancellationToken);

    /// <summary>Puts the case on the table: players see its title and hook, the Gamemaster gets the case file.</summary>
    [HttpPost("{mysteryId}/table")]
    public Task<ActionResult<ResultBase<Society>>> BringToTable([FromRoute] string mysteryId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.BringToTableAsync(caller, mysteryId, cancellationToken), cancellationToken);

    /// <summary>Takes the case off the table without closing it.</summary>
    [HttpDelete("table")]
    public Task<ActionResult<ResultBase<Society>>> ClearTable(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.ClearTableAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Starts generating a new case in the background; poll the job for progress.</summary>
    [HttpPost("generate")]
    public Task<ActionResult<ResultBase<MysteryJob>>> Generate(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.StartAsync(caller, cancellationToken), cancellationToken);

    /// <summary>The running generation, or the last one; null when there has been none since the API started.</summary>
    [HttpGet("jobs/current")]
    public Task<ActionResult<ResultBase<MysteryJob?>>> CurrentJob(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => Task.FromResult(mysteries.GetCurrentJob(caller)), cancellationToken);

    [HttpGet("jobs/{jobId}")]
    public Task<ActionResult<ResultBase<MysteryJob>>> Job([FromRoute] string jobId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => Task.FromResult(mysteries.GetJob(caller, jobId)), cancellationToken);

    /// <summary>The whole case file with its generation log.</summary>
    [HttpGet("{mysteryId}")]
    public Task<ActionResult<ResultBase<StoredCase>>> Get([FromRoute] string mysteryId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.GetAsync(caller, mysteryId, cancellationToken), cancellationToken);

    /// <summary>Records how the mystery ended and writes it back to the Society's world: attitudes, fates, memories, new people.</summary>
    [HttpPost("{mysteryId}/close")]
    public Task<ActionResult<ResultBase<StoredCase>>> Close([FromRoute] string mysteryId, [FromBody] CloseMysteryRequest? request, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.CloseAsync(caller, mysteryId, request ?? new CloseMysteryRequest(), cancellationToken), cancellationToken);

    /// <summary>The case as a readable Markdown report (the CaseLab report).</summary>
    [HttpGet("{mysteryId}/report")]
    public Task<ActionResult<ResultBase<string>>> Report([FromRoute] string mysteryId, CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.GetReportAsync(caller, mysteryId, cancellationToken), cancellationToken);
}

/// <summary>Host only: the Society's persistent world, seeded before its first mystery and written back to as mysteries close.</summary>
[Route("api/world")]
public sealed class WorldController(IAccountService accounts, IMysteryService mysteries) : NexusControllerBase(accounts)
{
    /// <summary>The world: every persistent character with attitude, memories and knowledge, and what has changed.</summary>
    [HttpGet]
    public Task<ActionResult<ResultBase<SocietyWorld>>> Get(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.GetWorldAsync(caller, cancellationToken), cancellationToken);

    /// <summary>Seeds the world now (a background job, polled like a generation); otherwise the first mystery does it.</summary>
    [HttpPost("seed")]
    public Task<ActionResult<ResultBase<MysteryJob>>> Seed(CancellationToken cancellationToken) =>
        AsCallerAsync(caller => mysteries.StartSeedAsync(caller, cancellationToken), cancellationToken);
}

/// <summary>
/// For the GM agent only (NexusTable/GmAgent), authorized by the shared LiveKit:AgentKey in the X-Agent-Key header.
/// Gives the Gamemaster what players can't see: every sheet with its dark secret.
/// </summary>
[AllowAnonymous]
[ApiController]
[Route("api/agent")]
public sealed class AgentController(ICallService call, IRollStore rolls, ISessionService sessions, IOptions<LiveKitOptions> options) : ControllerBase
{
    public const string KeyHeader = "X-Agent-Key";

    [HttpGet("table")]
    public async Task<ActionResult<ResultBase<GmTable>>> Table(CancellationToken cancellationToken)
    {
        if (!IsAgent())
        {
            return Unauthorized(new ResultBase<GmTable>().Fail("Unknown agent.", "unauthorized"));
        }

        return this.ToActionResult(await call.GetGmTableAsync(cancellationToken).ConfigureAwait(false));
    }

    /// <summary>A roll as the server threw it, so the Gamemaster never relies on what a browser claims.</summary>
    [HttpGet("rolls/{rollId}")]
    public async Task<ActionResult<ResultBase<Roll>>> GetRoll([FromRoute] string rollId, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<Roll>();
        if (!IsAgent())
        {
            return Unauthorized(ret.Fail("Unknown agent.", "unauthorized"));
        }

        var roll = Roll.IsValidId<Roll>(rollId) ? await rolls.GetAsync(rollId, cancellationToken).ConfigureAwait(false) : null;
        return this.ToActionResult(roll is null ? ret.Fail("Roll not found.", ErrorCodes.NotFound) : ret.Ok(roll));
    }

    /// <summary>What was said to and by the Gamemaster, and notes it was dictated, for the open session's log.</summary>
    [HttpPost("events")]
    public Task<ActionResult<ResultBase<int>>> Events([FromBody] AgentEventsRequest? request, CancellationToken cancellationToken) =>
        AsAgentAsync(() => sessions.AppendFromAgentAsync(request ?? new AgentEventsRequest(), cancellationToken));

    /// <summary>A logged event (a note the host typed), read back so the Gamemaster never relies on what a browser claims.</summary>
    [HttpGet("events/{eventId}")]
    public Task<ActionResult<ResultBase<SessionEvent>>> Event([FromRoute] string eventId, CancellationToken cancellationToken) =>
        AsAgentAsync(async () => await sessions.GetEventAsync(eventId, cancellationToken).ConfigureAwait(false) is { } e
            ? new ResultBase<SessionEvent>().Ok(e)
            : new ResultBase<SessionEvent>().Fail("Event not found.", ErrorCodes.NotFound));

    /// <summary>The scribe's new play state for the session.</summary>
    [HttpPut("sessions/{sessionId}/state")]
    public Task<ActionResult<ResultBase<PlaySession>>> State([FromRoute] string sessionId, [FromBody] PlayState? state, CancellationToken cancellationToken) =>
        AsAgentAsync(() => sessions.UpdateStateAsync(sessionId, state ?? new PlayState(), cancellationToken));

    /// <summary>A test the Gamemaster asks for: shown at the table with the dice the character has and the sixes it takes.</summary>
    [HttpPost("tests")]
    public Task<ActionResult<ResultBase<TestRequest>>> Test([FromBody] TestRequestInput? input, CancellationToken cancellationToken) =>
        AsAgentAsync(() => sessions.RequestTestAsync(input ?? new TestRequestInput(), cancellationToken));

    private async Task<ActionResult<ResultBase<T>>> AsAgentAsync<T>(Func<Task<ResultBase<T>>> action) =>
        IsAgent() ? this.ToActionResult(await action().ConfigureAwait(false)) : Unauthorized(new ResultBase<T>().Fail("Unknown agent.", "unauthorized"));

    private bool IsAgent()
    {
        var expected = options.Value.AgentKey;
        var given = Request.Headers[KeyHeader].ToString();
        return expected.Length > 0
            && CryptographicOperations.FixedTimeEquals(Encoding.UTF8.GetBytes(given), Encoding.UTF8.GetBytes(expected));
    }
}
