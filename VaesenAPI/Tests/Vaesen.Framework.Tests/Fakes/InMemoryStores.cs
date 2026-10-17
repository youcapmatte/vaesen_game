using System.Collections.Concurrent;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests.Fakes;

/// <summary>In-memory <see cref="IAccountStore"/> with the same rules as AccountRepository (unique username and Auth0 user).</summary>
public sealed class InMemoryAccountStore : IAccountStore
{
    private readonly ConcurrentDictionary<string, Account> _accounts = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> _usernames = new(StringComparer.OrdinalIgnoreCase);
    private readonly ConcurrentDictionary<string, string> _auth0UserIds = new(StringComparer.Ordinal);
    private readonly Lock _createLock = new();

    public Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken = default) =>
        Task.FromResult(_accounts.GetValueOrDefault(accountId));

    public Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken = default) =>
        Task.FromResult(_usernames.TryGetValue(username.Trim(), out var id) ? _accounts.GetValueOrDefault(id) : null);

    public Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default) =>
        Task.FromResult(_auth0UserIds.TryGetValue(auth0UserId, out var id) ? _accounts.GetValueOrDefault(id) : null);

    public Task<ItemAction<Account>> CreateAsync(Account account, CancellationToken cancellationToken = default)
    {
        var ret = new ItemAction<Account>();
        lock (_createLock)
        {
            var username = account.Username.Trim();
            if (_usernames.ContainsKey(username))
            {
                ret.Fail("Username is taken.", AccountRepository.UsernameTaken);
                return Task.FromResult(ret);
            }

            if (account.Auth0UserId is not null && _auth0UserIds.ContainsKey(account.Auth0UserId))
            {
                ret.Fail("This Auth0 user already has an account.", AccountRepository.Auth0UserTaken);
                return Task.FromResult(ret);
            }

            Stamp(account);
            _accounts[account.Id] = account;
            _usernames[username] = account.Id;
            if (account.Auth0UserId is not null)
            {
                _auth0UserIds[account.Auth0UserId] = account.Id;
            }
        }

        ret.Ok(account);
        return Task.FromResult(ret);
    }

    internal static void Stamp(BaseCore item)
    {
        var now = DateTime.UtcNow;
        item.CreatedAt = now;
        item.UpdatedAt = now;
        item.Version = 1;
    }
}

/// <summary>In-memory <see cref="ICharacterStore"/> with the same rules as CharacterRepository (unique names).</summary>
public sealed class InMemoryCharacterStore : ICharacterStore
{
    private readonly ConcurrentDictionary<string, Character> _characters = new(StringComparer.Ordinal);
    private readonly ConcurrentDictionary<string, string> _names = new(StringComparer.OrdinalIgnoreCase);
    private readonly Lock _createLock = new();

    public Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken = default) =>
        Task.FromResult(_characters.GetValueOrDefault(characterId));

    public Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken = default)
    {
        IReadOnlyList<Character> characters = _characters.Values
            .Where(c => string.Equals(c.AccountId, accountId, StringComparison.Ordinal))
            .OrderBy(c => c.Name, StringComparer.Ordinal)
            .ToList();
        return Task.FromResult(characters);
    }

    public Task<ItemAction<Character>> CreateAsync(Character character, CancellationToken cancellationToken = default)
    {
        var ret = new ItemAction<Character>();
        lock (_createLock)
        {
            var name = character.Name.Trim();
            if (_names.ContainsKey(name))
            {
                ret.Fail("That character name is taken.", CharacterRepository.NameTaken);
                return Task.FromResult(ret);
            }

            InMemoryAccountStore.Stamp(character);
            _names[name] = character.Id;
            _characters[character.Id] = character;
        }

        ret.Ok(character);
        return Task.FromResult(ret);
    }
}
