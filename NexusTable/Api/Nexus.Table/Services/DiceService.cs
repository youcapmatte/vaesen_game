using Microsoft.Extensions.Options;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

/// <summary>
/// Skill tests by the Vaesen rules (chapter 3), rolled on the server:
///   pool = attribute + skill + difficulty + bonus (+2 for the Advantage) - 1 per Condition of the matching type,
///   at least one die; every six is a success, and one is usually enough, but the Gamemaster may ask for more
///   (successesNeeded). A roll of a test the Gamemaster asked for takes its skill, difficulty and sixes. A failed roll can be pushed once: every die that isn't a six is
///   thrown again, and the character suffers a Condition of the roll's type, which takes effect after the push.
/// </summary>
public interface IDiceService
{
    Task<ResultBase<RollOutcome>> RollAsync(Account caller, RollRequest request, CancellationToken cancellationToken = default);

    Task<ResultBase<RollOutcome>> PushAsync(Account caller, string rollId, PushRollRequest request, CancellationToken cancellationToken = default);

    Task<ResultBase<IReadOnlyList<Roll>>> RecentAsync(CancellationToken cancellationToken = default);
}

public sealed class DiceService : IDiceService
{
    public const int MaxModifier = 3;
    public const int AdvantageDice = 2;
    private const int MaxReasonLength = 200;

    private readonly IRollStore _rolls;
    private readonly ICharacterStore _characters;
    private readonly ISocietyService _society;
    private readonly IRulesService _rules;
    private readonly IDiceRoller _dice;
    private readonly ITableNotifier _notifier;
    private readonly IOptions<NexusOptions> _options;
    private readonly ISessionService _sessions;

    public DiceService(
        IRollStore rolls,
        ICharacterStore characters,
        ISocietyService society,
        IRulesService rules,
        IDiceRoller dice,
        ITableNotifier notifier,
        IOptions<NexusOptions> options,
        ISessionService sessions)
    {
        _sessions = sessions;
        _rolls = rolls;
        _characters = characters;
        _society = society;
        _rules = rules;
        _dice = dice;
        _notifier = notifier;
        _options = options;
    }

    public async Task<ResultBase<RollOutcome>> RollAsync(Account caller, RollRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<RollOutcome>();
        var rules = _rules.Rules;

        var character = await GetRollableAsync(caller, request.CharacterId, cancellationToken).ConfigureAwait(false);
        if (!character.Success)
        {
            return ret.Fail(character.ErrorMessage, character.ErrorCode);
        }

        var sheet = character.Result!;

        // A test the Gamemaster asked for sets what is rolled, how hard, and how many sixes it takes.
        if (request.TestId is { Length: > 0 } testId)
        {
            var test = await _sessions.FindPendingTestAsync(testId, cancellationToken).ConfigureAwait(false);
            if (test is null || test.CharacterId != sheet.Id)
            {
                return ret.Fail("That test is no longer waiting for this character.", ErrorCodes.Invalid);
            }

            request.Skill = test.Skill;
            request.Attribute = test.Skill is null ? test.Attribute : null;
            request.Difficulty = test.Difficulty;
            request.SuccessesNeeded = test.SuccessesNeeded;
            request.Reason ??= test.Reason;
        }

        if (request.SuccessesNeeded is < 1 or > SessionService.MaxSuccessesNeeded)
        {
            return ret.Fail($"Sixes needed are 1 to {SessionService.MaxSuccessesNeeded}.", ErrorCodes.Invalid);
        }

        if (sheet.PhysicallyBroken || sheet.MentallyBroken)
        {
            return ret.Fail($"{sheet.Name} is Broken and can't succeed at anything until their Conditions are treated.", ErrorCodes.Invalid);
        }

        var skill = request.Skill is null ? null : rules.FindSkill(request.Skill);
        var attribute = skill?.Attribute ?? request.Attribute;
        if ((request.Skill is not null && skill is null) || !rules.IsAttribute(attribute))
        {
            return ret.Fail("Choose a skill, or an attribute for an attribute test.", ErrorCodes.Invalid);
        }

        if (Math.Abs(request.Difficulty) > MaxModifier || Math.Abs(request.Bonus) > MaxModifier)
        {
            return ret.Fail($"Difficulty and bonus are -{MaxModifier} to +{MaxModifier}.", ErrorCodes.Invalid);
        }

        if (request.UseAdvantage && sheet.Advantage is null)
        {
            return ret.Fail($"{sheet.Name} has no Advantage this mystery.", ErrorCodes.Invalid);
        }

        var pool = BuildPool(sheet, attribute!, skill, request, rules);
        var size = Math.Max(1, pool.Sum(p => p.Dice));
        var dice = Enumerable.Range(0, size).Select(_ => _dice.D6()).ToList();

        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        var roll = new Roll
        {
            SocietyId = society.Result!.Id,
            AccountId = caller.Id,
            CharacterId = sheet.Id,
            CharacterName = sheet.Name,
            Skill = skill?.Id,
            Attribute = attribute!,
            Pool = pool,
            PoolSize = size,
            Dice = dice,
            Successes = CountSuccesses(dice, rules),
            SuccessesNeeded = request.SuccessesNeeded,
            TestId = string.IsNullOrEmpty(request.TestId) ? null : request.TestId,
            Reason = Trim(request.Reason),
            CreatedBy = caller.Id,
        };

        var created = await _rolls.CreateAsync(roll, cancellationToken).ConfigureAwait(false);
        if (!created.Success)
        {
            return ret.Fail(created.ErrorMessage, created.ErrorCode);
        }

        await _notifier.RollMadeAsync(created.Result!, cancellationToken).ConfigureAwait(false);
        await _sessions.RollMadeAsync(created.Result!, cancellationToken).ConfigureAwait(false);
        return ret.Ok(new RollOutcome { Roll = created.Result!, Character = sheet });
    }

    public async Task<ResultBase<RollOutcome>> PushAsync(Account caller, string rollId, PushRollRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<RollOutcome>();
        var rules = _rules.Rules;

        var roll = Roll.IsValidId<Roll>(rollId) ? await _rolls.GetAsync(rollId, cancellationToken).ConfigureAwait(false) : null;
        if (roll is null)
        {
            return ret.Fail("Roll not found.", ErrorCodes.NotFound);
        }

        if (roll.Pushed)
        {
            return ret.Fail("A roll can only be pushed once.", NexusErrorCodes.AlreadyPushed);
        }

        if (roll.Succeeded)
        {
            return ret.Fail("Only a failed roll can be pushed.", ErrorCodes.Invalid);
        }

        var character = await GetRollableAsync(caller, roll.CharacterId, cancellationToken).ConfigureAwait(false);
        if (!character.Success)
        {
            return ret.Fail(character.ErrorMessage, character.ErrorCode);
        }

        var sheet = character.Result!;
        var physical = VaesenRules.IsPhysicalAttribute(roll.Attribute);
        var ofType = physical ? rules.Conditions.Physical : rules.Conditions.Mental;
        var willBreak = ofType.All(c => sheet.Conditions.Contains(c.Id));
        if (!willBreak && (!ofType.Exists(c => c.Id == request.Condition) || sheet.Conditions.Contains(request.Condition)))
        {
            var kind = physical ? "physical" : "mental";
            return ret.Fail($"Pushing costs a {kind} Condition {sheet.Name} doesn't have yet.", ErrorCodes.Invalid);
        }

        var pushed = roll.Dice.Select(d => d == rules.Dice.SuccessOn ? d : _dice.D6()).ToList();
        roll.PushedDice = pushed;
        roll.Pushed = true;
        roll.Successes = CountSuccesses(pushed, rules);
        roll.PushCondition = willBreak ? null : request.Condition;
        roll.UpdatedBy = caller.Id;

        // The Condition comes after the push; with every Condition of the type already taken, the character breaks.
        CharacterRules.AddCondition(sheet, willBreak ? ofType[0].Id : request.Condition, rules);
        sheet.UpdatedBy = caller.Id;

        var savedRoll = await _rolls.UpdateAsync(roll, cancellationToken).ConfigureAwait(false);
        if (!savedRoll.Success)
        {
            return ret.Fail(savedRoll.ErrorMessage, savedRoll.ErrorCode);
        }

        var savedCharacter = await _characters.UpdateAsync(sheet, cancellationToken).ConfigureAwait(false);
        if (!savedCharacter.Success)
        {
            return ret.Fail(savedCharacter.ErrorMessage, savedCharacter.ErrorCode);
        }

        await _notifier.RollMadeAsync(savedRoll.Result!, cancellationToken).ConfigureAwait(false);
        await _notifier.CharacterChangedAsync(savedCharacter.Result!, cancellationToken).ConfigureAwait(false);
        await _sessions.RollMadeAsync(savedRoll.Result!, cancellationToken).ConfigureAwait(false);
        return ret.Ok(new RollOutcome { Roll = savedRoll.Result!, Character = savedCharacter.Result! });
    }

    public async Task<ResultBase<IReadOnlyList<Roll>>> RecentAsync(CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<Roll>>();
        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        return ret.Ok(await _rolls.ListRecentBySocietyAsync(society.Result!.Id, _options.Value.RecentRolls, cancellationToken).ConfigureAwait(false));
    }

    /// <summary>The pool, part by part, so the table sees how it was built.</summary>
    public static List<RollPoolPart> BuildPool(Character sheet, string attribute, RuleSkill? skill, RollRequest request, VaesenRules rules)
    {
        var attributeName = rules.Attributes.Find(a => a.Id == attribute)?.Name ?? attribute;
        var pool = new List<RollPoolPart> { new() { Label = attributeName, Dice = sheet.Attributes.GetValueOrDefault(attribute) } };
        if (skill is not null)
        {
            pool.Add(new() { Label = skill.Name, Dice = sheet.Skills.GetValueOrDefault(skill.Id) });
        }

        if (request.Difficulty != 0)
        {
            pool.Add(new() { Label = "Difficulty", Dice = request.Difficulty });
        }

        if (request.Bonus != 0)
        {
            pool.Add(new() { Label = "Gear and help", Dice = request.Bonus });
        }

        if (request.UseAdvantage)
        {
            pool.Add(new() { Label = "Advantage", Dice = AdvantageDice });
        }

        var conditions = VaesenRules.IsPhysicalAttribute(attribute) ? rules.Conditions.Physical : rules.Conditions.Mental;
        pool.AddRange(conditions
            .Where(c => sheet.Conditions.Contains(c.Id))
            .Select(c => new RollPoolPart { Label = c.Name, Dice = -1 }));

        return pool;
    }

    private static int CountSuccesses(IEnumerable<int> dice, VaesenRules rules) => dice.Count(d => d == rules.Dice.SuccessOn);

    private async Task<ResultBase<Character>> GetRollableAsync(Account caller, string characterId, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<Character>();
        var character = Character.IsValidId<Character>(characterId)
            ? await _characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false)
            : null;
        if (character is null)
        {
            return ret.Fail("Character not found.", ErrorCodes.NotFound);
        }

        // Players roll for their own characters; the host may roll for anyone (e.g. a player without a keyboard).
        return character.AccountId == caller.Id || caller.Role == AccountRole.Host
            ? ret.Ok(character)
            : ret.Fail("You can only roll for your own character.", ErrorCodes.Forbidden);
    }

    private static string? Trim(string? text)
    {
        var trimmed = text?.Trim();
        if (string.IsNullOrEmpty(trimmed))
        {
            return null;
        }

        return trimmed.Length > MaxReasonLength ? trimmed[..MaxReasonLength] : trimmed;
    }
}
