using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Core.Data;

/// <summary>Generic storage for one model type. Deleted items (soft delete) are never returned.</summary>
public interface IBaseRepository<T> where T : BaseCore
{
    Task<T?> GetItemAsync(string id, CancellationToken cancellationToken = default);

    Task<IReadOnlyList<T>> GetItemsAsync(IReadOnlyCollection<string> ids, CancellationToken cancellationToken = default);

    /// <summary>Stores a new item. Fails with <see cref="ErrorCodes.Conflict"/> if the id already exists.</summary>
    Task<ItemAction<T>> CreateItemAsync(T item, CancellationToken cancellationToken = default);

    /// <summary>Stores changes. Fails with <see cref="ErrorCodes.Conflict"/> if someone else updated the item first.</summary>
    Task<ItemAction<T>> UpdateItemAsync(T item, CancellationToken cancellationToken = default);

    /// <summary>Soft delete: marks the item deleted so it's no longer returned.</summary>
    Task<ItemAction<T>> DeleteItemAsync(string id, CancellationToken cancellationToken = default);
}

/// <summary>Outcome of a write: the stored item on success.</summary>
public class ItemAction<T> : ResultBase<T>
{
}
