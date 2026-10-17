using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Data;

namespace Vaesen.Framework.Common.Repositorys;

/// <summary>Persistence for <see cref="Account"/>, so services can be tested without DynamoDB.</summary>
public interface IAccountStore
{
    Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken = default);

    /// <summary>Case-insensitive.</summary>
    Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken = default);

    Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default);

    /// <summary>
    /// Creates the account and reserves its username (and Auth0 user id, if set) in one step.
    /// Fails with <see cref="AccountRepository.UsernameTaken"/> or <see cref="AccountRepository.Auth0UserTaken"/>.
    /// </summary>
    Task<ItemAction<Account>> CreateAsync(Account account, CancellationToken cancellationToken = default);
}

/// <summary>
/// Accounts in the Vaesen table:
///   {accountId} / ITEM                     the account
///   USERNAME#{lowercase} / USERNAME        username reservation and lookup
///   AUTH0#{auth0UserId} / AUTH0            Auth0 link and lookup
/// </summary>
public sealed class AccountRepository : BaseRepository<Account>, IAccountStore
{
    public const string UsernameTaken = "username_taken";
    public const string Auth0UserTaken = "auth0_user_taken";

    private const string UsernameKind = "USERNAME";
    private const string Auth0Kind = "AUTH0";

    public AccountRepository(IAmazonDynamoDB dynamoDb, IOptions<DataServiceOptions> options, TimeProvider timeProvider)
        : base(dynamoDb, options, timeProvider)
    {
    }

    public static string UsernameKey(string username) => $"USERNAME#{username.Trim().ToLowerInvariant()}";

    public static string Auth0Key(string auth0UserId) => $"AUTH0#{auth0UserId.Trim()}";

    public Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken = default) =>
        GetItemAsync(accountId, cancellationToken);

    public async Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(username))
        {
            return null;
        }

        var accountId = await GetReservationOwnerAsync(UsernameKey(username), UsernameKind, cancellationToken).ConfigureAwait(false);
        return accountId is null ? null : await GetItemAsync(accountId, cancellationToken).ConfigureAwait(false);
    }

    public async Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(auth0UserId))
        {
            return null;
        }

        var accountId = await GetReservationOwnerAsync(Auth0Key(auth0UserId), Auth0Kind, cancellationToken).ConfigureAwait(false);
        return accountId is null ? null : await GetItemAsync(accountId, cancellationToken).ConfigureAwait(false);
    }

    public Task<ItemAction<Account>> CreateAsync(Account account, CancellationToken cancellationToken = default)
    {
        var reservations = new List<Reservation>
        {
            new(UsernameKey(account.Username), UsernameKind, "Username is taken.", UsernameTaken),
        };
        if (!string.IsNullOrWhiteSpace(account.Auth0UserId))
        {
            reservations.Add(new(Auth0Key(account.Auth0UserId), Auth0Kind, "This Auth0 user already has an account.", Auth0UserTaken));
        }

        return CreateItemAsync(account, reservations, cancellationToken);
    }
}
