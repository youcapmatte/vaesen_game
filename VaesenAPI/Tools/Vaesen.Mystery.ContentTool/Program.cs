using Microsoft.Extensions.DependencyInjection;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;

// ContentTool (spec 5.6): validate the content pack, list it, approve or retire records, and generate new records into
// the schemas. It edits the Content folder in the source tree, so every change shows up in git for review.

var a = new ToolArgs(args);
var command = a.Positional.FirstOrDefault() ?? "help";
var root = a.Get("content") ?? MysteryToolHost.FindSourceContent();
var setting = a.Get("setting") ?? "vaesen";

if (command is not "help" && root is null)
{
    Console.Error.WriteLine("Could not find the Content folder; pass --content <path>.");
    return 1;
}

var dir = root is null ? "" : Path.Combine(root, setting);

return command switch
{
    "validate" => Validate(),
    "list" => List(),
    "approve" => SetStatus(ContentStatus.Approved),
    "retire" => SetStatus(ContentStatus.Retired),
    "generate" => await Generate(),
    _ => Help(),
};

int Help()
{
    Console.WriteLine("""
        ContentTool: the setting's canon content.

          validate                       Check the whole pack (vocabulary, references, coverage).
          list                           Every record with its version and status.
          approve <id> [<id>...] | --all-drafts
                                         Mark records approved: generation then uses them.
          retire <id>                    Take a record out of use.
          generate <vaesen|conflict|character> "<what you want>"
                                         Have the model write a new draft record (needs ANTHROPIC_API_KEY).

          --content <path>   the Content folder (default: found in the source tree)
          --setting <id>     the setting (default: vaesen)
        """);
    return 0;
}

int Validate()
{
    var pack = ContentLoader.Load(dir);
    var issues = ContentValidator.Validate(pack);
    Console.WriteLine($"{pack.Setting.Name}: {pack.Vaesen.Count} creatures, {pack.PlaceTemplates.Count} place templates, {pack.Levels.Count} levels, " +
        $"{pack.Conflicts.Count} conflicts, {pack.Characters.Count} characters, {pack.Vocabulary.Tags.Count} tags.");
    foreach (var issue in issues.OrderBy(i => i.IsError ? 0 : 1))
    {
        Console.WriteLine(issue);
    }

    Console.WriteLine(issues.Exists(i => i.IsError) ? "FAILED" : "passed");
    return issues.Exists(i => i.IsError) ? 1 : 0;
}

int List()
{
    var pack = ContentLoader.Load(dir);
    foreach (var record in AllRecords(pack))
    {
        Console.WriteLine($"{ContentLoader.FolderFor(record),-11} {record.Id,-36} v{record.Version,-3} {record.Status,-9} {record.Name}");
    }

    return 0;
}

int SetStatus(string status)
{
    var pack = ContentLoader.Load(dir);
    var records = AllRecords(pack).ToList();
    var targets = a.Has("all-drafts") && status == ContentStatus.Approved
        ? records.Where(r => r.Status == ContentStatus.Draft).ToList()
        : a.Positional.Skip(1).Select(id => records.Find(r => r.Id == id) ?? throw new ArgumentException($"No record '{id}'.")).ToList();

    if (targets.Count == 0)
    {
        Console.Error.WriteLine("Name the records (ids), or pass --all-drafts.");
        return 1;
    }

    if (status == ContentStatus.Approved)
    {
        var errors = ContentValidator.Validate(pack).Where(i => i.IsError).ToList();
        if (errors.Count > 0)
        {
            Console.Error.WriteLine("Fix the pack's errors before approving anything:");
            errors.ForEach(e => Console.Error.WriteLine(e));
            return 1;
        }
    }

    foreach (var record in targets)
    {
        record.Status = status;
        ContentLoader.Write(dir, record);
        Console.WriteLine($"{record.Id} v{record.Version}: {status}");
    }

    return 0;
}

async Task<int> Generate()
{
    var kind = a.Positional.ElementAtOrDefault(1);
    var request = a.Positional.ElementAtOrDefault(2);
    if (kind is null || request is null || !ContentGenerator.Kinds.Contains(kind))
    {
        Console.Error.WriteLine($"Usage: generate <{string.Join("|", ContentGenerator.Kinds)}> \"<what you want>\"");
        return 1;
    }

    using var services = MysteryToolHost.Build();
    var generator = services.GetRequiredService<ContentGenerator>();
    var pack = ContentLoader.Load(dir);
    Console.WriteLine($"Writing a new {kind}...");
    var result = await generator.GenerateAsync(pack, kind, request);
    if (!result.Success)
    {
        Console.Error.WriteLine(result.ErrorMessage);
        return 1;
    }

    var record = result.Result!;
    if (AllRecords(pack).Any(r => r.Id == record.Id))
    {
        record.Id += "-" + Guid.NewGuid().ToString("N")[..6];
    }

    var path = ContentLoader.Write(dir, record);
    Console.WriteLine($"Draft written to {path}");
    foreach (var issue in ContentGenerator.Check(pack, record))
    {
        Console.WriteLine(issue);
    }

    Console.WriteLine("Review it, edit it if needed, then: approve " + record.Id);
    return 0;
}

static IEnumerable<ContentRecord> AllRecords(ContentPack pack) =>
    pack.Vaesen.Cast<ContentRecord>().Concat(pack.PlaceTemplates).Concat(pack.Levels).Concat(pack.Conflicts).Concat(pack.Characters);
