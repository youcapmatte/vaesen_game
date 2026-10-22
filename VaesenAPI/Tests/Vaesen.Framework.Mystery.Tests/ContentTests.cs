using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Models;
using Xunit.Abstractions;

namespace Vaesen.Framework.Mystery.Tests;

public sealed class ContentTests(ITestOutputHelper output)
{
    public static string ContentRoot => Path.Combine(AppContext.BaseDirectory, "Content", "vaesen");

    public static ContentPack LoadPack() => ContentLoader.Load(ContentRoot);

    [Fact]
    public void The_Vaesen_pack_loads_with_every_catalogue_filled()
    {
        var pack = LoadPack();

        Assert.NotEmpty(pack.Setting.WorldTruths);
        Assert.True(pack.Vocabulary.Tags.Count > 50);
        Assert.Equal(["nack", "nisse", "skogsra", "werewolf"], pack.Vaesen.Select(v => v.Id));
        Assert.True(pack.PlaceTemplates.Count >= 10);
        Assert.True(pack.Levels.Count >= 6);
        Assert.True(pack.Conflicts.Count >= 5);
        Assert.True(pack.Characters.Count >= 4);
    }

    [Fact]
    public void The_Vaesen_pack_has_no_content_errors()
    {
        var issues = ContentValidator.Validate(LoadPack());
        foreach (var issue in issues)
        {
            output.WriteLine(issue.ToString());
        }

        Assert.DoesNotContain(issues, i => i.IsError);
    }

    [Fact]
    public void Every_creature_has_a_level_and_a_conflict_it_can_use()
    {
        var pack = LoadPack();
        foreach (var vaesen in pack.Vaesen)
        {
            var fits = pack.Levels.Where(l => ContentValidator.LevelFits(l, vaesen, pack.Vocabulary)).ToList();
            Assert.True(fits.Count > 0, $"{vaesen.Id} has no level");
            Assert.Contains(fits, l => Generation.Assignment.FittingConflicts(pack, vaesen, l).Count > 0);
        }
    }

    [Fact]
    public void Implied_tags_expand_transitively()
    {
        var vocab = LoadPack().Vocabulary;

        var millpond = vocab.Expand("millpond");

        Assert.Contains("still-water", millpond);
        Assert.Contains("water", millpond);
    }

    [Fact]
    public void An_approved_record_cannot_be_changed_in_place()
    {
        var dir = Directory.CreateTempSubdirectory("content-").FullName;
        var record = new ConflictArchetype { Id = "test-conflict", Name = "Test", Status = ContentStatus.Approved };
        ContentLoader.Write(dir, record);

        record.Description = "changed";

        Assert.Throws<InvalidOperationException>(() => ContentLoader.Write(dir, record));
        record.Version = 2;
        record.Status = ContentStatus.Draft;
        ContentLoader.Write(dir, record);
        Assert.True(File.Exists(Path.Combine(dir, "conflicts", "test-conflict.v2.json")));
    }
}
