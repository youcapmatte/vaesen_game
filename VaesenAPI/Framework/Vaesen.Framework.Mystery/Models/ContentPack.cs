namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// One setting's canon, loaded from Content/{settingId}/: the setting, the vocabulary and the catalogues.
/// Holds the newest version of each record; <see cref="Usable"/> filters to what generation may use.
/// </summary>
public sealed class ContentPack
{
    public Setting Setting { get; set; } = new();

    public AffordanceVocabulary Vocabulary { get; set; } = new();

    public List<VaesenDefinition> Vaesen { get; set; } = [];

    public List<PlaceTemplate> PlaceTemplates { get; set; } = [];

    public List<LevelDefinition> Levels { get; set; } = [];

    public List<ConflictArchetype> Conflicts { get; set; } = [];

    public List<CharacterDefinition> Characters { get; set; } = [];

    public VaesenDefinition? FindVaesen(string? id) => Vaesen.Find(v => v.Id == id);

    public LevelDefinition? FindLevel(string? id) => Levels.Find(l => l.Id == id);

    public ConflictArchetype? FindConflict(string? id) => Conflicts.Find(c => c.Id == id);

    public CharacterDefinition? FindCharacter(string? id) => Characters.Find(c => c.Id == id);

    public PlaceTemplate? FindTemplate(string? id) => PlaceTemplates.Find(t => t.Id == id);

    /// <summary>The records generation may draw on: approved ones, or drafts too when <paramref name="includeDrafts"/>.</summary>
    public ContentPack Usable(bool includeDrafts)
    {
        bool Ok(ContentRecord record) => record.IsApproved || (includeDrafts && record.Status == ContentStatus.Draft);

        return new ContentPack
        {
            Setting = Setting,
            Vocabulary = Vocabulary,
            Vaesen = Vaesen.Where(Ok).ToList(),
            PlaceTemplates = PlaceTemplates.Where(Ok).ToList(),
            Levels = Levels.Where(Ok).ToList(),
            Conflicts = Conflicts.Where(Ok).ToList(),
            Characters = Characters.Where(Ok).ToList(),
        };
    }
}
