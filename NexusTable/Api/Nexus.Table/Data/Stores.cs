using Nexus.Table.Models;
using Vaesen.Framework.Core.Data;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Data;

/// <summary>
/// Storage for one model type. Two implementations: DynamoDB (<see cref="Dynamo"/>, the nexus-table-{env} table)
/// and local JSON files (<see cref="LocalFileStore{T}"/>, for development without DynamoDB). Pick one with
/// DataService:Provider ("DynamoDb" or "LocalFile").
/// </summary>
public interface IStore<T> where T : BaseCore
{
    Task<T?> GetAsync(string id, CancellationToken cancellationToken = default);

    Task<IReadOnlyList<T>> GetManyAsync(IReadOnlyCollection<string> ids, CancellationToken cancellationToken = default);

    /// <summary>Fails with <see cref="ErrorCodes.Conflict"/> if the id (or a unique value of it) is taken.</summary>
    Task<ResultBase<T>> CreateAsync(T item, CancellationToken cancellationToken = default);

    /// <summary>Fails with <see cref="ErrorCodes.Conflict"/> if someone else changed the item since it was read.</summary>
    Task<ResultBase<T>> UpdateAsync(T item, CancellationToken cancellationToken = default);
}

public interface IAccountStore : IStore<Account>
{
    /// <summary>The account of a token subject. Subjects are unique.</summary>
    Task<Account?> GetBySubjectAsync(string subject, CancellationToken cancellationToken = default);
}

public interface ISocietyStore : IStore<Society>;

public interface ICharacterStore : IStore<Character>
{
    /// <summary>The Society's characters, ordered by name.</summary>
    Task<IReadOnlyList<Character>> ListBySocietyAsync(string societyId, CancellationToken cancellationToken = default);
}

public interface IRollStore : IStore<Roll>
{
    /// <summary>The Society's latest rolls, newest first.</summary>
    Task<IReadOnlyList<Roll>> ListRecentBySocietyAsync(string societyId, int limit, CancellationToken cancellationToken = default);
}

/// <summary>Error codes of the Nexus stores and services.</summary>
public static class NexusErrorCodes
{
    public const string SubjectTaken = "subject_taken";
    public const string ArchetypeTaken = "archetype_taken";
    public const string AlreadyPushed = "already_pushed";
}
