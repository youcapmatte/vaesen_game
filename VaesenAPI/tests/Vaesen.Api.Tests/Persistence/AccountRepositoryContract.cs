using Vaesen.Api.Common;
using Vaesen.Api.Modules.Accounts;

namespace Vaesen.Api.Tests.Persistence;

/// <summary>Behaviour every IAccountRepository must have, run against the in-memory and the DynamoDB implementation.</summary>
public abstract class AccountRepositoryContract
{
    protected abstract IAccountRepository Repository { get; }

    protected virtual void SkipIfUnavailable()
    {
    }

    /// <summary>Usernames are unique per table, so tests sharing a database need their own.</summary>
    private static string UniqueUsername(string name) => $"{name}_{Guid.NewGuid():N}"[..24];

    private static Account NewAccount(string username, string? auth0UserId = null) => new(
        EntityIds.New(EntityIds.Account),
        username,
        "player@example.com",
        auth0UserId,
        new AccountSubscription(SubscriptionStatus.Active, "monthly", new DateTimeOffset(2030, 1, 1, 0, 0, 0, TimeSpan.Zero)),
        new DateTimeOffset(2026, 10, 17, 9, 0, 0, TimeSpan.Zero),
        Version: 1);

    [SkippableFact]
    public async Task Create_ThenGetById_RoundTripsAllFields()
    {
        SkipIfUnavailable();
        var account = NewAccount(UniqueUsername("Ingrid"), $"auth0|{Guid.NewGuid():N}");

        Assert.Equal(CreateAccountResult.Created, await Repository.TryCreateAsync(account, CancellationToken.None));

        Assert.Equal(account, await Repository.GetAsync(account.Id, CancellationToken.None));
    }

    [SkippableFact]
    public async Task GetByUsername_IsCaseInsensitive()
    {
        SkipIfUnavailable();
        var account = NewAccount(UniqueUsername("Johan"));
        await Repository.TryCreateAsync(account, CancellationToken.None);

        var found = await Repository.GetByUsernameAsync(account.Username.ToUpperInvariant(), CancellationToken.None);

        Assert.Equal(account.Id, found?.Id);
    }

    [SkippableFact]
    public async Task GetByAuth0UserId_FindsTheLinkedAccount()
    {
        SkipIfUnavailable();
        var auth0UserId = $"google-oauth2|{Guid.NewGuid():N}";
        var account = NewAccount(UniqueUsername("Astrid"), auth0UserId);
        await Repository.TryCreateAsync(account, CancellationToken.None);

        Assert.Equal(account.Id, (await Repository.GetByAuth0UserIdAsync(auth0UserId, CancellationToken.None))?.Id);
    }

    [SkippableFact]
    public async Task Create_WithTakenUsername_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var username = UniqueUsername("Erik");
        await Repository.TryCreateAsync(NewAccount(username), CancellationToken.None);
        var duplicate = NewAccount(username.ToLowerInvariant());

        Assert.Equal(CreateAccountResult.UsernameTaken, await Repository.TryCreateAsync(duplicate, CancellationToken.None));
        Assert.Null(await Repository.GetAsync(duplicate.Id, CancellationToken.None));
    }

    [SkippableFact]
    public async Task Create_WithTakenAuth0UserId_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var auth0UserId = $"auth0|{Guid.NewGuid():N}";
        await Repository.TryCreateAsync(NewAccount(UniqueUsername("Sigrid"), auth0UserId), CancellationToken.None);
        var duplicate = NewAccount(UniqueUsername("Other"), auth0UserId);

        Assert.Equal(CreateAccountResult.Auth0UserIdTaken, await Repository.TryCreateAsync(duplicate, CancellationToken.None));
        Assert.Null(await Repository.GetAsync(duplicate.Id, CancellationToken.None));
        Assert.Null(await Repository.GetByUsernameAsync(duplicate.Username, CancellationToken.None));
    }

    [SkippableFact]
    public async Task Lookups_ForUnknownValues_ReturnNull()
    {
        SkipIfUnavailable();

        Assert.Null(await Repository.GetAsync(EntityIds.New(EntityIds.Account), CancellationToken.None));
        Assert.Null(await Repository.GetByUsernameAsync(UniqueUsername("Nobody"), CancellationToken.None));
        Assert.Null(await Repository.GetByAuth0UserIdAsync("auth0|nobody", CancellationToken.None));
    }
}
