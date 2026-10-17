using Vaesen.Api.Modules.Players;

namespace Vaesen.Api.Tests.Persistence;

/// <summary>
/// Behaviour every ICharacterRepository must have. Run against the in-memory and the DynamoDB implementation,
/// so the fake used by endpoint tests can't drift from the real thing.
/// </summary>
public abstract class CharacterRepositoryContract
{
    protected abstract ICharacterRepository Repository { get; }

    private static Character NewCharacter(string accountId, string name) =>
        new(Guid.NewGuid().ToString("N"), accountId, name, null, 1);

    private static string NewAccountId() => $"auth0|{Guid.NewGuid():N}";

    /// <summary>Names are unique per table, so tests sharing a database need their own.</summary>
    private static string UniqueName(string name) => $"{name}{Guid.NewGuid():N}"[..20];

    protected virtual void SkipIfUnavailable()
    {
    }

    [SkippableFact]
    public async Task Create_ThenGet_ReturnsTheCharacter()
    {
        SkipIfUnavailable();
        var character = NewCharacter(NewAccountId(), UniqueName("Ingrid"));

        Assert.True(await Repository.TryCreateAsync(character, CancellationToken.None));

        Assert.Equal(character, await Repository.GetAsync(character.Id, CancellationToken.None));
    }

    [SkippableFact]
    public async Task Get_Unknown_ReturnsNull()
    {
        SkipIfUnavailable();

        Assert.Null(await Repository.GetAsync("does-not-exist", CancellationToken.None));
    }

    [SkippableFact]
    public async Task Create_WithTakenName_DifferentCase_Fails()
    {
        SkipIfUnavailable();
        var name = UniqueName("Johan");
        Assert.True(await Repository.TryCreateAsync(NewCharacter(NewAccountId(), name), CancellationToken.None));

        var duplicate = NewCharacter(NewAccountId(), name.ToUpperInvariant());

        Assert.False(await Repository.TryCreateAsync(duplicate, CancellationToken.None));
        Assert.Null(await Repository.GetAsync(duplicate.Id, CancellationToken.None));
    }

    [SkippableFact]
    public async Task ListByAccount_ReturnsOnlyThatAccountsCharacters()
    {
        SkipIfUnavailable();
        var accountId = NewAccountId();
        var first = NewCharacter(accountId, UniqueName("Astrid"));
        var second = NewCharacter(accountId, UniqueName("Erik"));
        await Repository.TryCreateAsync(first, CancellationToken.None);
        await Repository.TryCreateAsync(second, CancellationToken.None);
        await Repository.TryCreateAsync(NewCharacter(NewAccountId(), UniqueName("Sigrid")), CancellationToken.None);

        var characters = await Repository.ListByAccountAsync(accountId, CancellationToken.None);

        Assert.Equal(new[] { first.Id, second.Id }.Order(), characters.Select(c => c.Id).Order());
    }

    [SkippableFact]
    public async Task ListByAccount_WithNoCharacters_IsEmpty()
    {
        SkipIfUnavailable();

        Assert.Empty(await Repository.ListByAccountAsync(NewAccountId(), CancellationToken.None));
    }
}
