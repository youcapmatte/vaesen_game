namespace Vaesen.Api.Modules.Accounts;

public interface IAccountRepository
{
    Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken);

    /// <summary>Case-insensitive.</summary>
    Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken);

    Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken);

    /// <summary>Creates the account and reserves its username (and Auth0 user id, if set) in one step.</summary>
    Task<CreateAccountResult> TryCreateAsync(Account account, CancellationToken cancellationToken);
}

public enum CreateAccountResult
{
    Created,
    UsernameTaken,
    Auth0UserIdTaken,
}
