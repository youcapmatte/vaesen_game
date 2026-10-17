using System.Text;
using Newtonsoft.Json;

namespace Vaesen.Framework.Core.Models;

/// <summary>
/// Base for everything with an id. Ids are "{type}-{lowercase guid}", where {type} is the class name in lowercase,
/// e.g. <c>account-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab</c>. A new object gets its id on first read.
/// </summary>
public class ObjectCore
{
    private string _id = string.Empty;

    [JsonProperty("id")]
    public virtual string Id
    {
        get
        {
            if (string.IsNullOrWhiteSpace(_id))
            {
                _id = CreateRandomId(IdPrefix);
            }

            return _id;
        }
        set => _id = value?.Trim() ?? string.Empty;
    }

    /// <summary>Prefix of this type's ids. Defaults to the class name in lowercase.</summary>
    [JsonIgnore]
    protected virtual string IdPrefix => GetDefaultPrefix(GetType());

    public static string GetDefaultPrefix(Type type)
    {
        ArgumentNullException.ThrowIfNull(type);

        var typeName = type.Name;
        var genericSeparator = typeName.IndexOf('`', StringComparison.Ordinal);
        if (genericSeparator > 0)
        {
            typeName = typeName[..genericSeparator];
        }

        return NormalizePrefix(typeName);
    }

    public static string GetDefaultPrefix<TObject>() where TObject : ObjectCore => GetDefaultPrefix(typeof(TObject));

    /// <summary>Lowercase letters and digits; anything else becomes a single '-'.</summary>
    public static string NormalizePrefix(string? prefix)
    {
        if (string.IsNullOrWhiteSpace(prefix))
        {
            throw new ArgumentException("ID prefix cannot be empty.", nameof(prefix));
        }

        var raw = prefix.Trim().ToLowerInvariant();
        var builder = new StringBuilder(raw.Length);
        var previousWasDash = false;
        foreach (var ch in raw)
        {
            if (char.IsLetterOrDigit(ch))
            {
                builder.Append(ch);
                previousWasDash = false;
                continue;
            }

            if (!previousWasDash)
            {
                builder.Append('-');
                previousWasDash = true;
            }
        }

        var normalized = builder.ToString().Trim('-');
        if (string.IsNullOrWhiteSpace(normalized))
        {
            throw new ArgumentException("ID prefix cannot be empty after normalization.", nameof(prefix));
        }

        return normalized;
    }

    public static string CreateRandomId(string prefix) => FormatId(prefix, Guid.NewGuid());

    public static string FormatId(string prefix, Guid guid) => $"{NormalizePrefix(prefix)}-{guid:D}";

    /// <summary>True if <paramref name="id"/> is "{prefix}-{lowercase guid}".</summary>
    public static bool IsValidId(string prefix, string? id)
    {
        var normalizedPrefix = NormalizePrefix(prefix);
        return id is not null
            && id.Length == normalizedPrefix.Length + 1 + 36
            && id.StartsWith(normalizedPrefix + "-", StringComparison.Ordinal)
            && Guid.TryParseExact(id.AsSpan(normalizedPrefix.Length + 1), "D", out _)
            && string.Equals(id, id.ToLowerInvariant(), StringComparison.Ordinal);
    }

    public static bool IsValidId<TObject>(string? id) where TObject : ObjectCore => IsValidId(GetDefaultPrefix<TObject>(), id);
}

/// <summary>Base for everything stored by a repository: timestamps, soft delete and a version for safe concurrent updates.</summary>
public class BaseCore : ObjectCore
{
    private DateTime _createdAt;
    private DateTime _updatedAt;

    [JsonProperty("createdAt")]
    public DateTime CreatedAt
    {
        get => _createdAt;
        set => _createdAt = EnsureUtc(value);
    }

    [JsonProperty("updatedAt")]
    public DateTime UpdatedAt
    {
        get => _updatedAt;
        set => _updatedAt = EnsureUtc(value);
    }

    /// <summary>Soft delete: repositories hide deleted items instead of removing them.</summary>
    [JsonProperty("isDeleted")]
    public bool IsDeleted { get; set; }

    /// <summary>Incremented by every update; an update based on an older version is rejected.</summary>
    [JsonProperty("version")]
    public int Version { get; set; }

    private static DateTime EnsureUtc(DateTime value)
    {
        if (value == default || value.Kind == DateTimeKind.Utc)
        {
            return value;
        }

        return value.Kind == DateTimeKind.Local
            ? value.ToUniversalTime()
            : DateTime.SpecifyKind(value, DateTimeKind.Utc);
    }
}
