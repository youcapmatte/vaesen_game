using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests.Persistence;

/// <summary>
/// Behaviour the stores must have. Runs against the in-memory fake and the real repositories (DynamoDB Local),
/// so the fake used by endpoint tests can't drift from the real thing.
/// </summary>
public abstract class StoreContract
{
    protected abstract IAccountStore Accounts { get; }

    protected abstract IGuildStore Guilds { get; }

    protected abstract ICharacterStore Characters { get; }

    protected virtual void SkipIfUnavailable()
    {
    }

    /// <summary>Unique values are unique per table, so tests sharing a database need their own.</summary>
    private static string Unique(string name, int length = 24) => $"{name}{Guid.NewGuid():N}"[..length];

    private static Account NewAccount(string? auth0UserId = null) => new()
    {
        Username = Unique("user_"),
        Email = "player@example.com",
        Auth0UserId = auth0UserId,
        GuildIds = [],
        Subscription = new AccountSubscription
        {
            Status = SubscriptionStatus.Active,
            Plan = "monthly",
            CurrentPeriodEnd = new DateTime(2030, 1, 1, 0, 0, 0, DateTimeKind.Utc),
        },
    };

    private async Task<(Guild Guild, Account Founder)> CreateGuildAsync()
    {
        var founder = NewAccount();
        Assert.True((await Accounts.CreateAsync(founder)).Success);

        var guild = new Guild { Name = Unique("Guild "), CreatedBy = founder.Id, MemberAccountIds = [founder.Id] };
        founder.GuildIds.Add(guild.Id);
        Assert.True((await Guilds.CreateAsync(guild, founder)).Success);
        return (guild, founder);
    }

    // ---- Accounts -------------------------------------------------------------------------------------------

    [SkippableFact]
    public async Task Account_Create_ThenGetById_RoundTripsAllFields()
    {
        SkipIfUnavailable();
        var account = NewAccount($"auth0|{Guid.NewGuid():N}");

        var created = await Accounts.CreateAsync(account);
        var loaded = await Accounts.GetAsync(account.Id);

        Assert.True(created.Success);
        Assert.NotNull(loaded);
        Assert.Equal(account.Username, loaded.Username);
        Assert.Equal(account.Auth0UserId, loaded.Auth0UserId);
        Assert.Equal(SubscriptionStatus.Active, loaded.Subscription.Status);
        Assert.Equal(account.Subscription.CurrentPeriodEnd, loaded.Subscription.CurrentPeriodEnd);
        Assert.Equal(1, loaded.Version);
        Assert.NotEqual(default, loaded.CreatedAt);
    }

    [SkippableFact]
    public async Task Account_GetByUsername_IsCaseInsensitive()
    {
        SkipIfUnavailable();
        var account = NewAccount();
        await Accounts.CreateAsync(account);

        Assert.Equal(account.Id, (await Accounts.GetByUsernameAsync(account.Username.ToUpperInvariant()))?.Id);
    }

    [SkippableFact]
    public async Task Account_GetByAuth0UserId_FindsTheLinkedAccount()
    {
        SkipIfUnavailable();
        var auth0UserId = $"google-oauth2|{Guid.NewGuid():N}";
        var account = NewAccount(auth0UserId);
        await Accounts.CreateAsync(account);

        Assert.Equal(account.Id, (await Accounts.GetByAuth0UserIdAsync(auth0UserId))?.Id);
    }

    [SkippableFact]
    public async Task Account_CreateWithTakenUsername_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var first = NewAccount();
        await Accounts.CreateAsync(first);
        var duplicate = NewAccount();
        duplicate.Username = first.Username.ToLowerInvariant();

        var result = await Accounts.CreateAsync(duplicate);

        Assert.Equal(AccountRepository.UsernameTaken, result.ErrorCode);
        Assert.Null(await Accounts.GetAsync(duplicate.Id));
    }

    [SkippableFact]
    public async Task Account_CreateWithTakenAuth0UserId_ReportsItAndStoresNothing()
    {
        SkipIfUnavailable();
        var auth0UserId = $"auth0|{Guid.NewGuid():N}";
        await Accounts.CreateAsync(NewAccount(auth0UserId));
        var duplicate = NewAccount(auth0UserId);

        var result = await Accounts.CreateAsync(duplicate);

        Assert.Equal(AccountRepository.Auth0UserTaken, result.ErrorCode);
        Assert.Null(await Accounts.GetAsync(duplicate.Id));
        Assert.Null(await Accounts.GetByUsernameAsync(duplicate.Username));
    }

    // ---- Guilds ---------------------------------------------------------------------------------------------

    [SkippableFact]
    public async Task Guild_Create_StoresGuildAndFoundersMembership()
    {
        SkipIfUnavailable();
        var (guild, founder) = await CreateGuildAsync();

        var loadedGuild = await Guilds.GetAsync(guild.Id);
        var loadedFounder = await Accounts.GetAsync(founder.Id);

        Assert.Equal(guild.Name, loadedGuild?.Name);
        Assert.Equal([founder.Id], loadedGuild?.MemberAccountIds);
        Assert.Equal(founder.Id, loadedGuild?.CreatedBy);
        Assert.Equal([guild.Id], loadedFounder?.GuildIds);
        Assert.Equal(2, loadedFounder?.Version);
        Assert.Equal(guild.Id, (await Guilds.GetByNameAsync(guild.Name.ToUpperInvariant()))?.Id);
    }

    [SkippableFact]
    public async Task Guild_CreateWithTakenName_StoresNothing()
    {
        SkipIfUnavailable();
        var (existing, _) = await CreateGuildAsync();
        var founder = NewAccount();
        await Accounts.CreateAsync(founder);
        var duplicate = new Guild { Name = existing.Name.ToLowerInvariant(), MemberAccountIds = [founder.Id] };
        founder.GuildIds.Add(duplicate.Id);

        var result = await Guilds.CreateAsync(duplicate, founder);

        Assert.Equal(GuildRepository.NameTaken, result.ErrorCode);
        Assert.Null(await Guilds.GetAsync(duplicate.Id));
        Assert.Empty((await Accounts.GetAsync(founder.Id))!.GuildIds);
    }

    [SkippableFact]
    public async Task Guild_SaveBasedOnAnOldVersion_IsRejected()
    {
        SkipIfUnavailable();
        var (guild, _) = await CreateGuildAsync();
        var first = (await Guilds.GetAsync(guild.Id))!;
        var second = (await Guilds.GetAsync(guild.Id))!;

        first.Name = first.Name + "!";
        Assert.True((await Guilds.SaveAsync(first, [])).Success);
        var stale = await Guilds.SaveAsync(second, []);

        Assert.Equal(ErrorCodes.Conflict, stale.ErrorCode);
    }

    [SkippableFact]
    public async Task Guild_SaveWithAccount_FailsWhollyWhenTheAccountChanged()
    {
        SkipIfUnavailable();
        var (guild, _) = await CreateGuildAsync();
        var member = NewAccount();
        await Accounts.CreateAsync(member);
        var staleMember = (await Accounts.GetAsync(member.Id))!;
        var loadedGuild = (await Guilds.GetAsync(guild.Id))!;

        // Someone else changes the account first.
        var concurrent = (await Accounts.GetAsync(member.Id))!;
        concurrent.GuildIds.Add("guild-00000000-0000-4000-8000-000000000000");
        Assert.True((await Guilds.SaveAsync((await Guilds.GetAsync(guild.Id))!, [concurrent])).Success);

        loadedGuild.MemberAccountIds.Add(staleMember.Id);
        staleMember.GuildIds.Add(guild.Id);
        var result = await Guilds.SaveAsync(loadedGuild, [staleMember]);

        Assert.Equal(ErrorCodes.Conflict, result.ErrorCode);
        Assert.DoesNotContain(member.Id, (await Guilds.GetAsync(guild.Id))!.MemberAccountIds);
    }

    // ---- Characters -----------------------------------------------------------------------------------------

    [SkippableFact]
    public async Task Character_Create_StoresCharacterAndGuildsList()
    {
        SkipIfUnavailable();
        var (guild, founder) = await CreateGuildAsync();
        var loadedGuild = (await Guilds.GetAsync(guild.Id))!;
        var character = new Character { GuildId = guild.Id, Name = Unique("Ingrid", 16), CreatedBy = founder.Id };
        loadedGuild.CharacterIds.Add(character.Id);

        Assert.True((await Guilds.CreateCharacterAsync(loadedGuild, character)).Success);

        Assert.Equal(character.Name, (await Characters.GetAsync(character.Id))?.Name);
        Assert.Equal([character.Id], (await Guilds.GetAsync(guild.Id))?.CharacterIds);
        Assert.Equal([character.Id], (await Characters.ListByGuildAsync(guild.Id)).Select(c => c.Id));
        Assert.Equal([character.Id], (await Characters.GetManyAsync([character.Id])).Select(c => c.Id));
    }

    [SkippableFact]
    public async Task Character_CreateWithTakenName_StoresNothing()
    {
        SkipIfUnavailable();
        var (guild, _) = await CreateGuildAsync();
        var name = Unique("Johan", 16);
        var first = (await Guilds.GetAsync(guild.Id))!;
        var character = new Character { GuildId = guild.Id, Name = name };
        first.CharacterIds.Add(character.Id);
        await Guilds.CreateCharacterAsync(first, character);

        var second = (await Guilds.GetAsync(guild.Id))!;
        var duplicate = new Character { GuildId = guild.Id, Name = name.ToUpperInvariant() };
        second.CharacterIds.Add(duplicate.Id);
        var result = await Guilds.CreateCharacterAsync(second, duplicate);

        Assert.Equal(CharacterRepository.NameTaken, result.ErrorCode);
        Assert.Null(await Characters.GetAsync(duplicate.Id));
        Assert.Equal([character.Id], (await Guilds.GetAsync(guild.Id))?.CharacterIds);
    }

    private async Task<(Guild Guild, Character Character)> CreateCharacterAsync(string? name = null)
    {
        var (guild, founder) = await CreateGuildAsync();
        var loadedGuild = (await Guilds.GetAsync(guild.Id))!;
        var character = new Character
        {
            GuildId = guild.Id,
            Name = name ?? Unique("Astrid", 16),
            FirstName = "Astrid",
            LastName = "Falk",
            Class = "Occultist",
            HealthPoints = 42,
            CreatedBy = founder.Id,
        };
        loadedGuild.CharacterIds.Add(character.Id);
        Assert.True((await Guilds.CreateCharacterAsync(loadedGuild, character)).Success);
        return (loadedGuild, character);
    }

    [SkippableFact]
    public async Task Character_RoundTripsAllFields_AndIsFoundByName()
    {
        SkipIfUnavailable();
        var (_, character) = await CreateCharacterAsync();

        var loaded = await Characters.GetByNameAsync(character.Name.ToUpperInvariant());

        Assert.NotNull(loaded);
        Assert.Equal(character.Id, loaded.Id);
        Assert.Equal("Astrid", loaded.FirstName);
        Assert.Equal("Falk", loaded.LastName);
        Assert.Equal("Occultist", loaded.Class);
        Assert.Equal(42, loaded.HealthPoints);
    }

    [SkippableFact]
    public async Task Character_Update_SavesChanges_AndRejectsStaleVersions()
    {
        SkipIfUnavailable();
        var (_, character) = await CreateCharacterAsync();
        var first = (await Characters.GetAsync(character.Id))!;
        var stale = (await Characters.GetAsync(character.Id))!;

        first.HealthPoints = 7;
        Assert.True((await Characters.UpdateAsync(first, first.Name)).Success);
        stale.Class = "Doctor";
        var conflict = await Characters.UpdateAsync(stale, stale.Name);

        Assert.Equal(ErrorCodes.Conflict, conflict.ErrorCode);
        var loaded = (await Characters.GetAsync(character.Id))!;
        Assert.Equal(7, loaded.HealthPoints);
        Assert.Equal("Occultist", loaded.Class);
        Assert.Equal(2, loaded.Version);
    }

    [SkippableFact]
    public async Task Character_Rename_MovesTheNameReservation()
    {
        SkipIfUnavailable();
        var (_, character) = await CreateCharacterAsync();
        var loaded = (await Characters.GetAsync(character.Id))!;
        var oldName = loaded.Name;
        loaded.Name = Unique("Renamed", 16);

        Assert.True((await Characters.UpdateAsync(loaded, oldName)).Success);

        Assert.Equal(character.Id, (await Characters.GetByNameAsync(loaded.Name))?.Id);
        Assert.Null(await Characters.GetByNameAsync(oldName));
    }

    [SkippableFact]
    public async Task Character_RenameToATakenName_ChangesNothing()
    {
        SkipIfUnavailable();
        var (_, taken) = await CreateCharacterAsync();
        var (_, character) = await CreateCharacterAsync();
        var loaded = (await Characters.GetAsync(character.Id))!;
        var oldName = loaded.Name;
        loaded.Name = taken.Name.ToUpperInvariant();

        var result = await Characters.UpdateAsync(loaded, oldName);

        Assert.Equal(CharacterRepository.NameTaken, result.ErrorCode);
        Assert.Equal(oldName, (await Characters.GetAsync(character.Id))?.Name);
        Assert.Equal(character.Id, (await Characters.GetByNameAsync(oldName))?.Id);
    }

    [SkippableFact]
    public async Task Character_Delete_RemovesItFromGuild_AndFreesTheName()
    {
        SkipIfUnavailable();
        var (guild, character) = await CreateCharacterAsync();
        var loadedGuild = (await Guilds.GetAsync(guild.Id))!;
        var loadedCharacter = (await Characters.GetAsync(character.Id))!;
        loadedGuild.CharacterIds.Remove(character.Id);

        Assert.True((await Guilds.DeleteCharacterAsync(loadedGuild, loadedCharacter)).Success);

        Assert.Null(await Characters.GetAsync(character.Id));
        Assert.Null(await Characters.GetByNameAsync(character.Name));
        Assert.Empty((await Guilds.GetAsync(guild.Id))!.CharacterIds);

        // The name can be used again.
        var reuseGuild = (await Guilds.GetAsync(guild.Id))!;
        var reused = new Character { GuildId = guild.Id, Name = character.Name };
        reuseGuild.CharacterIds.Add(reused.Id);
        Assert.True((await Guilds.CreateCharacterAsync(reuseGuild, reused)).Success);
    }

    [SkippableFact]
    public async Task Lookups_ForUnknownValues_ReturnNull()
    {
        SkipIfUnavailable();

        Assert.Null(await Accounts.GetAsync(new Account().Id));
        Assert.Null(await Accounts.GetByUsernameAsync(Unique("nobody")));
        Assert.Null(await Guilds.GetAsync(new Guild().Id));
        Assert.Null(await Guilds.GetByNameAsync(Unique("No guild ")));
        Assert.Null(await Characters.GetAsync(new Character().Id));
        Assert.Null(await Characters.GetByNameAsync(Unique("Nobody", 16)));
        Assert.Empty(await Characters.ListByGuildAsync(new Guild().Id));
    }
}
