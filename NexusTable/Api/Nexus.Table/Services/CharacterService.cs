using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

public interface ICharacterService
{
    Task<ResultBase<Character>> GetAsync(string characterId, CancellationToken cancellationToken = default);

    Task<ResultBase<IReadOnlyList<Character>>> ListAsync(CancellationToken cancellationToken = default);

    /// <summary>Creates the caller's character and makes it the one they play.</summary>
    Task<ResultBase<Character>> CreateAsync(Account caller, CreateCharacterRequest request, CancellationToken cancellationToken = default);

    /// <summary>The owner (or the host) changes the free-text parts of the sheet.</summary>
    Task<ResultBase<Character>> UpdateAsync(Account caller, string characterId, UpdateCharacterRequest request, CancellationToken cancellationToken = default);

    /// <summary>The owner (or the host) marks or heals a Condition.</summary>
    Task<ResultBase<Character>> SetConditionAsync(Account caller, string characterId, SetConditionRequest request, CancellationToken cancellationToken = default);
}

public sealed class CharacterService : ICharacterService
{
    private readonly ICharacterStore _characters;
    private readonly IAccountStore _accounts;
    private readonly ISocietyService _society;
    private readonly IRulesService _rules;
    private readonly ITableNotifier _notifier;

    public CharacterService(
        ICharacterStore characters,
        IAccountStore accounts,
        ISocietyService society,
        IRulesService rules,
        ITableNotifier notifier)
    {
        _characters = characters;
        _accounts = accounts;
        _society = society;
        _rules = rules;
        _notifier = notifier;
    }

    public async Task<ResultBase<Character>> GetAsync(string characterId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var character = Character.IsValidId<Character>(characterId)
            ? await _characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false)
            : null;
        return character is null ? ret.Fail("Character not found.", ErrorCodes.NotFound) : ret.Ok(character);
    }

    public async Task<ResultBase<IReadOnlyList<Character>>> ListAsync(CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<Character>>();
        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        return ret.Ok(await _characters.ListBySocietyAsync(society.Result!.Id, cancellationToken).ConfigureAwait(false));
    }

    public async Task<ResultBase<Character>> CreateAsync(Account caller, CreateCharacterRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var rules = _rules.Rules;
        var errors = CharacterRules.Validate(request, rules);
        if (errors.Count > 0)
        {
            return ret.Fail(string.Join(" ", errors), ErrorCodes.Invalid);
        }

        var society = await _society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        // "There should not be more than one player character of the same archetype in the group."
        var others = await _characters.ListBySocietyAsync(society.Result!.Id, cancellationToken).ConfigureAwait(false);
        var sameArchetype = others.FirstOrDefault(c => c.Archetype == request.Archetype && c.AccountId != caller.Id);
        if (sameArchetype is not null)
        {
            var archetypeName = rules.FindArchetype(request.Archetype)!.Name;
            return ret.Fail($"{sameArchetype.Name} is already the Society's {archetypeName}. Choose another archetype.", NexusErrorCodes.ArchetypeTaken);
        }

        var created = await _characters.CreateAsync(CharacterRules.Build(request, rules, caller, society.Result.Id), cancellationToken).ConfigureAwait(false);
        if (!created.Success)
        {
            return created;
        }

        caller.ActiveCharacterId = created.Result!.Id;
        caller.UpdatedBy = caller.Id;
        var account = await _accounts.UpdateAsync(caller, cancellationToken).ConfigureAwait(false);
        if (!account.Success)
        {
            return ret.Fail(account.ErrorMessage, account.ErrorCode);
        }

        await _notifier.CharacterChangedAsync(created.Result, cancellationToken).ConfigureAwait(false);
        await _notifier.SocietyChangedAsync(society.Result.Id, cancellationToken).ConfigureAwait(false);
        return created;
    }

    public async Task<ResultBase<Character>> UpdateAsync(Account caller, string characterId, UpdateCharacterRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var found = await GetEditableAsync(caller, characterId, cancellationToken).ConfigureAwait(false);
        if (!found.Success)
        {
            return found;
        }

        var texts = new[] { request.Motivation, request.DarkSecret, request.Memento, request.Description, request.Advantage, request.Notes };
        if (texts.Any(t => t?.Length > CharacterRules.MaxTextLength)
            || request.Relationships?.Any(r => string.IsNullOrWhiteSpace(r.Name) || r.Text?.Length > CharacterRules.MaxTextLength) == true)
        {
            return ret.Fail($"Texts can be at most {CharacterRules.MaxTextLength} characters, and relationships need a name.", ErrorCodes.Invalid);
        }

        var character = found.Result!;
        character.Motivation = request.Motivation?.Trim() ?? character.Motivation;
        character.DarkSecret = request.DarkSecret?.Trim() ?? character.DarkSecret;
        character.Relationships = request.Relationships ?? character.Relationships;
        character.Memento = request.Memento?.Trim() ?? character.Memento;
        character.Equipment = request.Equipment?.Where(e => !string.IsNullOrWhiteSpace(e)).Select(e => e.Trim()).ToList() ?? character.Equipment;
        character.Description = request.Description?.Trim() ?? character.Description;
        character.Advantage = request.Advantage is null ? character.Advantage : NullIfEmpty(request.Advantage);
        character.PortraitUrl = request.PortraitUrl is null ? character.PortraitUrl : NullIfEmpty(request.PortraitUrl);
        character.Notes = request.Notes ?? character.Notes;
        return await SaveAsync(caller, character, cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<Character>> SetConditionAsync(Account caller, string characterId, SetConditionRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var found = await GetEditableAsync(caller, characterId, cancellationToken).ConfigureAwait(false);
        if (!found.Success)
        {
            return found;
        }

        var character = found.Result!;
        var known = request.Active
            ? CharacterRules.AddCondition(character, request.Condition, _rules.Rules)
            : CharacterRules.RemoveCondition(character, request.Condition, _rules.Rules);
        return known
            ? await SaveAsync(caller, character, cancellationToken).ConfigureAwait(false)
            : ret.Fail($"Unknown Condition '{request.Condition}'.", ErrorCodes.Invalid);
    }

    private async Task<ResultBase<Character>> GetEditableAsync(Account caller, string characterId, CancellationToken cancellationToken)
    {
        var found = await GetAsync(characterId, cancellationToken).ConfigureAwait(false);
        if (found.Success && found.Result!.AccountId != caller.Id && caller.Role != AccountRole.Host)
        {
            return new ResultBase<Character>().Fail("That is someone else's character.", ErrorCodes.Forbidden);
        }

        return found;
    }

    private async Task<ResultBase<Character>> SaveAsync(Account caller, Character character, CancellationToken cancellationToken)
    {
        character.UpdatedBy = caller.Id;
        var updated = await _characters.UpdateAsync(character, cancellationToken).ConfigureAwait(false);
        if (updated.Success)
        {
            await _notifier.CharacterChangedAsync(updated.Result!, cancellationToken).ConfigureAwait(false);
        }

        return updated;
    }

    private static string? NullIfEmpty(string value) => string.IsNullOrWhiteSpace(value) ? null : value.Trim();
}
