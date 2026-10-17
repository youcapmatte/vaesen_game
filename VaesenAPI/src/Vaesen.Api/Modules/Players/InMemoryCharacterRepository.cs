using System.Collections.Concurrent;

namespace Vaesen.Api.Modules.Players;

/// <summary>Process-local stand-in for tests. Same behaviour as the DynamoDB version, including unique names.</summary>
public sealed class InMemoryCharacterRepository : ICharacterRepository
{
    private readonly ConcurrentDictionary<string, Character> _characters = new();
    private readonly ConcurrentDictionary<string, string> _names = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _createLock = new();

    public Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken) =>
        Task.FromResult(_characters.GetValueOrDefault(characterId));

    public Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken)
    {
        IReadOnlyList<Character> characters = _characters.Values
            .Where(c => c.AccountId == accountId)
            .OrderBy(c => c.Name, StringComparer.Ordinal)
            .ToList();
        return Task.FromResult(characters);
    }

    public Task<bool> TryCreateAsync(Character character, CancellationToken cancellationToken)
    {
        lock (_createLock)
        {
            var name = character.Name.Trim();
            if (_characters.ContainsKey(character.Id) || _names.ContainsKey(name))
            {
                return Task.FromResult(false);
            }

            _names[name] = character.Id;
            _characters[character.Id] = character;
            return Task.FromResult(true);
        }
    }
}
