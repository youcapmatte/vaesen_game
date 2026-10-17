namespace Vaesen.Api.Common;

/// <summary>
/// Ids of everything Vaesen stores: "{type}-{lowercase guid}", e.g. "account-3f2b8c1e-...".
/// The type prefix makes ids self-describing in logs, URLs and the database.
/// </summary>
public static class EntityIds
{
    public const string Account = "account";
    public const string Character = "character";
    public const string Guild = "guild";

    public static string New(string type) => $"{type}-{Guid.NewGuid().ToString().ToLowerInvariant()}";

    /// <summary>True if <paramref name="id"/> is "{type}-{guid}" for the given type.</summary>
    public static bool IsValid(string type, string? id) =>
        id is not null
        && id.Length == type.Length + 1 + 36
        && id.StartsWith(type + "-", StringComparison.Ordinal)
        && Guid.TryParseExact(id.AsSpan(type.Length + 1), "D", out _)
        && id == id.ToLowerInvariant();
}
