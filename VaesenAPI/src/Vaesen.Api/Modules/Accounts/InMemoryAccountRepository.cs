using System.Collections.Concurrent;

namespace Vaesen.Api.Modules.Accounts;

/// <summary>Process-local stand-in for tests. Same behaviour as the DynamoDB version, including unique usernames.</summary>
public sealed class InMemoryAccountRepository : IAccountRepository
{
    private readonly ConcurrentDictionary<string, Account> _accounts = new();
    private readonly ConcurrentDictionary<string, string> _usernames = new(StringComparer.OrdinalIgnoreCase);
    private readonly ConcurrentDictionary<string, string> _auth0UserIds = new();
    private readonly Lock _createLock = new();

    public Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken) =>
        Task.FromResult(_accounts.GetValueOrDefault(accountId));

    public Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken) =>
        Task.FromResult(_usernames.TryGetValue(username.Trim(), out var id) ? _accounts.GetValueOrDefault(id) : null);

    public Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken) =>
        Task.FromResult(_auth0UserIds.TryGetValue(auth0UserId, out var id) ? _accounts.GetValueOrDefault(id) : null);

    public Task<CreateAccountResult> TryCreateAsync(Account account, CancellationToken cancellationToken)
    {
        lock (_createLock)
        {
            var username = account.Username.Trim();
            if (_usernames.ContainsKey(username))
            {
                return Task.FromResult(CreateAccountResult.UsernameTaken);
            }
            if (account.Auth0UserId is not null && _auth0UserIds.ContainsKey(account.Auth0UserId))
            {
                return Task.FromResult(CreateAccountResult.Auth0UserIdTaken);
            }

            _accounts[account.Id] = account;
            _usernames[username] = account.Id;
            if (account.Auth0UserId is not null)
            {
                _auth0UserIds[account.Auth0UserId] = account.Id;
            }
            return Task.FromResult(CreateAccountResult.Created);
        }
    }
}
