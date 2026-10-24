using System.Text;
using Microsoft.Extensions.Options;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Content;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Services;

/// <summary>
/// Generates canon content into the fixed schemas (spec 5.6): creatures, conflict archetypes and canonical characters.
/// Every record comes back as a draft, is checked against the pack, and waits for the owner's approval. Levels are
/// assembled from place templates and are not generated here yet (their nested affordances are recursive, which
/// structured outputs do not allow).
/// </summary>
/// <summary>The period canon as the content tool drafts it.</summary>
public sealed class PeriodCanon
{
    [Newtonsoft.Json.JsonProperty("periodTransport")]
    public List<PeriodTransportLine> PeriodTransport { get; set; } = [];

    [Newtonsoft.Json.JsonProperty("regionalInstitutions")]
    public List<RegionalInstitution> RegionalInstitutions { get; set; } = [];
}

/// <summary>A level's place in the country as the content tool drafts it.</summary>
public sealed class LevelPlacement
{
    [Newtonsoft.Json.JsonProperty("geography")]
    public LevelGeography Geography { get; set; } = new();

    [Newtonsoft.Json.JsonProperty("journey")]
    public LevelJourney Journey { get; set; } = new();

    [Newtonsoft.Json.JsonProperty("travel")]
    public string Travel { get; set; } = "";
}

public sealed class ContentGenerator(IMysteryModel model, IOptions<MysteryOptions> options)
{
    public static readonly IReadOnlyList<string> Kinds = ["vaesen", "conflict", "character"];

    public async Task<ResultBase<ContentRecord>> GenerateAsync(ContentPack pack, string kind, string request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<ContentRecord>();
        var o = options.Value;
        var system = BuildSystem(pack);
        var context = BuildContext(pack, kind);
        var instruction = $"# Write one new {kind} record\n\n{request}\n\nUse a readable lowercase id with hyphens that is not already taken. Set version to 1 and status to draft.";

        ResultBase<ContentRecord> Fail(ResultBase r) => ret.Fail(r.ErrorMessage, r.ErrorCode);

        ContentRecord record;
        switch (kind)
        {
            case "vaesen":
                var v = await model.GenerateAsync<VaesenDefinition>(Request(o, system, context, instruction), cancellationToken);
                if (!v.Success)
                {
                    return Fail(v);
                }

                record = v.Result!.Value;
                break;
            case "conflict":
                var c = await model.GenerateAsync<ConflictArchetype>(Request(o, system, context, instruction), cancellationToken);
                if (!c.Success)
                {
                    return Fail(c);
                }

                record = c.Result!.Value;
                break;
            case "character":
                var ch = await model.GenerateAsync<CharacterDefinition>(Request(o, system, context, instruction), cancellationToken);
                if (!ch.Success)
                {
                    return Fail(ch);
                }

                record = ch.Result!.Value;
                break;
            default:
                return ret.Fail($"Unknown content kind '{kind}'. Use one of: {string.Join(", ", Kinds)}.", ErrorCodes.Invalid);
        }

        record.Version = 1;
        record.Status = ContentStatus.Draft;
        return ret.Ok(record);
    }

    /// <summary>
    /// A level in another month (fixes 4, section 12.4): the same places, owners and history, with the season, the
    /// journey, the work of the month and the scenes as they are then.
    /// </summary>
    public async Task<ResultBase<LevelMonthVariant>> GenerateVariantAsync(ContentPack pack, LevelDefinition level, int month, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<LevelMonthVariant>();
        var monthName = System.Globalization.CultureInfo.InvariantCulture.DateTimeFormat.GetMonthName(month);
        var context = new StringBuilder();
        context.AppendLine($"# The level, as written for month {level.Month}");
        context.AppendLine(PromptBuilder.Json(new
        {
            level.Name,
            level.Region,
            level.SettlementType,
            level.Terrain,
            level.Description,
            level.SeasonNotes,
            level.Travel,
            level.PublicFacts,
            places = level.Places.Select(p => new { p.Id, p.Name, p.OwnerRole, p.Description }),
        }));
        var instruction = $"""
            # Write the level as it is in {monthName}

            The places, their owners, the people and the history stay exactly as they are; only what the season changes is new.
            - seasonNotes: the weather, the light and the work of {monthName} here, in the same voice and length as the level's own.
            - travel: the journey from Uppsala in {monthName} (roads, ice, snow, boats or railway as the period and the season allow); empty if unchanged.
            - publicFacts: only what everyone knows in this month and not otherwise (a closed road, a fair, a hiring day); often none.
            - placeDescriptions: by place id, a new description for each place the season changes visibly, in the length of the original.
            Set month to {month}.
            """;
        var reply = await model.GenerateAsync<LevelMonthVariant>(Request(options.Value, BuildSystem(pack), context.ToString(), instruction), cancellationToken);
        if (!reply.Success)
        {
            return ret.Fail(reply.ErrorMessage, reply.ErrorCode);
        }

        var variant = reply.Result!.Value;
        variant.Month = month;
        return ret.Ok(variant);
    }

    /// <summary>
    /// Drafts the period canon (fixes 4, section 15) for the regions the levels are in: railways, canals, steamer routes
    /// and telegraph lines with the year each opened and the months it runs, and where the authorities sit. The owner
    /// reviews it once, like the canonical roster.
    /// </summary>
    public async Task<ResultBase<PeriodCanon>> GenerateCanonAsync(ContentPack pack, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<PeriodCanon>();
        var context = new StringBuilder();
        context.AppendLine($"# Headquarters: Uppsala. Play starts in {pack.Setting.StartYear}.");
        context.AppendLine("# The levels' regions and how the travel text says one gets there");
        foreach (var level in pack.Levels)
        {
            context.AppendLine($"- {level.Name} ({level.Region}): {level.Travel}");
        }

        context.AppendLine();
        context.AppendLine("# Already in the canon (keep these ids; correct them if they are wrong)");
        context.AppendLine(PromptBuilder.Json(new { pack.Setting.PeriodTransport, pack.Setting.RegionalInstitutions }));
        var instruction = $"""
            # Write the period transport and institutions canon

            periodTransport: every railway, canal, steamer route and telegraph line a journey from Uppsala to these regions
            could use between {pack.Setting.StartYear} and {pack.Setting.StartYear + 10}, and the main lines between them.
            For each: a readable id, its name, kind, endpoints and the main stops between (via), the year it opened (for a
            line that opened in stages, one entry per stage with the stretch it covers), the months it runs (canals and lake
            or coastal steamers stop when the ice comes; leave empty for all year), the region, and notes (gauge, private
            or state, what it carried). Lines being built in {pack.Setting.StartYear} are listed with the year they opened.
            Only what you are confident existed: leave out what you are unsure of, and say in notes when a year is approximate.

            regionalInstitutions: for each region, the county seat and governor, the district courts (häradsrätt) and where
            they sit, the crown sheriffs (kronolänsman) of the districts, the provincial doctors, the savings banks and the
            newspapers of the nearest towns. A readable id, the kind (countySeat, districtCourt, crownSheriff,
            provincialDoctor, bank, newspaper), the name, the town it sits in (seat), the region, notes.
            """;
        var reply = await model.GenerateAsync<PeriodCanon>(Request(options.Value, BuildSystem(pack), context.ToString(), instruction), cancellationToken);
        return reply.Success ? ret.Ok(reply.Result!.Value) : ret.Fail(reply.ErrorMessage, reply.ErrorCode);
    }

    /// <summary>
    /// A level's geography and journey (fixes 4, sections 13 and 14), from its region, the period canon and a real place
    /// it can be modelled on. The places, owners and history stay as they are.
    /// </summary>
    public async Task<ResultBase<LevelPlacement>> GenerateGeographyAsync(ContentPack pack, LevelDefinition level, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<LevelPlacement>();
        var context = new StringBuilder();
        context.AppendLine($"# The level (play starts in {pack.Setting.StartYear}; the level is written for month {level.Month}, with variants for months {string.Join(", ", level.MonthVariants.Select(v => v.Month))})");
        context.AppendLine(PromptBuilder.Json(new
        {
            level.Name,
            level.Region,
            level.SettlementType,
            level.Size,
            level.Terrain,
            level.Description,
            level.Travel,
            level.PublicFacts,
            places = level.Places.Select(p => new { p.Id, p.Name, p.OwnerRole }),
        }));
        context.AppendLine();
        context.AppendLine("# Period transport (lineId values) and institutions");
        context.AppendLine(PromptBuilder.Json(new { pack.Setting.PeriodTransport, pack.Setting.RegionalInstitutions }));
        var instruction = $"""
            # Place this level in the real country

            geography:
            - realAnalogue: the real place it is closest to in history, industry and landscape, as "Place (district)". It
              grounds the routes and distances; it is never shown to players. fictional: true unless the level is that place.
            - province, county, parish.
            - knownFor: two to four things an educated Swede of {pack.Setting.StartYear} would know of the place or district before arriving.
            - neighbours: the nearest market town, the county seat and any other town the case may need (a works, a
              mining town, a port): name, kind, a rough distance and direction by road or water, and what each offers
              (market, bank, newspaper, steamers, railway from a year).
            - authorities: where the crown sheriff (or police), the district court and its sessions, the provincial doctor,
              the post office and how often the post goes, the telegraph, the nearest bank and newspaper are, and how far.
              Use the institutions above where they fit. Empty only if the level truly has none within reach.

            journey, from Uppsala, valid for {pack.Setting.StartYear}:
            - two routes where the country allows (by road, and by rail or water), each as legs: from, to, mode, lineId for
              railway, steamer and canal legs (an id from the period transport, open by {pack.Setting.StartYear}), duration,
              cost in the period's money if known, schedule, months it runs (empty for all year; a winter sleigh road over the
              ice only in the frozen months), notes. A line not open yet is only a note ("rail from 1873").
            - Every month the level is written for must have at least one route.
            - journeyScene: three or four sentences of the journey for the game master to narrate on the way.

            travel: the journey again in one line, agreeing with the legs.
            """;
        var reply = await model.GenerateAsync<LevelPlacement>(Request(options.Value, BuildSystem(pack), context.ToString(), instruction), cancellationToken);
        return reply.Success ? ret.Ok(reply.Result!.Value) : ret.Fail(reply.ErrorMessage, reply.ErrorCode);
    }

    /// <summary>Checks a new record against the pack it would join; returns only its own issues.</summary>
    public static List<CoherenceIssue> Check(ContentPack pack, ContentRecord record)
    {
        var copy = pack.Usable(includeDrafts: true);
        switch (record)
        {
            case VaesenDefinition v:
                copy.Vaesen = copy.Vaesen.Where(x => x.Id != v.Id).Append(v).ToList();
                break;
            case ConflictArchetype c:
                copy.Conflicts = copy.Conflicts.Where(x => x.Id != c.Id).Append(c).ToList();
                break;
            case CharacterDefinition ch:
                copy.Characters = copy.Characters.Where(x => x.Id != ch.Id).Append(ch).ToList();
                break;
        }

        var prefix = record switch
        {
            VaesenDefinition => "vaesen:",
            ConflictArchetype => "conflict:",
            CharacterDefinition => "character:",
            _ => "content:",
        } + record.Id;
        return ContentValidator.Validate(copy).Where(i => i.Location.StartsWith(prefix, StringComparison.Ordinal)).ToList();
    }

    private static ModelRequest Request(MysteryOptions o, string system, string context, string instruction) => new()
    {
        Step = "content",
        Model = o.CreativeModel,
        Effort = o.CreativeEffort,
        MaxTokens = o.MaxOutputTokens,
        System = system,
        Context = context,
        Instruction = instruction,
    };

    private static string BuildSystem(ContentPack pack)
    {
        var s = new StringBuilder();
        s.AppendLine($"You write canon content for a mystery engine set in {pack.Setting.Era}: structured records the engine reasons over and the owner reviews before use.");
        s.AppendLine("Draw on your own knowledge of Scandinavian folklore and the history and daily life of the period. Write originally; never quote or closely paraphrase any published game text.");
        s.AppendLine();
        s.AppendLine("World truths nothing may contradict:");
        foreach (var truth in pack.Setting.WorldTruths)
        {
            s.AppendLine($"- {truth}");
        }

        s.AppendLine();
        s.AppendLine("Creatures need: deep lore rich and varied enough that cases built on it diverge (8-12 items); common knowledge that misleads (4-6); giveaway tells that identify it on sight (3-6); ambiguous signs that fit it and several other beings, each naming what else it fits (8-12), described in plain mundane terms across many registers.");
        s.AppendLine("Every affordance tag must come from the vocabulary given. Answer with the JSON the schema asks for and nothing else.");
        return s.ToString();
    }

    private static string BuildContext(ContentPack pack, string kind)
    {
        var s = new StringBuilder();
        s.AppendLine("# Affordance vocabulary (tag ids)");
        s.AppendLine(string.Join(", ", pack.Vocabulary.Tags.Select(t => t.Id)));
        s.AppendLine();
        s.AppendLine("# Conflict tags in use");
        s.AppendLine(string.Join(", ", pack.Conflicts.SelectMany(c => c.Tags).Distinct().Order()));
        s.AppendLine();
        s.AppendLine("# Existing records of this kind (ids taken; the first is an example of the format)");
        object? example = kind switch
        {
            "vaesen" => pack.Vaesen.FirstOrDefault(),
            "conflict" => pack.Conflicts.FirstOrDefault(),
            "character" => pack.Characters.FirstOrDefault(),
            _ => null,
        };
        var ids = kind switch
        {
            "vaesen" => pack.Vaesen.Select(v => v.Id),
            "conflict" => pack.Conflicts.Select(c => c.Id),
            "character" => pack.Characters.Select(c => c.Id),
            _ => [],
        };
        s.AppendLine(string.Join(", ", ids));
        if (example is not null)
        {
            s.AppendLine();
            s.AppendLine(PromptBuilder.Json(example));
        }

        return s.ToString();
    }
}
