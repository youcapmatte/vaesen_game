using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Services;

/// <summary>Reading, changing and deleting characters. Creating one happens in its guild (<see cref="IGuildService.CreateCharacterAsync"/>).</summary>
public interface ICharacterService
{
    Task<ResultBase<Character>> GetAsync(string characterId, CancellationToken cancellationToken = default);

    Task<ResultBase<Character>> GetByNameAsync(string name, CancellationToken cancellationToken = default);

    /// <summary>Changes first/last name, class and health. The character name can't be changed.</summary>
    Task<ResultBase<Character>> UpdateAsync(string characterId, UpdateCharacterRequest request, CancellationToken cancellationToken = default);

    /// <summary>Deletes the character: it leaves its guild, whoever played it stops, and its name becomes free again.</summary>
    Task<ResultBase<Character>> DeleteAsync(string characterId, CancellationToken cancellationToken = default);
}

public sealed class CharacterService : ICharacterService
{
    private readonly ICharacterStore _characters;
    private readonly IGuildStore _guilds;

    public CharacterService(ICharacterStore characters, IGuildStore guilds)
    {
        _characters = characters;
        _guilds = guilds;
    }

    public async Task<ResultBase<Character>> GetAsync(string characterId, CancellationToken cancellationToken = default)
    {
        if (!Character.IsValidId<Character>(characterId))
        {
            return new ResultBase<Character>().Fail("Not a character id (expected 'character-{guid}').", ErrorCodes.Invalid);
        }

        return Found(await _characters.GetAsync(characterId, cancellationToken).ConfigureAwait(false));
    }

    public async Task<ResultBase<Character>> GetByNameAsync(string name, CancellationToken cancellationToken = default) =>
        Found(await _characters.GetByNameAsync(name, cancellationToken).ConfigureAwait(false));

    public async Task<ResultBase<Character>> UpdateAsync(string characterId, UpdateCharacterRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var loaded = await GetAsync(characterId, cancellationToken).ConfigureAwait(false);
        if (!loaded.Success)
        {
            return loaded;
        }

        var character = loaded.Result!;

        // Null means "keep"; an empty string clears a text field.
        var firstName = request?.FirstName is null ? character.FirstName : CharacterRules.Clean(request.FirstName);
        var lastName = request?.LastName is null ? character.LastName : CharacterRules.Clean(request.LastName);
        var characterClass = request?.Class is null ? character.Class : CharacterRules.Clean(request.Class);
        var healthPoints = request?.HealthPoints ?? character.HealthPoints;

        var invalid = CharacterRules.ValidatePersonName(firstName, "First name")
            ?? CharacterRules.ValidatePersonName(lastName, "Last name")
            ?? CharacterRules.ValidateClass(characterClass)
            ?? CharacterRules.ValidateHealthPoints(healthPoints);
        if (invalid is not null)
        {
            return ret.Fail(invalid, ErrorCodes.Invalid);
        }

        character.FirstName = firstName;
        character.LastName = lastName;
        character.Class = characterClass;
        character.HealthPoints = healthPoints;
        if (!string.IsNullOrWhiteSpace(request?.UpdatedByAccountId))
        {
            character.UpdatedBy = request.UpdatedByAccountId.Trim();
        }

        var saved = await _characters.UpdateAsync(character, cancellationToken).ConfigureAwait(false);
        return saved.Success ? ret.Ok(character) : ret.Fail(saved.ErrorMessage, saved.ErrorCode);
    }

    public async Task<ResultBase<Character>> DeleteAsync(string characterId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var loaded = await GetAsync(characterId, cancellationToken).ConfigureAwait(false);
        if (!loaded.Success)
        {
            return loaded;
        }

        var character = loaded.Result!;
        var guild = await _guilds.GetAsync(character.GuildId, cancellationToken).ConfigureAwait(false);
        if (guild is null)
        {
            return ret.Fail("The character's guild doesn't exist.", ErrorCodes.NotFound);
        }

        guild.CharacterIds.RemoveAll(id => string.Equals(id, character.Id, StringComparison.Ordinal));
        guild.CharacterAssignments.RemoveAll(a => string.Equals(a.CharacterId, character.Id, StringComparison.Ordinal));

        var saved = await _guilds.DeleteCharacterAsync(guild, character, cancellationToken).ConfigureAwait(false);
        return saved.Success ? ret.Ok(character) : ret.Fail(saved.ErrorMessage, saved.ErrorCode);
    }

    private static ResultBase<Character> Found(Character? character) =>
        character is null
            ? new ResultBase<Character>().Fail("Character not found.", ErrorCodes.NotFound)
            : new ResultBase<Character>().Ok(character);
}
