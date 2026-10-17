using Newtonsoft.Json;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests.Fakes;

/// <summary>
/// In-memory stand-in for the DynamoDB repositories, with the same rules: reads return copies, writes check versions
/// and unique values, and multi-item writes are all or nothing. Register one instance for all store interfaces.
/// </summary>
public sealed class InMemoryDatabase : IAccountStore, ICharacterStore, IGuildStore
{
    private readonly Dictionary<string, BaseCore> _items = new(StringComparer.Ordinal);

    /// <summary>"{kind}#{lowercase value}" -> owner id.</summary>
    private readonly Dictionary<string, string> _reservations = new(StringComparer.Ordinal);

    private readonly Lock _lock = new();

    // ---- Accounts -------------------------------------------------------------------------------------------

    public Task<Account?> GetAsync(string accountId, CancellationToken cancellationToken = default) => Task.FromResult(Read<Account>(accountId));

    public Task<Account?> GetByUsernameAsync(string username, CancellationToken cancellationToken = default) =>
        Task.FromResult(Read<Account>(Owner("USERNAME", username.Trim().ToLowerInvariant())));

    public Task<Account?> GetByAuth0UserIdAsync(string auth0UserId, CancellationToken cancellationToken = default) =>
        Task.FromResult(Read<Account>(Owner("AUTH0", auth0UserId)));

    public Task<ItemAction<Account>> CreateAsync(Account account, CancellationToken cancellationToken = default)
    {
        var reservations = new List<(string, string, string, string)>
        {
            ("USERNAME", account.Username.Trim().ToLowerInvariant(), "Username is taken.", AccountRepository.UsernameTaken),
        };
        if (account.Auth0UserId is not null)
        {
            reservations.Add(("AUTH0", account.Auth0UserId, "This Auth0 user already has an account.", AccountRepository.Auth0UserTaken));
        }

        var result = Commit([Create(account)], reservations.Select(r => (r.Item1, r.Item2, account.Id, r.Item3, r.Item4)).ToList());
        var ret = new ItemAction<Account>();
        if (result.Success)
        {
            ret.Ok(account);
        }
        else
        {
            ret.Fail(result.ErrorMessage, result.ErrorCode);
        }

        return Task.FromResult(ret);
    }

    // ---- Characters -----------------------------------------------------------------------------------------

    Task<Character?> ICharacterStore.GetAsync(string characterId, CancellationToken cancellationToken) => Task.FromResult(Read<Character>(characterId));

    Task<Character?> ICharacterStore.GetByNameAsync(string name, CancellationToken cancellationToken) =>
        Task.FromResult(Read<Character>(Owner("CHARNAME", name.Trim().ToLowerInvariant())));

    public Task<ResultBase> UpdateAsync(Character character, CancellationToken cancellationToken = default) =>
        Task.FromResult(Commit([Update(character)], []));

    public Task<IReadOnlyList<Character>> GetManyAsync(IReadOnlyCollection<string> characterIds, CancellationToken cancellationToken = default) =>
        Task.FromResult<IReadOnlyList<Character>>(characterIds.Select(Read<Character>).OfType<Character>().ToList());

    public Task<IReadOnlyList<Character>> ListByGuildAsync(string guildId, CancellationToken cancellationToken = default)
    {
        lock (_lock)
        {
            IReadOnlyList<Character> characters = _items.Values.OfType<Character>()
                .Where(c => string.Equals(c.GuildId, guildId, StringComparison.Ordinal) && !c.IsDeleted)
                .OrderBy(c => c.Name, StringComparer.Ordinal)
                .Select(Clone)
                .ToList();
            return Task.FromResult(characters);
        }
    }

    // ---- Guilds ---------------------------------------------------------------------------------------------

    Task<Guild?> IGuildStore.GetAsync(string guildId, CancellationToken cancellationToken) => Task.FromResult(Read<Guild>(guildId));

    public Task<Guild?> GetByNameAsync(string name, CancellationToken cancellationToken = default) =>
        Task.FromResult(Read<Guild>(Owner("GUILDNAME", name.Trim().ToLowerInvariant())));

    Task<IReadOnlyList<Guild>> IGuildStore.GetManyAsync(IReadOnlyCollection<string> guildIds, CancellationToken cancellationToken) =>
        Task.FromResult<IReadOnlyList<Guild>>(guildIds.Select(Read<Guild>).OfType<Guild>().ToList());

    public Task<ResultBase> CreateAsync(Guild guild, Account founder, CancellationToken cancellationToken = default) =>
        Task.FromResult(Commit(
            [Create(guild), Update(founder)],
            [("GUILDNAME", guild.Name.Trim().ToLowerInvariant(), guild.Id, "That guild name is taken.", GuildRepository.NameTaken)]));

    public Task<ResultBase> SaveAsync(Guild guild, IReadOnlyList<Account> changedAccounts, CancellationToken cancellationToken = default) =>
        Task.FromResult(Commit([Update(guild), .. changedAccounts.Select(Update)], []));

    public Task<ResultBase> CreateCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default) =>
        Task.FromResult(Commit(
            [Create(character), Update(guild)],
            [("CHARNAME", character.Name.Trim().ToLowerInvariant(), character.Id, "That character name is taken.", CharacterRepository.NameTaken)]));

    public Task<ResultBase> DeleteCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default)
    {
        character.IsDeleted = true;
        return Task.FromResult(Commit(
            [Update(character), Update(guild)],
            [],
            releases: [("CHARNAME", character.Name.Trim().ToLowerInvariant(), character.Id)]));
    }

    // ---- Storage --------------------------------------------------------------------------------------------

    private sealed record Write(BaseCore Item, bool IsCreate, int ExpectedVersion);

    private static Write Create(BaseCore item) => new(item, IsCreate: true, ExpectedVersion: 0);

    private static Write Update(BaseCore item) => new(item, IsCreate: false, ExpectedVersion: item.Version);

    /// <summary>Applies all writes and reservations, or none. Same error codes as the DynamoDB repositories.</summary>
    private ResultBase Commit(
        IReadOnlyList<Write> writes,
        IReadOnlyList<(string Kind, string Value, string OwnerId, string Message, string Code)> reservations,
        IReadOnlyList<(string Kind, string Value, string OwnerId)>? releases = null)
    {
        var ret = new ResultBase();
        lock (_lock)
        {
            foreach (var write in writes)
            {
                _items.TryGetValue(write.Item.Id, out var stored);
                if (write.IsCreate && stored is not null)
                {
                    return ret.Fail("An item with this id already exists.", ErrorCodes.Conflict);
                }

                if (!write.IsCreate && (stored is null || stored.Version != write.ExpectedVersion))
                {
                    return ret.Fail("The item was changed or removed by someone else. Reload and try again.", ErrorCodes.Conflict);
                }
            }

            foreach (var reservation in reservations)
            {
                if (_reservations.ContainsKey($"{reservation.Kind}#{reservation.Value}"))
                {
                    return ret.Fail(reservation.Message, reservation.Code);
                }
            }

            foreach (var release in releases ?? [])
            {
                if (!string.Equals(_reservations.GetValueOrDefault($"{release.Kind}#{release.Value}"), release.OwnerId, StringComparison.Ordinal))
                {
                    return ret.Fail("The reservation is held by someone else.", ErrorCodes.Conflict);
                }
            }

            var now = DateTime.UtcNow;
            foreach (var write in writes)
            {
                if (write.IsCreate)
                {
                    write.Item.CreatedAt = now;
                    write.Item.Version = 0;
                }

                write.Item.UpdatedAt = now;
                write.Item.Version++;
                _items[write.Item.Id] = Clone(write.Item);
            }

            foreach (var reservation in reservations)
            {
                _reservations[$"{reservation.Kind}#{reservation.Value}"] = reservation.OwnerId;
            }

            foreach (var release in releases ?? [])
            {
                _reservations.Remove($"{release.Kind}#{release.Value}");
            }
        }

        return ret.Ok();
    }

    private T? Read<T>(string? id) where T : BaseCore
    {
        if (id is null)
        {
            return null;
        }

        lock (_lock)
        {
            return _items.TryGetValue(id, out var item) && item is T typed && !typed.IsDeleted ? Clone(typed) : null;
        }
    }

    private string? Owner(string kind, string value)
    {
        lock (_lock)
        {
            return _reservations.GetValueOrDefault($"{kind}#{value}");
        }
    }

    private static T Clone<T>(T item) where T : BaseCore =>
        (T)JsonConvert.DeserializeObject(JsonConvert.SerializeObject(item), item.GetType())!;
}
