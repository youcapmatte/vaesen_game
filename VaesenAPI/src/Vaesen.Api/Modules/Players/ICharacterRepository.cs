namespace Vaesen.Api.Modules.Players;

/// <summary>A playable character, owned by an account.</summary>
/// <param name="Version">Incremented on every update; used to detect concurrent writes.</param>
public sealed record Character(string Id, string AccountId, string Name, string? GuildId, int Version);

public interface ICharacterRepository
{
    Task<Character?> GetAsync(string characterId, CancellationToken cancellationToken);

    /// <summary>All characters of an account, ordered by name.</summary>
    Task<IReadOnlyList<Character>> ListByAccountAsync(string accountId, CancellationToken cancellationToken);

    /// <summary>Creates the character and reserves its name (case-insensitive). Returns false if the name is taken.</summary>
    Task<bool> TryCreateAsync(Character character, CancellationToken cancellationToken);
}
