using Amazon.DynamoDBv2;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Repositorys;

/// <summary>
/// Persistence for <see cref="Guild"/>. Every write keeps the guild and the accounts/characters it points to in step:
/// they're stored in one transaction, and a concurrent change to any of them fails the whole write with "conflict".
/// </summary>
public interface IGuildStore
{
    Task<Guild?> GetAsync(string guildId, CancellationToken cancellationToken = default);

    /// <summary>Case-insensitive.</summary>
    Task<Guild?> GetByNameAsync(string name, CancellationToken cancellationToken = default);

    Task<IReadOnlyList<Guild>> GetManyAsync(IReadOnlyCollection<string> guildIds, CancellationToken cancellationToken = default);

    /// <summary>
    /// Creates the guild (reserving its name) and saves the founding account, which the caller has already added the
    /// guild to. Fails with <see cref="GuildRepository.NameTaken"/> or <see cref="ErrorCodes.Conflict"/>.
    /// </summary>
    Task<ResultBase> CreateAsync(Guild guild, Account founder, CancellationToken cancellationToken = default);

    /// <summary>Saves the guild and the accounts whose membership changed with it.</summary>
    Task<ResultBase> SaveAsync(Guild guild, IReadOnlyList<Account> changedAccounts, CancellationToken cancellationToken = default);

    /// <summary>
    /// Creates the character (reserving its name) and saves the guild, which the caller has already added the
    /// character to. Fails with <see cref="CharacterRepository.NameTaken"/> or <see cref="ErrorCodes.Conflict"/>.
    /// </summary>
    Task<ResultBase> CreateCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default);

    /// <summary>
    /// Deletes the character (soft delete) and frees its name, saving the guild, which the caller has already removed
    /// the character from. Fails with <see cref="ErrorCodes.Conflict"/> if either changed in the meantime.
    /// </summary>
    Task<ResultBase> DeleteCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default);
}

/// <summary>
/// Guilds in the Vaesen table:
///   {guildId} / ITEM                        the guild (members, characters, who plays what)
///   GUILDNAME#{lowercase name} / GUILDNAME  name reservation, so names are unique
/// </summary>
public sealed class GuildRepository : BaseRepository<Guild>, IGuildStore
{
    public const string NameTaken = "guild_name_taken";

    private const string NameKind = "GUILDNAME";

    private readonly AccountRepository _accounts;
    private readonly CharacterRepository _characters;

    public GuildRepository(
        IAmazonDynamoDB dynamoDb,
        IOptions<DataServiceOptions> options,
        TimeProvider timeProvider,
        AccountRepository accounts,
        CharacterRepository characters)
        : base(dynamoDb, options, timeProvider)
    {
        _accounts = accounts;
        _characters = characters;
    }

    public static Reservation NameReservation(string name) =>
        new($"GUILDNAME#{name.Trim().ToLowerInvariant()}", NameKind, "That guild name is taken.", NameTaken);

    public Task<Guild?> GetAsync(string guildId, CancellationToken cancellationToken = default) =>
        GetItemAsync(guildId, cancellationToken);

    public async Task<Guild?> GetByNameAsync(string name, CancellationToken cancellationToken = default)
    {
        if (string.IsNullOrWhiteSpace(name))
        {
            return null;
        }

        var reservation = NameReservation(name);
        var guildId = await GetReservationOwnerAsync(reservation.Key, reservation.Kind, cancellationToken).ConfigureAwait(false);
        return guildId is null ? null : await GetItemAsync(guildId, cancellationToken).ConfigureAwait(false);
    }

    public Task<IReadOnlyList<Guild>> GetManyAsync(IReadOnlyCollection<string> guildIds, CancellationToken cancellationToken = default) =>
        GetItemsAsync(guildIds, cancellationToken);

    public Task<ResultBase> CreateAsync(Guild guild, Account founder, CancellationToken cancellationToken = default) =>
        DataTransaction.ExecuteAsync(DynamoDb,
        [
            PrepareCreate(guild),
            PrepareReservation(NameReservation(guild.Name), guild.Id),
            _accounts.PrepareUpdate(founder),
        ], cancellationToken);

    public Task<ResultBase> SaveAsync(Guild guild, IReadOnlyList<Account> changedAccounts, CancellationToken cancellationToken = default)
    {
        var writes = new List<TransactWrite> { PrepareUpdate(guild) };
        writes.AddRange(changedAccounts.Select(_accounts.PrepareUpdate));
        return DataTransaction.ExecuteAsync(DynamoDb, writes, cancellationToken);
    }

    public Task<ResultBase> CreateCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default) =>
        DataTransaction.ExecuteAsync(DynamoDb,
        [
            _characters.PrepareCreate(character),
            _characters.PrepareReservation(CharacterRepository.NameReservation(character.Name), character.Id),
            PrepareUpdate(guild),
        ], cancellationToken);

    public Task<ResultBase> DeleteCharacterAsync(Guild guild, Character character, CancellationToken cancellationToken = default)
    {
        character.IsDeleted = true;
        return DataTransaction.ExecuteAsync(DynamoDb,
        [
            _characters.PrepareUpdate(character),
            _characters.PrepareReservationRelease(CharacterRepository.NameReservation(character.Name), character.Id),
            PrepareUpdate(guild),
        ], cancellationToken);
    }
}
