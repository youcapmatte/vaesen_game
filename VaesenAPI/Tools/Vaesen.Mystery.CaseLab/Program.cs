using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Newtonsoft.Json;
using Vaesen.Framework.Mystery;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

// CaseLab (spec 3.3): generate mysteries offline, run the validator, and dump case packets with readable reports, so the
// owner can judge many cases without starting a voice session.

var args2 = new ToolArgs(args);
var command = args2.Positional.FirstOrDefault() ?? "help";

return command switch
{
    "assign" => await Assign(),
    "generate" => await Generate(),
    "validate" => Validate(),
    "report" => Report(),
    _ => Help(),
};

int Help()
{
    Console.WriteLine("""
        CaseLab: generate and inspect mysteries offline.

          assign    [--count 5] [--month 10] [--seed N] [--vaesen id] [--level id]
                    Show what the assignment stage would choose (no model calls).
          generate  [--count 1] [--month 10] [--seed N] [--vaesen id] [--level id] [--origin id] [--out dir]
                    [--no-critic] [--approved-only] [--density low|medium|high] [--verbose]
                    Generate cases with the Anthropic API (needs ANTHROPIC_API_KEY) and write
                    {id}.json and {id}.md (report and generation log) to --out (default ./cases).
          validate  <case.json>   Run the structural checks on a case packet.
          report    <case.json>   Write the Markdown report of a case packet to stdout.

        Models and limits come from Mystery__* settings (appsettings.json, user secrets, environment).
        """);
    return 0;
}

ServiceProvider Services(bool verbose = false) => MysteryToolHost.Build(o =>
{
    o.UseDraftContent = !args2.Has("approved-only");
    if (args2.Has("no-critic"))
    {
        o.UseCritic = false;
    }

    if (args2.Get("density") is { } density)
    {
        o.RedHerringDensity = density;
    }

    o.CasePacketPath = Path.GetFullPath(args2.Get("out") ?? "cases");
}, verbose ? LogLevel.Information : LogLevel.Warning);

GenerationRequest Request(int index, bool store) => new()
{
    History = new SocietyHistory { SocietyId = "caselab", SocietyName = "CaseLab" },
    Month = args2.GetInt("month"),
    Seed = args2.GetInt("seed") is { } seed ? seed + index : null,
    VaesenId = args2.Get("vaesen"),
    OriginId = args2.Get("origin"),
    LevelId = args2.Get("level"),
    Store = store,
};

async Task<int> Assign()
{
    using var services = Services();
    var generator = services.GetRequiredService<IMysteryGenerator>();
    var count = args2.GetInt("count") ?? 5;
    for (var i = 0; i < count; i++)
    {
        var brief = await generator.AssignAsync(Request(i, store: false));
        if (!brief.Success)
        {
            Console.Error.WriteLine(brief.ErrorMessage);
            return 1;
        }

        var b = brief.Result!;
        Console.WriteLine($"seed {b.Seed}: {b.VaesenId} at {b.LevelId} (month {b.Month}), {b.ReasonKind}{(b.OriginId is null ? "" : $"/{b.OriginId}")}, " +
            $"{(b.HiddenHand ? $"hidden hand ({b.HandDisposition})" : b.PlotCentreKind)}, focus {b.Focus}, hook {b.HookChannelId} from a {b.HookGiverType}; " +
            $"conflicts {string.Join(", ", b.ConflictArchetypeIds)}{(b.Notes.Count == 0 ? "" : " | " + string.Join(" ", b.Notes))}");
    }

    return 0;
}

async Task<int> Generate()
{
    using var services = Services(args2.Has("verbose"));
    var generator = services.GetRequiredService<IMysteryGenerator>();
    var store = (FileCasePacketStore)services.GetRequiredService<ICasePacketStore>();
    var count = args2.GetInt("count") ?? 1;
    var failures = 0;
    var progress = new Progress<GenerationProgress>(p => Console.WriteLine($"  [{p.CaseAttempt}] {p.Step}: {p.Message}"));

    for (var i = 0; i < count; i++)
    {
        Console.WriteLine($"Case {i + 1} of {count}");
        var result = await generator.GenerateAsync(Request(i, store: true), progress);
        var stored = result.Result?.Case;
        if (stored is not null)
        {
            var folder = Path.Combine(store.Root, stored.Packet.SocietyId);
            Directory.CreateDirectory(folder);
            var name = string.IsNullOrEmpty(stored.Packet.MysteryId) ? $"failed-{DateTime.UtcNow:yyyyMMddHHmmss}" : stored.Packet.MysteryId;
            if (!result.Success)
            {
                // A failed case is not stored by the generator; keep it here for inspection.
                File.WriteAllText(Path.Combine(folder, name + ".failed.json"), JsonConvert.SerializeObject(stored, Formatting.Indented));
            }

            var report = Path.Combine(folder, name + (result.Success ? ".md" : ".failed.md"));
            File.WriteAllText(report, CaseReport.ToMarkdown(stored));
            Console.WriteLine($"  -> {report}");
        }

        if (!result.Success)
        {
            failures++;
            Console.Error.WriteLine($"  FAILED: {result.ErrorMessage}");
        }
        else
        {
            Console.WriteLine($"  '{stored!.Packet.Title}': {stored.Packet.Facts.Count} facts, {stored.Packet.Channels.Count} channels, {stored.Packet.Cast.Count} people, " +
                $"{stored.Packet.Validation.Issues.Count(x => !x.IsError)} warnings, {stored.Log.Sum(e => e.OutputTokens)} output tokens.");
        }
    }

    return failures == 0 ? 0 : 1;
}

int Validate()
{
    if (!TryLoad(out var stored))
    {
        return 1;
    }

    var pack = ContentLoaderFor();
    var p = stored.Packet;
    var level = pack.FindLevel(p.Brief.LevelId)?.ForCase(p.Level);
    if (level is null)
    {
        Console.Error.WriteLine($"Level '{p.Brief.LevelId}' is not in the content pack.");
        return 1;
    }

    var context = new ValidationContext
    {
        Pack = pack,
        Level = level,
        Vaesen = p.Brief.VaesenIds.Select(pack.FindVaesen).OfType<VaesenDefinition>().ToList(),
        Conflict = pack.FindConflict(p.Brief.ConflictArchetypeId),
    };
    var issues = new CaseValidator().Validate(p, context);
    var walk = new FactGraph(p).Walk();
    Console.WriteLine($"{p.Title}: {p.Facts.Count(f => f.LoadBearing)} load-bearing facts, {walk.Known.Count}/{p.Facts.Count} facts reachable, tension reaches {walk.FinalTension:0.00}.");
    foreach (var issue in issues.OrderBy(i => i.IsError ? 0 : 1))
    {
        Console.WriteLine(issue);
    }

    Console.WriteLine(issues.Exists(i => i.IsError) ? "FAILED" : "passed");
    return issues.Exists(i => i.IsError) ? 1 : 0;
}

int Report()
{
    if (!TryLoad(out var stored))
    {
        return 1;
    }

    Console.WriteLine(CaseReport.ToMarkdown(stored));
    return 0;
}

bool TryLoad(out StoredCase stored)
{
    stored = new StoredCase();
    var path = args2.Positional.ElementAtOrDefault(1);
    if (path is null || !File.Exists(path))
    {
        Console.Error.WriteLine("Give the path of a case file.");
        return false;
    }

    var json = File.ReadAllText(path);
    // Accept both a stored case ({ packet, brief, log }) and a bare packet.
    var asStored = JsonConvert.DeserializeObject<StoredCase>(json);
    stored = asStored is { Packet.Facts.Count: > 0 } ? asStored : new StoredCase { Packet = JsonConvert.DeserializeObject<CasePacket>(json)! };
    return true;
}

ContentPack ContentLoaderFor()
{
    using var services = Services();
    var content = services.GetRequiredService<IContentStore>().GetPackAsync(services.GetRequiredService<Microsoft.Extensions.Options.IOptions<MysteryOptions>>().Value.SettingId).Result;
    return content.Result!;
}
