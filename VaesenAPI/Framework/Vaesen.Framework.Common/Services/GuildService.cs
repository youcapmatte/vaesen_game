using System.Text.RegularExpressions;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Services;

public interface IGuildService
{
    /// <summary>Creates a guild with its founder as the first member and guild master.</summary>
    Task<ResultBase<Guild>> CreateAsync(CreateGuildRequest request, CancellationToken cancellationToken = default);

    Task<ResultBase<Guild>> GetAsync(string guildId, CancellationToken cancellationToken = default);

    Task<ResultBase<Guild>> GetByNameAsync(string name, CancellationToken cancellationToken = default);

    /// <summary>The guilds an account is a member of.</summary>
    Task<ResultBase<IReadOnlyList<Guild>>> ListForAccountAsync(string accountId, CancellationToken cancellationToken = default);

    Task<ResultBase<Guild>> AddMemberAsync(string guildId, string accountId, CancellationToken cancellationToken = default);

    /// <summary>Removes the member and frees the character they were playing. The guild master can't be removed.</summary>
    Task<ResultBase<Guild>> RemoveMemberAsync(string guildId, string accountId, CancellationToken cancellationToken = default);

    Task<ResultBase<IReadOnlyList<Character>>> ListCharactersAsync(string guildId, CancellationToken cancellationToken = default);

    Task<ResultBase<Character>> CreateCharacterAsync(string guildId, CreateCharacterRequest request, CancellationToken cancellationToken = default);

    /// <summary>
    /// Makes a member the player of one of the guild's characters. Whoever played that character before loses it,
    /// and the member stops playing any other character in the guild.
    /// </summary>
    Task<ResultBase<Guild>> AssignPlayerAsync(string guildId, string characterId, AssignPlayerRequest request, CancellationToken cancellationToken = default);

    /// <summary>Frees a character: nobody plays it.</summary>
    Task<ResultBase<Guild>> ReleasePlayerAsync(string guildId, string characterId, CancellationToken cancellationToken = default);
}

public sealed partial class GuildService : IGuildService
{
    public const string NotAMember = "not_a_member";
    public const string CharacterNotInGuild = "character_not_in_guild";
    public const string GuildMasterCannotLeave = "guild_master_cannot_leave";

    private readonly IGuildStore _guilds;
    private readonly IAccountStore _accounts;
    private readonly ICharacterStore _characters;
    private readonly TimeProvider _timeProvider;
    private readonly IOptions<CharacterOptions> _characterOptions;

    public GuildService(
        IGuildStore guilds,
        IAccountStore accounts,
        ICharacterStore characters,
        TimeProvider timeProvider,
        IOptions<CharacterOptions> characterOptions)
    {
        _guilds = guilds;
        _accounts = accounts;
        _characters = characters;
        _timeProvider = timeProvider;
        _characterOptions = characterOptions;
    }

    public async Task<ResultBase<Guild>> CreateAsync(CreateGuildRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Guild>();
        var name = (request?.Name ?? "").Trim();
        if (!GuildNamePattern().IsMatch(name))
        {
            return ret.Fail("Guild name must be 3-32 characters: letters, digits, spaces, apostrophes or hyphens.", ErrorCodes.Invalid);
        }

        var founder = await FindAccountAsync(request?.CreatedByAccountId, cancellationToken).ConfigureAwait(false);
        if (founder is null)
        {
            return ret.Fail("The founding account doesn't exist.", ErrorCodes.NotFound);
        }

        var guild = new Guild
        {
            Name = name,
            CreatedBy = founder.Id,
            UpdatedBy = founder.Id,
            GuildMasterAccountId = founder.Id,
            MemberAccountIds = [founder.Id],
        };
        founder.GuildIds.Add(guild.Id);

        var saved = await _guilds.CreateAsync(guild, founder, cancellationToken).ConfigureAwait(false);
        return saved.Success ? ret.Ok(guild) : ret.Fail(saved.ErrorMessage, saved.ErrorCode);
    }

    public async Task<ResultBase<Guild>> GetAsync(string guildId, CancellationToken cancellationToken = default)
    {
        if (!Guild.IsValidId<Guild>(guildId))
        {
            return new ResultBase<Guild>().Fail("Not a guild id (expected 'guild-{guid}').", ErrorCodes.Invalid);
        }

        return Found(await _guilds.GetAsync(guildId, cancellationToken).ConfigureAwait(false));
    }

    public async Task<ResultBase<Guild>> GetByNameAsync(string name, CancellationToken cancellationToken = default) =>
        Found(await _guilds.GetByNameAsync(name, cancellationToken).ConfigureAwait(false));

    public async Task<ResultBase<IReadOnlyList<Guild>>> ListForAccountAsync(string accountId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<Guild>>();
        var account = await FindAccountAsync(accountId, cancellationToken).ConfigureAwait(false);
        if (account is null)
        {
            return ret.Fail("Account not found.", ErrorCodes.NotFound);
        }

        var guilds = await _guilds.GetManyAsync(account.GuildIds, cancellationToken).ConfigureAwait(false);
        return ret.Ok(guilds.OrderBy(g => g.Name, StringComparer.OrdinalIgnoreCase).ToList());
    }

    public async Task<ResultBase<Guild>> AddMemberAsync(string guildId, string accountId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Guild>();
        var (guild, account, error) = await LoadGuildAndAccountAsync(guildId, accountId, cancellationToken).ConfigureAwait(false);
        if (error is not null)
        {
            return ret.Fail(error.ErrorMessage, error.ErrorCode);
        }

        if (guild!.IsMember(account!.Id))
        {
            return ret.Ok(guild);
        }

        guild.MemberAccountIds.Add(account.Id);
        guild.UpdatedBy = account.Id;
        if (!account.GuildIds.Contains(guild.Id, StringComparer.Ordinal))
        {
            account.GuildIds.Add(guild.Id);
        }

        return await SaveAsync(guild, [account], cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<Guild>> RemoveMemberAsync(string guildId, string accountId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Guild>();
        var (guild, account, error) = await LoadGuildAndAccountAsync(guildId, accountId, cancellationToken).ConfigureAwait(false);
        if (error is not null)
        {
            return ret.Fail(error.ErrorMessage, error.ErrorCode);
        }

        if (!guild!.IsMember(account!.Id))
        {
            return ret.Fail("The account isn't a member of this guild.", NotAMember);
        }

        if (guild.IsGuildMaster(account.Id))
        {
            return ret.Fail("The guild master can't leave the guild.", GuildMasterCannotLeave);
        }

        guild.MemberAccountIds.RemoveAll(id => string.Equals(id, account.Id, StringComparison.Ordinal));
        guild.CharacterAssignments.RemoveAll(a => string.Equals(a.AccountId, account.Id, StringComparison.Ordinal));
        account.GuildIds.RemoveAll(id => string.Equals(id, guild.Id, StringComparison.Ordinal));

        return await SaveAsync(guild, [account], cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<IReadOnlyList<Character>>> ListCharactersAsync(string guildId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<Character>>();
        var guild = await GetAsync(guildId, cancellationToken).ConfigureAwait(false);
        if (!guild.Success)
        {
            return ret.Fail(guild.ErrorMessage, guild.ErrorCode);
        }

        var characters = await _characters.GetManyAsync(guild.Result!.CharacterIds, cancellationToken).ConfigureAwait(false);
        return ret.Ok(characters.OrderBy(c => c.Name, StringComparer.OrdinalIgnoreCase).ToList());
    }

    public async Task<ResultBase<Character>> CreateCharacterAsync(string guildId, CreateCharacterRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Character>();
        var firstName = CharacterRules.Clean(request?.FirstName);
        var lastName = CharacterRules.Clean(request?.LastName);
        var characterClass = CharacterRules.Clean(request?.Class);
        var invalid = CharacterRules.ValidatePersonName(firstName, "First name")
            ?? CharacterRules.ValidatePersonName(lastName, "Last name")
            ?? CharacterRules.ValidateClass(characterClass, _characterOptions.Value.EffectiveClasses)
            ?? CharacterRules.ValidateHealthPoints(request?.HealthPoints);
        if (invalid is not null)
        {
            return ret.Fail(invalid, ErrorCodes.Invalid);
        }

        var (guild, creator, error) = await LoadGuildAndAccountAsync(guildId, request?.CreatedByAccountId, cancellationToken).ConfigureAwait(false);
        if (error is not null)
        {
            return ret.Fail(error.ErrorMessage, error.ErrorCode);
        }

        if (!guild!.IsMember(creator!.Id))
        {
            return ret.Fail("Only guild members can create the guild's characters.", NotAMember);
        }

        var character = new Character
        {
            GuildId = guild.Id,
            Name = Character.FullName(firstName!, lastName!),
            FirstName = firstName!,
            LastName = lastName!,
            Class = CharacterRules.NormalizeClass(characterClass, _characterOptions.Value.EffectiveClasses),
            HealthPoints = request?.HealthPoints ?? Character.DefaultHealthPoints,
            CreatedBy = creator.Id,
            UpdatedBy = creator.Id,
        };
        guild.CharacterIds.Add(character.Id);
        guild.UpdatedBy = creator.Id;

        var saved = await _guilds.CreateCharacterAsync(guild, character, cancellationToken).ConfigureAwait(false);
        return saved.Success ? ret.Ok(character) : ret.Fail(saved.ErrorMessage, saved.ErrorCode);
    }

    public async Task<ResultBase<Guild>> AssignPlayerAsync(string guildId, string characterId, AssignPlayerRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Guild>();
        var (guild, account, error) = await LoadGuildAndAccountAsync(guildId, request?.AccountId, cancellationToken).ConfigureAwait(false);
        if (error is not null)
        {
            return ret.Fail(error.ErrorMessage, error.ErrorCode);
        }

        if (!guild!.OwnsCharacter(characterId))
        {
            return ret.Fail("The character doesn't belong to this guild.", CharacterNotInGuild);
        }

        if (!guild.IsMember(account!.Id))
        {
            return ret.Fail("Only guild members can play the guild's characters.", NotAMember);
        }

        if (string.Equals(guild.GetPlayerOf(characterId), account.Id, StringComparison.Ordinal))
        {
            return ret.Ok(guild);
        }

        // One player per character, one character per player.
        guild.CharacterAssignments.RemoveAll(a =>
            string.Equals(a.CharacterId, characterId, StringComparison.Ordinal)
            || string.Equals(a.AccountId, account.Id, StringComparison.Ordinal));
        guild.CharacterAssignments.Add(new CharacterAssignment
        {
            CharacterId = characterId,
            AccountId = account.Id,
            AssignedAt = _timeProvider.GetUtcNow().UtcDateTime,
        });
        guild.UpdatedBy = account.Id;

        return await SaveAsync(guild, [], cancellationToken).ConfigureAwait(false);
    }

    public async Task<ResultBase<Guild>> ReleasePlayerAsync(string guildId, string characterId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Guild>();
        var loaded = await GetAsync(guildId, cancellationToken).ConfigureAwait(false);
        if (!loaded.Success)
        {
            return loaded;
        }

        var guild = loaded.Result!;
        if (!guild.OwnsCharacter(characterId))
        {
            return ret.Fail("The character doesn't belong to this guild.", CharacterNotInGuild);
        }

        if (guild.CharacterAssignments.RemoveAll(a => string.Equals(a.CharacterId, characterId, StringComparison.Ordinal)) == 0)
        {
            return ret.Ok(guild);
        }

        return await SaveAsync(guild, [], cancellationToken).ConfigureAwait(false);
    }

    private async Task<ResultBase<Guild>> SaveAsync(Guild guild, IReadOnlyList<Account> changedAccounts, CancellationToken cancellationToken)
    {
        var saved = await _guilds.SaveAsync(guild, changedAccounts, cancellationToken).ConfigureAwait(false);
        return saved.Success ? new ResultBase<Guild>().Ok(guild) : new ResultBase<Guild>().Fail(saved.ErrorMessage, saved.ErrorCode);
    }

    private async Task<Account?> FindAccountAsync(string? accountId, CancellationToken cancellationToken) =>
        Account.IsValidId<Account>(accountId) ? await _accounts.GetAsync(accountId!, cancellationToken).ConfigureAwait(false) : null;

    private async Task<(Guild? Guild, Account? Account, ResultBase? Error)> LoadGuildAndAccountAsync(
        string guildId, string? accountId, CancellationToken cancellationToken)
    {
        var guild = await GetAsync(guildId, cancellationToken).ConfigureAwait(false);
        if (!guild.Success)
        {
            return (null, null, guild);
        }

        var account = await FindAccountAsync(accountId, cancellationToken).ConfigureAwait(false);
        return account is null
            ? (null, null, new ResultBase().Fail("Account not found.", ErrorCodes.NotFound))
            : (guild.Result, account, null);
    }

    private static ResultBase<Guild> Found(Guild? guild) =>
        guild is null
            ? new ResultBase<Guild>().Fail("Guild not found.", ErrorCodes.NotFound)
            : new ResultBase<Guild>().Ok(guild);

    [GeneratedRegex(@"^[\p{L}\p{N}' -]{3,32}$")]
    private static partial Regex GuildNamePattern();
}
