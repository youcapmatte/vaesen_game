using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Data;

/// <summary>Where canon content comes from. Files in the library today; DynamoDB (CANON#{type}) once content is published there.</summary>
public interface IContentStore
{
    Task<ResultBase<ContentPack>> GetPackAsync(string settingId, CancellationToken cancellationToken = default);
}

/// <summary>Reads content packs from Content/{settingId}/ and keeps them in memory.</summary>
public sealed class FileContentStore(IOptions<MysteryOptions> options) : IContentStore
{
    private readonly Dictionary<string, ContentPack> _cache = [];
    private readonly Lock _lock = new();

    public string Root => string.IsNullOrWhiteSpace(options.Value.ContentPath)
        ? Path.Combine(AppContext.BaseDirectory, "Content")
        : options.Value.ContentPath;

    public Task<ResultBase<ContentPack>> GetPackAsync(string settingId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<ContentPack>();
        try
        {
            lock (_lock)
            {
                if (!_cache.TryGetValue(settingId, out var pack))
                {
                    pack = ContentLoader.Load(Path.Combine(Root, settingId));
                    _cache[settingId] = pack;
                }

                ret.Ok(pack);
            }
        }
        catch (Exception ex) when (ex is IOException or InvalidDataException or UnauthorizedAccessException)
        {
            ret.Fail($"Could not load the content for '{settingId}': {ex.Message}", ErrorCodes.NotFound);
        }

        return Task.FromResult(ret);
    }
}

/// <summary>A stored case: the packet (the hidden GM file) and the log of how it was generated.</summary>
public sealed class StoredCase
{
    [JsonProperty("packet")]
    public CasePacket Packet { get; set; } = new();

    [JsonProperty("brief")]
    public AssignmentBrief Brief { get; set; } = new();

    [JsonProperty("log")]
    public List<GenerationLogEntry> Log { get; set; } = [];
}

/// <summary>Short facts about a stored case, safe to list (no truth in it).</summary>
public sealed class CaseSummary
{
    [JsonProperty("mysteryId")]
    public string MysteryId { get; set; } = "";

    [JsonProperty("societyId")]
    public string SocietyId { get; set; } = "";

    [JsonProperty("title")]
    public string Title { get; set; } = "";

    [JsonProperty("generatedAt")]
    public DateTime GeneratedAt { get; set; }

    [JsonProperty("levelId")]
    public string LevelId { get; set; } = "";

    [JsonProperty("month")]
    public int Month { get; set; }

    [JsonProperty("passed")]
    public bool Passed { get; set; }

    /// <summary>For the Society's history (cooldowns); spoils only which creature it was.</summary>
    [JsonProperty("vaesenIds")]
    public List<string> VaesenIds { get; set; } = [];

    [JsonProperty("hookChannelId")]
    public string HookChannelId { get; set; } = "";

    [JsonProperty("hookGiverType")]
    public string HookGiverType { get; set; } = "";

    public static CaseSummary From(CasePacket p) => new()
    {
        MysteryId = p.MysteryId,
        SocietyId = p.SocietyId,
        Title = p.Title,
        GeneratedAt = p.GeneratedAt,
        LevelId = p.Brief.LevelId,
        Month = p.Brief.Month,
        Passed = p.Validation.Passed,
        VaesenIds = p.Brief.VaesenIds,
        HookChannelId = p.Brief.HookChannelId,
        HookGiverType = p.Brief.HookGiverType,
    };
}

/// <summary>Case packets. Host only: players must never be able to read one (spec 20).</summary>
public interface ICasePacketStore
{
    Task<ResultBase<StoredCase>> SaveAsync(StoredCase stored, CancellationToken cancellationToken = default);

    Task<ResultBase<StoredCase>> GetAsync(string societyId, string mysteryId, CancellationToken cancellationToken = default);

    Task<ResultBase<List<CaseSummary>>> ListAsync(string societyId, CancellationToken cancellationToken = default);
}

/// <summary>Case packets as JSON files: {CasePacketPath}/{societyId}/{mysteryId}.json. Development and CaseLab; S3 later.</summary>
public sealed class FileCasePacketStore(IOptions<MysteryOptions> options) : ICasePacketStore
{
    public string Root => string.IsNullOrWhiteSpace(options.Value.CasePacketPath)
        ? Path.Combine(Directory.GetCurrentDirectory(), ".data", "cases")
        : options.Value.CasePacketPath;

    public async Task<ResultBase<StoredCase>> SaveAsync(StoredCase stored, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        try
        {
            var folder = Path.Combine(Root, Safe(stored.Packet.SocietyId));
            Directory.CreateDirectory(folder);
            var json = JsonConvert.SerializeObject(stored, Formatting.Indented);
            await File.WriteAllTextAsync(Path.Combine(folder, Safe(stored.Packet.MysteryId) + ".json"), json, cancellationToken);
            ret.Ok(stored);
        }
        catch (IOException ex)
        {
            ret.Fail($"Could not save the case: {ex.Message}");
        }

        return ret;
    }

    public async Task<ResultBase<StoredCase>> GetAsync(string societyId, string mysteryId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        var path = Path.Combine(Root, Safe(societyId), Safe(mysteryId) + ".json");
        if (!File.Exists(path))
        {
            return ret.Fail("No such case.", ErrorCodes.NotFound);
        }

        try
        {
            var stored = JsonConvert.DeserializeObject<StoredCase>(await File.ReadAllTextAsync(path, cancellationToken));
            return stored is null ? ret.Fail("The case file is empty.", ErrorCodes.NotFound) : ret.Ok(stored);
        }
        catch (Exception ex) when (ex is IOException or JsonException)
        {
            return ret.Fail($"Could not read the case: {ex.Message}");
        }
    }

    public async Task<ResultBase<List<CaseSummary>>> ListAsync(string societyId, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<List<CaseSummary>>();
        var folder = Path.Combine(Root, Safe(societyId));
        if (!Directory.Exists(folder))
        {
            return ret.Ok([]);
        }

        var summaries = new List<CaseSummary>();
        foreach (var file in Directory.EnumerateFiles(folder, "*.json"))
        {
            try
            {
                var stored = JsonConvert.DeserializeObject<StoredCase>(await File.ReadAllTextAsync(file, cancellationToken));
                if (stored is not null)
                {
                    summaries.Add(CaseSummary.From(stored.Packet));
                }
            }
            catch (JsonException)
            {
                // A half-written or foreign file is skipped, not fatal for the listing.
            }
        }

        return ret.Ok(summaries.OrderByDescending(s => s.GeneratedAt).ToList());
    }

    /// <summary>Ids become file names; keep them to letters, digits and dashes.</summary>
    private static string Safe(string id) =>
        new(id.Select(c => char.IsLetterOrDigit(c) || c is '-' or '_' ? c : '_').ToArray());
}
