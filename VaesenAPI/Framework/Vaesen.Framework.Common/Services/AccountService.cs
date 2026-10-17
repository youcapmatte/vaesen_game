using System.Text.RegularExpressions;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Services;

public interface IAccountService
{
    Task<ResultBase<Account>> CreateAsync(CreateAccountRequest request, CancellationToken cancellationToken = default);

    Task<ResultBase<Account>> GetAsync(string accountId, CancellationToken cancellationToken = default);

    Task<ResultBase<Account>> GetByUsernameAsync(string username, CancellationToken cancellationToken = default);

    Task<ResultBase<Account>> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default);
}

public sealed partial class AccountService : IAccountService
{
    private const int MaxEmailLength = 254;
    private const int MaxAuth0UserIdLength = 256;

    private readonly IAccountStore _accounts;

    public AccountService(IAccountStore accounts)
    {
        _accounts = accounts;
    }

    public async Task<ResultBase<Account>> CreateAsync(CreateAccountRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Account>();

        var username = (request?.Username ?? "").Trim();
        var email = string.IsNullOrWhiteSpace(request?.Email) ? null : request.Email.Trim();
        var auth0UserId = string.IsNullOrWhiteSpace(request?.Auth0UserId) ? null : request.Auth0UserId.Trim();

        if (!UsernamePattern().IsMatch(username))
        {
            return ret.Fail("Username must be 3-24 characters: letters, digits, '_' or '-'.", ErrorCodes.Invalid);
        }

        if (email is not null && (email.Length > MaxEmailLength || !email.Contains('@', StringComparison.Ordinal)))
        {
            return ret.Fail("Not a valid email address.", ErrorCodes.Invalid);
        }

        if (auth0UserId is not null && (auth0UserId.Length > MaxAuth0UserIdLength || !auth0UserId.Contains('|', StringComparison.Ordinal)))
        {
            return ret.Fail("Expected Auth0's user_id, e.g. 'auth0|65f1c2...'.", ErrorCodes.Invalid);
        }

        var account = new Account
        {
            Username = username,
            Email = email,
            Auth0UserId = auth0UserId,
        };

        var created = await _accounts.CreateAsync(account, cancellationToken).ConfigureAwait(false);
        return created.Success ? ret.Ok(created.Result!) : ret.Fail(created.ErrorMessage, created.ErrorCode);
    }

    public async Task<ResultBase<Account>> GetAsync(string accountId, CancellationToken cancellationToken = default)
    {
        if (!Account.IsValidId<Account>(accountId))
        {
            return new ResultBase<Account>().Fail("Not an account id (expected 'account-{guid}').", ErrorCodes.Invalid);
        }

        return Found(await _accounts.GetAsync(accountId, cancellationToken).ConfigureAwait(false));
    }

    public async Task<ResultBase<Account>> GetByUsernameAsync(string username, CancellationToken cancellationToken = default) =>
        Found(await _accounts.GetByUsernameAsync(username, cancellationToken).ConfigureAwait(false));

    public async Task<ResultBase<Account>> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default) =>
        Found(await _accounts.GetByAuth0UserIdAsync(auth0UserId, cancellationToken).ConfigureAwait(false));

    private static ResultBase<Account> Found(Account? account) =>
        account is null
            ? new ResultBase<Account>().Fail("Account not found.", ErrorCodes.NotFound)
            : new ResultBase<Account>().Ok(account);

    [GeneratedRegex("^[A-Za-z0-9_-]{3,24}$")]
    private static partial Regex UsernamePattern();
}
