using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;

namespace Vaesen.Framework.Tests.Persistence;

/// <summary>
/// Behaviour every <see cref="IAccountStore"/> must have. Runs against the in-memory fake and the real
/// AccountRepository (DynamoDB Local), so the fake used by endpoint tests can't drift from the real thing.
/// </summary>
public abstract class AccountStoreContract
{
    protected abstract IAccountStore Store { get; }

    protected virtual void SkipIfUnavailable()
    {
    }

    /// <summary>Usernames are unique per table, so tests sharing a database need their own.</summary>
    private static string UniqueUsername(string name) => $"{name}_{Guid.NewGuid():N}"[..24];

    private static Account NewAccount(string username, string? auth0UserId = null) => new()
    {
        Username = username,
        Email = "player@example.com",
        Auth0UserId = auth0UserId,
        GuildIds = ["guild-1a2b3c4d-0000-4000-8000-000000000001"],
        Subscription = new AccountSubscription
        {
            Status = SubscriptionStatus.Active,
            Plan = "monthly",
            CurrentPeriodEnd = new DateTime(2030, 1, 1, 0, 0, 0, DateTimeKind.Utc),
        },
    };

    [SkippableFact]
    public async Task Create_ThenGetById_RoundTripsAllFields()
    {
        SkipIfUnavailable();
        var account = NewAccount(UniqueUsername("Ingrid"), $"auth0|{Guid.NewGuid():N}");

        var created = await Store.CreateAsync(account);
        var loaded = await Store.GetAsync(account.Id);

        Assert.True(created.Success);
        Assert.NotNull(loaded);
        Assert.Equal(account.Username, loaded.Username);
        Assert.Equal(account.Email, loaded.Email);
        Assert.Equal(account.Auth0UserId, loaded.Auth0UserId);
        Assert.Equal(SubscriptionStatus.Active, loaded.Subscription.Status);
        Assert.Equal("monthly", loaded.Subscription.Plan);
        Assert.Equal(account.Subscription.CurrentPeriodEnd, loaded.Subscription.CurrentPeriodEnd);
        Assert.Equal(account.GuildIds, loaded.GuildIds);
        Assert.Equal(1, loaded.Version);
        Assert.NotEqual(default, loaded.CreatedAt);
    }

    [SkippableFact]
    public async Task GetByUsername_IsCaseInsensitive()
    {
        SkipIfUnavailable();
        var account = NewAccount(UniqueUsername("Johan"));
        await Store.CreateAsync(account);

        var found = await Store.GetByUsernameAsync(account.Username.ToUpperInvariant());

        Assert.Equal(account.Id, found?.Id);
    }

    [SkippableFact]
    public async Task GetByAuth0UserId_FindsTheLinkedAccount()
    {
        SkipIfUnavailable();
        var auth0UserId = $"google-oauth2|{Guid.NewGuid():N}";
        var account = NewAccount(UniqueUsername("Astrid"), auth0UserId);
        await Store.CreateAsync(account);

        Assert.Equal(account.Id, (await Store.GetByAuth0UserIdAsync(auth0UserId))?.Id);
    }

    [SkippableFact]
    public async Task Create_WithTakenUsername_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var username = UniqueUsername("Erik");
        await Store.CreateAsync(NewAccount(username));
        var duplicate = NewAccount(username.ToLowerInvariant());

        var result = await Store.CreateAsync(duplicate);

        Assert.Equal(AccountRepository.UsernameTaken, result.ErrorCode);
        Assert.Null(await Store.GetAsync(duplicate.Id));
    }

    [SkippableFact]
    public async Task Create_WithTakenAuth0UserId_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var auth0UserId = $"auth0|{Guid.NewGuid():N}";
        await Store.CreateAsync(NewAccount(UniqueUsername("Sigrid"), auth0UserId));
        var duplicate = NewAccount(UniqueUsername("Other"), auth0UserId);

        var result = await Store.CreateAsync(duplicate);

        Assert.Equal(AccountRepository.Auth0UserTaken, result.ErrorCode);
        Assert.Null(await Store.GetAsync(duplicate.Id));
        Assert.Null(await Store.GetByUsernameAsync(duplicate.Username));
    }

    [SkippableFact]
    public async Task Lookups_ForUnknownValues_ReturnNull()
    {
        SkipIfUnavailable();

        Assert.Null(await Store.GetAsync(new Account().Id));
        Assert.Null(await Store.GetByUsernameAsync(UniqueUsername("Nobody")));
        Assert.Null(await Store.GetByAuth0UserIdAsync("auth0|nobody"));
    }
}

/// <summary>Behaviour every <see cref="ICharacterStore"/> must have (in-memory fake and CharacterRepository).</summary>
public abstract class CharacterStoreContract
{
    protected abstract ICharacterStore Store { get; }

    protected virtual void SkipIfUnavailable()
    {
    }

    private static Character NewCharacter(string accountId, string name) => new() { AccountId = accountId, Name = name };

    private static string NewAccountId() => new Account().Id;

    /// <summary>Names are unique per table, so tests sharing a database need their own.</summary>
    private static string UniqueName(string name) => $"{name}{Guid.NewGuid():N}"[..20];

    [SkippableFact]
    public async Task Create_ThenGet_ReturnsTheCharacter()
    {
        SkipIfUnavailable();
        var character = NewCharacter(NewAccountId(), UniqueName("Ingrid"));

        Assert.True((await Store.CreateAsync(character)).Success);
        var loaded = await Store.GetAsync(character.Id);

        Assert.Equal(character.Name, loaded?.Name);
        Assert.Equal(character.AccountId, loaded?.AccountId);
    }

    [SkippableFact]
    public async Task Get_Unknown_ReturnsNull()
    {
        SkipIfUnavailable();

        Assert.Null(await Store.GetAsync(new Character().Id));
    }

    [SkippableFact]
    public async Task Create_WithTakenName_DifferentCase_Fails()
    {
        SkipIfUnavailable();
        var name = UniqueName("Johan");
        Assert.True((await Store.CreateAsync(NewCharacter(NewAccountId(), name))).Success);
        var duplicate = NewCharacter(NewAccountId(), name.ToUpperInvariant());

        var result = await Store.CreateAsync(duplicate);

        Assert.Equal(CharacterRepository.NameTaken, result.ErrorCode);
        Assert.Null(await Store.GetAsync(duplicate.Id));
    }

    [SkippableFact]
    public async Task ListByAccount_ReturnsOnlyThatAccountsCharacters()
    {
        SkipIfUnavailable();
        var accountId = NewAccountId();
        var first = NewCharacter(accountId, UniqueName("Astrid"));
        var second = NewCharacter(accountId, UniqueName("Erik"));
        await Store.CreateAsync(first);
        await Store.CreateAsync(second);
        await Store.CreateAsync(NewCharacter(NewAccountId(), UniqueName("Sigrid")));

        var characters = await Store.ListByAccountAsync(accountId);

        Assert.Equal(new[] { first.Id, second.Id }.Order(StringComparer.Ordinal), characters.Select(c => c.Id).Order(StringComparer.Ordinal));
    }

    [SkippableFact]
    public async Task ListByAccount_WithNoCharacters_IsEmpty()
    {
        SkipIfUnavailable();

        Assert.Empty(await Store.ListByAccountAsync(NewAccountId()));
    }
}
