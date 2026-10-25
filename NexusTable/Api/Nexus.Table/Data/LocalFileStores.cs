using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Data;

/// <summary>The "LocalData" configuration section: where the LocalFile provider keeps its files.</summary>
public sealed class LocalDataOptions
{
    public const string SectionName = "LocalData";

    /// <summary>Folder for the JSON files, relative to the working directory. Ignored by git.</summary>
    public string Directory { get; set; } = ".data";
}

/// <summary>
/// Development storage: every item of a type in one JSON file ({Directory}/{type}.json), kept in memory and
/// written on each change. Same rules as DynamoDB: soft delete, versions checked on update, unique values.
/// Not for production: one process, no paging.
/// </summary>
public class LocalFileStore<T> : IStore<T> where T : BaseCore
{
    private static readonly JsonSerializerSettings SerializerSettings = new()
    {
        Formatting = Formatting.Indented,
        NullValueHandling = NullValueHandling.Ignore,
        DateTimeZoneHandling = DateTimeZoneHandling.Utc,
        Converters = { new StringEnumConverter() },
    };

    private readonly string _path;
    private readonly TimeProvider _timeProvider;
    private readonly Dictionary<string, string> _items;

    public LocalFileStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    {
        _timeProvider = timeProvider;
        var directory = Path.GetFullPath(options.Value.Directory);
        System.IO.Directory.CreateDirectory(directory);
        _path = Path.Combine(directory, $"{ObjectCore.GetDefaultPrefix<T>()}.json");
        _items = File.Exists(_path)
            ? JsonConvert.DeserializeObject<Dictionary<string, T>>(File.ReadAllText(_path), SerializerSettings)!
                .ToDictionary(kv => kv.Key, kv => Serialize(kv.Value), StringComparer.Ordinal)
            : new Dictionary<string, string>(StringComparer.Ordinal);
    }

    /// <summary>Guards <see cref="_items"/> and the file.</summary>
    protected object Sync { get; } = new();

    public Task<T?> GetAsync(string id, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            return Task.FromResult(Find(id));
        }
    }

    public Task<IReadOnlyList<T>> GetManyAsync(IReadOnlyCollection<string> ids, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            IReadOnlyList<T> found = ids.Distinct(StringComparer.Ordinal).Select(Find).OfType<T>().ToList();
            return Task.FromResult(found);
        }
    }

    public Task<ResultBase<T>> CreateAsync(T item, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<T>();
        lock (Sync)
        {
            if (_items.ContainsKey(item.Id))
            {
                return Task.FromResult(ret.Fail($"A {typeof(T).Name} with this id already exists.", ErrorCodes.Conflict));
            }

            if (UniqueClash(item) is { } clash)
            {
                return Task.FromResult(ret.Fail(clash.Message, clash.Code));
            }

            var now = _timeProvider.GetUtcNow().UtcDateTime;
            item.CreatedAt = now;
            item.UpdatedAt = now;
            item.Version = 1;
            _items[item.Id] = Serialize(item);
            Save();
        }

        return Task.FromResult(ret.Ok(item));
    }

    public Task<ResultBase<T>> UpdateAsync(T item, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<T>();
        lock (Sync)
        {
            if (!_items.TryGetValue(item.Id, out var stored) || Deserialize(stored).Version != item.Version)
            {
                return Task.FromResult(ret.Fail($"The {typeof(T).Name} was changed or removed by someone else. Reload and try again.", ErrorCodes.Conflict));
            }

            item.Version++;
            item.UpdatedAt = _timeProvider.GetUtcNow().UtcDateTime;
            _items[item.Id] = Serialize(item);
            Save();
        }

        return Task.FromResult(ret.Ok(item));
    }

    /// <summary>Every visible item. Call inside <see cref="Sync"/>.</summary>
    protected IEnumerable<T> All() => _items.Values.Select(Deserialize).Where(item => !item.IsDeleted);

    /// <summary>Override to enforce a unique value; return the error when <paramref name="item"/> would clash.</summary>
    protected virtual (string Message, string Code)? UniqueClash(T item) => null;

    // Stored as JSON, so callers always get their own copy, like from a database.
    private static string Serialize(T item) => JsonConvert.SerializeObject(item, SerializerSettings);

    private static T Deserialize(string json) => JsonConvert.DeserializeObject<T>(json, SerializerSettings)!;

    private T? Find(string id) =>
        _items.TryGetValue(id, out var json) && Deserialize(json) is { IsDeleted: false } item ? item : null;

    private void Save()
    {
        var all = _items.ToDictionary(kv => kv.Key, kv => Deserialize(kv.Value), StringComparer.Ordinal);
        var temp = _path + ".tmp";
        File.WriteAllText(temp, JsonConvert.SerializeObject(all, SerializerSettings));
        File.Move(temp, _path, overwrite: true);
    }
}

public sealed class LocalAccountStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<Account>(options, timeProvider), IAccountStore
{
    public Task<Account?> GetBySubjectAsync(string subject, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            return Task.FromResult(All().FirstOrDefault(a => a.Subject == subject));
        }
    }

    protected override (string Message, string Code)? UniqueClash(Account item) =>
        All().Any(a => a.Subject == item.Subject) ? ("This login already has an account.", NexusErrorCodes.SubjectTaken) : null;
}

public sealed class LocalSocietyStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<Society>(options, timeProvider), ISocietyStore;

public sealed class LocalCharacterStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<Character>(options, timeProvider), ICharacterStore
{
    public Task<IReadOnlyList<Character>> ListBySocietyAsync(string societyId, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            IReadOnlyList<Character> characters = All()
                .Where(c => c.SocietyId == societyId)
                .OrderBy(c => c.Name, StringComparer.OrdinalIgnoreCase)
                .ToList();
            return Task.FromResult(characters);
        }
    }
}

public sealed class LocalSessionStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<PlaySession>(options, timeProvider), ISessionStore
{
    public Task<IReadOnlyList<PlaySession>> ListByMysteryAsync(string mysteryId, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            IReadOnlyList<PlaySession> sessions = All().Where(s => s.MysteryId == mysteryId).OrderBy(s => s.Number).ToList();
            return Task.FromResult(sessions);
        }
    }
}

public sealed class LocalSessionEventStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<SessionEvent>(options, timeProvider), ISessionEventStore
{
    public Task<IReadOnlyList<SessionEvent>> ListBySessionAsync(string sessionId, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            IReadOnlyList<SessionEvent> events = All().Where(e => e.SessionId == sessionId).OrderBy(e => e.At).ThenBy(e => e.CreatedAt).ToList();
            return Task.FromResult(events);
        }
    }
}

public sealed class LocalRollStore(IOptions<LocalDataOptions> options, TimeProvider timeProvider)
    : LocalFileStore<Roll>(options, timeProvider), IRollStore
{
    public Task<IReadOnlyList<Roll>> ListRecentBySocietyAsync(string societyId, int limit, CancellationToken cancellationToken = default)
    {
        lock (Sync)
        {
            IReadOnlyList<Roll> rolls = All()
                .Where(r => r.SocietyId == societyId)
                .OrderByDescending(r => r.CreatedAt)
                .Take(limit)
                .ToList();
            return Task.FromResult(rolls);
        }
    }
}
