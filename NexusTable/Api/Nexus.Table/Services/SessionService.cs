using System.Globalization;
using System.Text;
using Microsoft.Extensions.Logging;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

/// <summary>
/// Sessions of play on the mystery at the table, and their log. A mystery may take several sessions: each starts
/// from the play state the last one ended with. Everything said to and by the Gamemaster, every roll, every test it
/// asks for and every note goes into the log, timestamped, so a session can be followed up (and given to Claude) later.
/// </summary>
public interface ISessionService
{
    /// <summary>The open session of the mystery at the table, if any.</summary>
    Task<PlaySession?> GetOpenAsync(CancellationToken cancellationToken = default);

    /// <summary>The open session, as the caller may see it (players get no play state).</summary>
    Task<ResultBase<PlaySession?>> GetCurrentAsync(Account caller, CancellationToken cancellationToken = default);

    /// <summary>Host only: starts the next session of the mystery at the table, or returns the one already open.</summary>
    Task<ResultBase<PlaySession>> StartAsync(Account caller, CancellationToken cancellationToken = default);

    /// <summary>Starts a session for the mystery at the table if none is open (the Gamemaster being brought in).</summary>
    Task<ResultBase<PlaySession>> EnsureOpenAsync(string startedBy, CancellationToken cancellationToken = default);

    Task<ResultBase<PlaySession>> EndAsync(Account caller, string sessionId, CancellationToken cancellationToken = default);

    /// <summary>Ends whatever session is open (the case leaving the table, or being closed).</summary>
    Task EndOpenAsync(string endedBy, CancellationToken cancellationToken = default);

    /// <summary>A note to the Gamemaster about what happened, in the log with its time.</summary>
    Task<ResultBase<SessionEvent>> AddNoteAsync(Account caller, NoteRequest request, CancellationToken cancellationToken = default);

    /// <summary>Host only: a mystery's sessions, with their play state.</summary>
    Task<ResultBase<IReadOnlyList<PlaySession>>> ListAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default);

    /// <summary>A session's log. Players may read the open session's, without host-only lines.</summary>
    Task<ResultBase<IReadOnlyList<SessionEvent>>> GetLogAsync(Account caller, string sessionId, CancellationToken cancellationToken = default);

    /// <summary>Host only: every session of a mystery, or one, as one Markdown document: play state and log.</summary>
    Task<ResultBase<string>> ExportAsync(Account caller, string mysteryId, string? sessionId, CancellationToken cancellationToken = default);

    // The Gamemaster agent's side.
    Task<ResultBase<int>> AppendFromAgentAsync(AgentEventsRequest request, CancellationToken cancellationToken = default);

    Task<ResultBase<PlaySession>> UpdateStateAsync(string sessionId, PlayState state, CancellationToken cancellationToken = default);

    Task<ResultBase<TestRequest>> RequestTestAsync(TestRequestInput input, CancellationToken cancellationToken = default);

    Task<SessionEvent?> GetEventAsync(string eventId, CancellationToken cancellationToken = default);

    /// <summary>The last events of a session, oldest first.</summary>
    Task<IReadOnlyList<SessionEvent>> RecentAsync(string sessionId, int count, CancellationToken cancellationToken = default);

    // The dice's side.
    Task<TestRequest?> FindPendingTestAsync(string testId, CancellationToken cancellationToken = default);

    /// <summary>Logs a roll in the open session and takes the test it answers off the pending list.</summary>
    Task RollMadeAsync(Roll roll, CancellationToken cancellationToken = default);
}

public sealed class SessionService(
    ISessionStore sessions,
    ISessionEventStore events,
    ISocietyService society,
    ICharacterStore characters,
    IRulesService rules,
    ITableNotifier notifier,
    TimeProvider time,
    ILogger<SessionService> logger) : ISessionService
{
    public const int MaxNoteLength = 2000;
    public const int MaxEventLength = 4000;
    public const int MaxSuccessesNeeded = 5;

    /// <summary>A note may be about a moment this long before it is sent (the writer took their time).</summary>
    public const double MaxNoteSecondsAgo = 60 * 60;

    /// <summary>A note sent this long after the moment it is about says when it was sent, too.</summary>
    public const double NoteSentLateSeconds = 10;

    private DateTime Now => time.GetUtcNow().UtcDateTime;

    public async Task<PlaySession?> GetOpenAsync(CancellationToken cancellationToken = default)
    {
        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (current.Result?.AtTable is not { } atTable)
        {
            return null;
        }

        var list = await sessions.ListByMysteryAsync(atTable.MysteryId, cancellationToken).ConfigureAwait(false);
        return list.LastOrDefault(s => s.IsOpen);
    }

    public async Task<ResultBase<PlaySession?>> GetCurrentAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var open = await GetOpenAsync(cancellationToken).ConfigureAwait(false);
        return new ResultBase<PlaySession?>().Ok(open is null ? null : caller.Role == AccountRole.Host ? open : open.ForPlayers());
    }

    public Task<ResultBase<PlaySession>> StartAsync(Account caller, CancellationToken cancellationToken = default) =>
        caller.Role != AccountRole.Host
            ? Task.FromResult(new ResultBase<PlaySession>().Fail("Only the host can start a session.", ErrorCodes.Forbidden))
            : EnsureOpenAsync(caller.Id, cancellationToken);

    public async Task<ResultBase<PlaySession>> EnsureOpenAsync(string startedBy, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<PlaySession>();
        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!current.Success)
        {
            return ret.Fail(current.ErrorMessage, current.ErrorCode);
        }

        if (current.Result!.AtTable is not { } atTable)
        {
            return ret.Fail("Bring a case to the table first: a session is a sitting of one mystery.", ErrorCodes.Invalid);
        }

        var list = await sessions.ListByMysteryAsync(atTable.MysteryId, cancellationToken).ConfigureAwait(false);
        if (list.LastOrDefault(s => s.IsOpen) is { } open)
        {
            return ret.Ok(open);
        }

        // The next session carries on from where the last one left the mystery.
        var last = list.LastOrDefault();
        var session = new PlaySession
        {
            SocietyId = current.Result.Id,
            MysteryId = atTable.MysteryId,
            MysteryTitle = atTable.Title,
            Number = (last?.Number ?? 0) + 1,
            StartedAt = Now,
            State = last?.State ?? new PlayState(),
            CreatedBy = startedBy,
        };
        var created = await sessions.CreateAsync(session, cancellationToken).ConfigureAwait(false);
        if (!created.Success)
        {
            return created;
        }

        await LogAsync(created.Result!, SessionEventKinds.SessionStarted, $"Session {session.Number} of \"{session.MysteryTitle}\" begins.", null, "The table", cancellationToken).ConfigureAwait(false);
        await notifier.SessionChangedAsync(created.Result!, cancellationToken).ConfigureAwait(false);
        return created;
    }

    public async Task<ResultBase<PlaySession>> EndAsync(Account caller, string sessionId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<PlaySession>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can end a session.", ErrorCodes.Forbidden);
        }

        var session = await GetSessionAsync(sessionId, cancellationToken).ConfigureAwait(false);
        if (session is null)
        {
            return ret.Fail("Session not found.", ErrorCodes.NotFound);
        }

        return session.IsOpen ? await CloseAsync(session, caller.Id, cancellationToken).ConfigureAwait(false) : ret.Ok(session);
    }

    public async Task EndOpenAsync(string endedBy, CancellationToken cancellationToken = default)
    {
        if (await GetOpenAsync(cancellationToken).ConfigureAwait(false) is { } open)
        {
            await CloseAsync(open, endedBy, cancellationToken).ConfigureAwait(false);
        }
    }

    private async Task<ResultBase<PlaySession>> CloseAsync(PlaySession session, string endedBy, CancellationToken cancellationToken)
    {
        var ended = await ChangeAsync(session.Id, s =>
        {
            s.EndedAt = Now;
            s.PendingTests = [];
            s.UpdatedBy = endedBy;
        }, cancellationToken).ConfigureAwait(false);
        if (ended.Success)
        {
            await LogAsync(ended.Result!, SessionEventKinds.SessionEnded, $"Session {ended.Result!.Number} ends.", null, "The table", cancellationToken).ConfigureAwait(false);
            await notifier.SessionChangedAsync(ended.Result, cancellationToken).ConfigureAwait(false);
        }

        return ended;
    }

    public async Task<ResultBase<SessionEvent>> AddNoteAsync(Account caller, NoteRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SessionEvent>();
        var text = request.Text?.Trim() ?? "";
        if (text.Length == 0 || text.Length > MaxNoteLength)
        {
            return ret.Fail($"Write the note, at most {MaxNoteLength} characters.", ErrorCodes.Invalid);
        }

        var open = await GetOpenAsync(cancellationToken).ConfigureAwait(false);
        if (open is null)
        {
            return ret.Fail("No session is open: start one to keep notes in its log.", ErrorCodes.Invalid);
        }

        // Placed when the writer began it, on the session clock: that is the moment it is about.
        var now = Now;
        var ago = Math.Clamp(request.SecondsAgo ?? 0, 0, MaxNoteSecondsAgo);
        var at = open.TimeAt((now - open.StartedAt).TotalSeconds - ago, now);
        var sentAt = (now - at).TotalSeconds >= NoteSentLateSeconds ? now : (DateTime?)null;
        var who = await NameOfAsync(caller, cancellationToken).ConfigureAwait(false);
        return ret.Ok(await LogAsync(open, SessionEventKinds.Note, text, caller.Id, who, cancellationToken, at, sentAt: sentAt).ConfigureAwait(false));
    }

    public async Task<ResultBase<IReadOnlyList<PlaySession>>> ListAsync(Account caller, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<PlaySession>>();
        return caller.Role != AccountRole.Host
            ? ret.Fail("Only the host can read a mystery's sessions.", ErrorCodes.Forbidden)
            : ret.Ok(await sessions.ListByMysteryAsync(mysteryId, cancellationToken).ConfigureAwait(false));
    }

    public async Task<ResultBase<IReadOnlyList<SessionEvent>>> GetLogAsync(Account caller, string sessionId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<SessionEvent>>();
        var session = await GetSessionAsync(sessionId, cancellationToken).ConfigureAwait(false);
        if (session is null)
        {
            return ret.Fail("Session not found.", ErrorCodes.NotFound);
        }

        var host = caller.Role == AccountRole.Host;
        if (!host && !session.IsOpen)
        {
            return ret.Fail("Only the host can read the log of a past session.", ErrorCodes.Forbidden);
        }

        var log = await events.ListBySessionAsync(sessionId, cancellationToken).ConfigureAwait(false);
        return ret.Ok(host ? log : log.Where(e => !e.HostOnly).ToList());
    }

    public async Task<ResultBase<string>> ExportAsync(Account caller, string mysteryId, string? sessionId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<string>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can export a session log.", ErrorCodes.Forbidden);
        }

        var list = (await sessions.ListByMysteryAsync(mysteryId, cancellationToken).ConfigureAwait(false))
            .Where(s => sessionId is null || s.Id == sessionId)
            .ToList();
        if (list.Count == 0)
        {
            return ret.Fail("No sessions to export.", ErrorCodes.NotFound);
        }

        var text = new StringBuilder();
        var inv = CultureInfo.InvariantCulture;
        text.AppendLine(inv, $"# Play log: \"{list[0].MysteryTitle}\"");
        text.AppendLine();
        text.AppendLine(inv, $"Mystery `{mysteryId}`. {list.Count} session(s). Each line has its time on the session clock (hours:minutes:seconds since the session began), then the time in UTC. A note is placed at the moment it is about. The case file itself is exported from its case page; fact, channel, event and lever ids below are the case file's.");
        foreach (var session in list)
        {
            text.AppendLine();
            text.AppendLine(inv, $"## Session {session.Number}: {session.StartedAt:yyyy-MM-dd HH:mm}{(session.EndedAt is { } end ? $" to {end:HH:mm}" : ", still open")}");
            text.AppendLine();
            text.AppendLine("### Where the mystery stood at the end");
            AppendState(text, session.State);
            text.AppendLine();
            text.AppendLine("### The log");
            foreach (var e in await events.ListBySessionAsync(session.Id, cancellationToken).ConfigureAwait(false))
            {
                text.AppendLine(inv, $"- [{SessionClock.Stamp(e.SessionTime)}] {e.At:HH:mm:ss} {Line(e)}");
            }
        }

        return ret.Ok(text.ToString());
    }

    private static void AppendState(StringBuilder text, PlayState state)
    {
        var inv = CultureInfo.InvariantCulture;
        text.AppendLine(inv, $"- **Story so far:** {(state.Summary.Length > 0 ? state.Summary : "(nothing yet)")}");
        text.AppendLine(inv, $"- **Scene:** {(state.Scene.Length > 0 ? state.Scene : "(not set)")}");
        text.AppendLine(inv, $"- **Facts found:** {(state.FactsFound.Count > 0 ? string.Join("; ", state.FactsFound.Select(f => $"{f.FactId} (session {f.Session}): {f.How}")) : "none")}");
        text.AppendLine(inv, $"- **Channels used:** {Join(state.ChannelsUsed)}");
        text.AppendLine(inv, $"- **People met:** {(state.PeopleMet.Count > 0 ? string.Join("; ", state.PeopleMet.Select(p => $"{p.Name} ({p.CharacterId}): {p.Note}")) : "none")}");
        text.AppendLine(inv, $"- **Beats and events that happened:** {Join(state.BeatsFired)}");
        text.AppendLine(inv, $"- **Deeds done:** {Join(state.DeedsDone)}");
        text.AppendLine(inv, $"- **Open threads:** {(state.OpenThreads.Count > 0 ? string.Join("; ", state.OpenThreads) : "none")}");
    }

    private static string Join(List<string> ids) => ids.Count > 0 ? string.Join(", ", ids) : "none";

    /// <summary>One line of the log as a reader (or Claude) reads it.</summary>
    public static string Line(SessionEvent e) => e.Kind switch
    {
        SessionEventKinds.PlayerSpoke => $"**{e.Speaker}** to the Gamemaster: {e.Text}",
        SessionEventKinds.GmSaid => $"**The Gamemaster:** {e.Text}",
        SessionEventKinds.Note => $"**Note from {e.Speaker}:** {e.Text}{(e.SentAt is { } sent ? $" *(sent at {sent:HH:mm:ss})*" : "")}",
        SessionEventKinds.Roll => $"**Dice:** {e.Text}",
        SessionEventKinds.TestRequested => $"**Test asked for:** {e.Text}",
        SessionEventKinds.StateUpdated => $"*(play state updated: {e.Text})*",
        _ => $"*{e.Text}*",
    };

    public async Task<ResultBase<int>> AppendFromAgentAsync(AgentEventsRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<int>();
        var open = await GetOpenAsync(cancellationToken).ConfigureAwait(false);
        if (open is null)
        {
            // Nothing to log into; the agent keeps going.
            return ret.Ok(0);
        }

        var count = 0;
        foreach (var e in request.Events.Where(e => SessionEventKinds.FromAgent.Contains(e.Kind) && !string.IsNullOrWhiteSpace(e.Text)))
        {
            // The agent's session clock places it; a bare time is taken as it is.
            var at = e.SessionTime is { } clock ? open.TimeAt(clock, Now) : e.At;
            await LogAsync(open, e.Kind, Cut(e.Text.Trim(), MaxEventLength), e.AccountId, e.Speaker, cancellationToken, at).ConfigureAwait(false);
            count++;
        }

        return ret.Ok(count);
    }

    public async Task<ResultBase<PlaySession>> UpdateStateAsync(string sessionId, PlayState state, CancellationToken cancellationToken = default)
    {
        state.UpdatedAt = Now;
        var updated = await ChangeAsync(sessionId, s => s.State = state, cancellationToken).ConfigureAwait(false);
        if (updated.Success)
        {
            var what = $"{state.FactsFound.Count} facts found, {state.PeopleMet.Count} people met, {state.BeatsFired.Count} beats; scene: {Cut(state.Scene, 120)}";
            await LogAsync(updated.Result!, SessionEventKinds.StateUpdated, what, null, "The scribe", cancellationToken, hostOnly: true).ConfigureAwait(false);
        }

        return updated;
    }

    public async Task<ResultBase<TestRequest>> RequestTestAsync(TestRequestInput input, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<TestRequest>();
        var open = await GetOpenAsync(cancellationToken).ConfigureAwait(false);
        if (open is null)
        {
            return ret.Fail("No session is open.", ErrorCodes.Invalid);
        }

        var all = await characters.ListBySocietyAsync(open.SocietyId, cancellationToken).ConfigureAwait(false);
        var wanted = input.Character.Trim();
        var character = all.FirstOrDefault(c => c.Id == wanted)
            ?? all.FirstOrDefault(c => string.Equals(c.Name, wanted, StringComparison.OrdinalIgnoreCase))
            ?? all.FirstOrDefault(c => c.Name.Split(' ')[0].Equals(wanted.Split(' ')[0], StringComparison.OrdinalIgnoreCase));
        if (character is null)
        {
            return ret.Fail($"No investigator is called \"{input.Character}\".", ErrorCodes.NotFound);
        }

        var book = rules.Rules;
        var name = input.Skill.Trim();
        var skill = book.Skills.Find(s => string.Equals(s.Id, name, StringComparison.OrdinalIgnoreCase) || string.Equals(s.Name, name, StringComparison.OrdinalIgnoreCase));
        var attribute = skill?.Attribute
            ?? book.Attributes.Find(a => string.Equals(a.Id, name, StringComparison.OrdinalIgnoreCase) || string.Equals(a.Name, name, StringComparison.OrdinalIgnoreCase))?.Id;
        if (attribute is null)
        {
            return ret.Fail($"\"{input.Skill}\" is neither a skill nor an attribute.", ErrorCodes.Invalid);
        }

        var difficulty = Math.Clamp(input.Difficulty, -DiceService.MaxModifier, DiceService.MaxModifier);
        var pool = DiceService.BuildPool(character, attribute, skill, new RollRequest { Difficulty = difficulty }, book);
        var test = new TestRequest
        {
            Id = "test-" + Guid.NewGuid().ToString("N")[..12],
            CharacterId = character.Id,
            CharacterName = character.Name,
            Skill = skill?.Id,
            Attribute = attribute,
            Difficulty = difficulty,
            SuccessesNeeded = Math.Clamp(input.SuccessesNeeded, 1, MaxSuccessesNeeded),
            Reason = string.IsNullOrWhiteSpace(input.Reason) ? null : Cut(input.Reason.Trim(), 200),
            Pool = pool,
            PoolSize = Math.Max(1, pool.Sum(p => p.Dice)),
            At = Now,
        };

        // One test per character at a time: a new one replaces what they had not rolled yet.
        var updated = await ChangeAsync(open.Id, s =>
        {
            s.PendingTests.RemoveAll(t => t.CharacterId == character.Id);
            s.PendingTests.Add(test);
        }, cancellationToken).ConfigureAwait(false);
        if (!updated.Success)
        {
            return ret.Fail(updated.ErrorMessage, updated.ErrorCode);
        }

        await LogAsync(updated.Result!, SessionEventKinds.TestRequested, Describe(test, book), null, "The Gamemaster", cancellationToken, test: test).ConfigureAwait(false);
        await notifier.SessionChangedAsync(updated.Result!, cancellationToken).ConfigureAwait(false);
        return ret.Ok(test);
    }

    public static string Describe(TestRequest t, VaesenRules book)
    {
        var what = t.Skill is { } s ? book.FindSkill(s)?.Name ?? s : $"{book.Attributes.Find(a => a.Id == t.Attribute)?.Name ?? t.Attribute} (attribute test)";
        var difficulty = t.Difficulty == 0 ? "" : $" {(t.Difficulty > 0 ? "+" : "")}{t.Difficulty}";
        return $"{t.CharacterName}: {what}{difficulty}, {t.PoolSize} dice, {t.SuccessesNeeded} six{(t.SuccessesNeeded == 1 ? "" : "es")} needed{(t.Reason is null ? "" : $" ({t.Reason})")}.";
    }

    public async Task<SessionEvent?> GetEventAsync(string eventId, CancellationToken cancellationToken = default) =>
        SessionEvent.IsValidId<SessionEvent>(eventId) ? await events.GetAsync(eventId, cancellationToken).ConfigureAwait(false) : null;

    public async Task<IReadOnlyList<SessionEvent>> RecentAsync(string sessionId, int count, CancellationToken cancellationToken = default) =>
        (await events.ListBySessionAsync(sessionId, cancellationToken).ConfigureAwait(false)).TakeLast(count).ToList();

    public async Task<TestRequest?> FindPendingTestAsync(string testId, CancellationToken cancellationToken = default) =>
        (await GetOpenAsync(cancellationToken).ConfigureAwait(false))?.PendingTests.Find(t => t.Id == testId);

    public async Task RollMadeAsync(Roll roll, CancellationToken cancellationToken = default)
    {
        var open = await GetOpenAsync(cancellationToken).ConfigureAwait(false);
        if (open is null)
        {
            return;
        }

        var book = rules.Rules;
        var what = roll.Skill is { } s ? book.FindSkill(s)?.Name ?? s : $"{book.Attributes.Find(a => a.Id == roll.Attribute)?.Name ?? roll.Attribute} (attribute test)";
        var faces = string.Join(" ", roll.PushedDice ?? roll.Dice);
        var text = $"{roll.CharacterName} rolled {what}{(roll.Reason is null ? "" : $" ({roll.Reason})")}: {roll.PoolSize} dice [{faces}], "
            + $"{roll.Successes} of {Math.Max(1, roll.SuccessesNeeded)} six{(roll.SuccessesNeeded > 1 ? "es" : "")} needed, {(roll.Succeeded ? "success" : "failure")}"
            + (roll.Pushed ? $", pushed{(roll.PushCondition is null ? "" : $" at the cost of {roll.PushCondition}")}" : "") + ".";
        await LogAsync(open, SessionEventKinds.Roll, text, roll.AccountId, roll.CharacterName, cancellationToken, rollId: roll.Id).ConfigureAwait(false);

        if (roll.TestId is { } testId && open.PendingTests.Exists(t => t.Id == testId))
        {
            var updated = await ChangeAsync(open.Id, x => x.PendingTests.RemoveAll(t => t.Id == testId), cancellationToken).ConfigureAwait(false);
            if (updated.Success)
            {
                await notifier.SessionChangedAsync(updated.Result!, cancellationToken).ConfigureAwait(false);
            }
        }
    }

    private async Task<PlaySession?> GetSessionAsync(string sessionId, CancellationToken cancellationToken) =>
        PlaySession.IsValidId<PlaySession>(sessionId) ? await sessions.GetAsync(sessionId, cancellationToken).ConfigureAwait(false) : null;

    /// <summary>Reads, changes and saves a session, again on a version clash (the scribe and the dice may meet).</summary>
    private async Task<ResultBase<PlaySession>> ChangeAsync(string sessionId, Action<PlaySession> change, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<PlaySession>();
        for (var attempt = 0; ; attempt++)
        {
            var session = await GetSessionAsync(sessionId, cancellationToken).ConfigureAwait(false);
            if (session is null)
            {
                return ret.Fail("Session not found.", ErrorCodes.NotFound);
            }

            change(session);
            var saved = await sessions.UpdateAsync(session, cancellationToken).ConfigureAwait(false);
            if (saved.Success || saved.ErrorCode != ErrorCodes.Conflict || attempt >= 3)
            {
                return saved;
            }
        }
    }

    private async Task<SessionEvent> LogAsync(
        PlaySession session, string kind, string text, string? accountId, string speaker, CancellationToken cancellationToken,
        DateTime? at = null, bool hostOnly = false, string? rollId = null, TestRequest? test = null, DateTime? sentAt = null)
    {
        var when = at ?? Now;
        var e = new SessionEvent
        {
            SessionId = session.Id,
            MysteryId = session.MysteryId,
            SocietyId = session.SocietyId,
            At = when,
            SessionTime = session.ClockAt(when),
            SentAt = sentAt,
            Kind = kind,
            AccountId = accountId,
            Speaker = speaker,
            Text = text,
            HostOnly = hostOnly,
            RollId = rollId,
            Test = test,
            CreatedBy = accountId ?? "system",
        };
        var created = await events.CreateAsync(e, cancellationToken).ConfigureAwait(false);
        if (!created.Success)
        {
            logger.LogWarning("A session event was not logged: {Error}", created.ErrorMessage);
            return e;
        }

        await notifier.SessionEventAsync(created.Result!, cancellationToken).ConfigureAwait(false);
        return created.Result!;
    }

    private async Task<string> NameOfAsync(Account caller, CancellationToken cancellationToken)
    {
        var character = caller.ActiveCharacterId is null ? null : await characters.GetAsync(caller.ActiveCharacterId, cancellationToken).ConfigureAwait(false);
        return caller.Role == AccountRole.Host ? $"{caller.DisplayName} (the host)" : character is null ? caller.DisplayName : $"{character.Name} ({caller.DisplayName})";
    }

    private static string Cut(string text, int max) => text.Length <= max ? text : text[..max] + "…";
}
