using Newtonsoft.Json;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Content;

/// <summary>
/// Reads a content pack from disk: Content/{settingId}/setting.json, vocabulary.json and one file per record in
/// vaesen/, places/, levels/, conflicts/ and characters/. A record's newer version is a new file ("nack.v2.json");
/// the loader keeps the highest version of each id.
/// </summary>
public static class ContentLoader
{
    public const string SettingFile = "setting.json";
    public const string VocabularyFile = "vocabulary.json";
    public const string VaesenFolder = "vaesen";
    public const string PlacesFolder = "places";
    public const string LevelsFolder = "levels";
    public const string ConflictsFolder = "conflicts";
    public const string CharactersFolder = "characters";

    public static readonly JsonSerializerSettings JsonSettings = new()
    {
        MissingMemberHandling = MissingMemberHandling.Ignore,
        NullValueHandling = NullValueHandling.Include,
        Formatting = Formatting.Indented,
    };

    /// <summary>Loads the pack in <paramref name="directory"/>. Throws on unreadable files; the caller reports them.</summary>
    public static ContentPack Load(string directory)
    {
        if (!Directory.Exists(directory))
        {
            throw new DirectoryNotFoundException($"Content directory not found: {directory}");
        }

        return new ContentPack
        {
            Setting = ReadFile<Setting>(Path.Combine(directory, SettingFile)),
            Vocabulary = ReadFile<AffordanceVocabulary>(Path.Combine(directory, VocabularyFile)),
            Vaesen = ReadFolder<VaesenDefinition>(Path.Combine(directory, VaesenFolder)),
            PlaceTemplates = ReadFolder<PlaceTemplate>(Path.Combine(directory, PlacesFolder)),
            Levels = ReadFolder<LevelDefinition>(Path.Combine(directory, LevelsFolder)),
            Conflicts = ReadFolder<ConflictArchetype>(Path.Combine(directory, ConflictsFolder)),
            Characters = ReadFolder<CharacterDefinition>(Path.Combine(directory, CharactersFolder)),
        };
    }

    /// <summary>The folder a record type lives in.</summary>
    public static string FolderFor(ContentRecord record) => record switch
    {
        VaesenDefinition => VaesenFolder,
        PlaceTemplate => PlacesFolder,
        LevelDefinition => LevelsFolder,
        ConflictArchetype => ConflictsFolder,
        CharacterDefinition => CharactersFolder,
        _ => throw new ArgumentException($"No content folder for {record.GetType().Name}.", nameof(record)),
    };

    /// <summary>The file name of a record version: "nack.json" for version 1, "nack.v2.json" after.</summary>
    public static string FileNameFor(ContentRecord record) =>
        record.Version <= 1 ? $"{record.Id}.json" : $"{record.Id}.v{record.Version}.json";

    /// <summary>
    /// Writes a record into the pack. An approved record is never overwritten: writing one whose file already holds an
    /// approved record fails, so a change has to be published as a new version.
    /// </summary>
    public static string Write(string directory, ContentRecord record)
    {
        var folder = Path.Combine(directory, FolderFor(record));
        Directory.CreateDirectory(folder);
        var path = Path.Combine(folder, FileNameFor(record));
        var json = JsonConvert.SerializeObject(record, record.GetType(), JsonSettings);
        if (File.Exists(path))
        {
            var current = (ContentRecord?)JsonConvert.DeserializeObject(File.ReadAllText(path), record.GetType(), JsonSettings);
            if (current is not null && current.Status == ContentStatus.Approved)
            {
                // Retiring is the one change allowed to an approved version; everything else needs a new version.
                current.Status = record.Status;
                if (record.Status != ContentStatus.Retired
                    || JsonConvert.SerializeObject(current, record.GetType(), JsonSettings) != json)
                {
                    throw new InvalidOperationException(
                        $"{record.Id} v{record.Version} is approved and cannot be changed; publish version {record.Version + 1} instead.");
                }
            }
        }

        File.WriteAllText(path, json + "\n");
        return path;
    }

    public static T ReadFile<T>(string path)
    {
        try
        {
            return JsonConvert.DeserializeObject<T>(File.ReadAllText(path), JsonSettings)
                ?? throw new InvalidDataException($"{path} is empty.");
        }
        catch (JsonException ex)
        {
            throw new InvalidDataException($"{Path.GetFileName(path)}: {ex.Message}", ex);
        }
    }

    private static List<T> ReadFolder<T>(string folder) where T : ContentRecord
    {
        if (!Directory.Exists(folder))
        {
            return [];
        }

        return Directory.EnumerateFiles(folder, "*.json")
            .Order(StringComparer.Ordinal)
            .Select(ReadFile<T>)
            .GroupBy(r => r.Id, StringComparer.Ordinal)
            .Select(g => g.OrderByDescending(r => r.Version).First())
            .OrderBy(r => r.Id, StringComparer.Ordinal)
            .ToList();
    }
}
