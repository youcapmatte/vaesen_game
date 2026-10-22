using System.Text;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>What the seeding call asks the model for: the Society's own core and background cast.</summary>
public sealed class WorldSeedOutput
{
    [JsonProperty("core")]
    public List<SeedCharacter> Core { get; set; } = [];

    [JsonProperty("background")]
    public List<SeedCharacter> Background { get; set; } = [];
}

public sealed class SeedCharacter
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("home")]
    public string Home { get; set; } = "";

    [JsonProperty("importance")]
    public int Importance { get; set; } = 1;

    [JsonProperty("identity")]
    public CharacterIdentity Identity { get; set; } = new();

    [JsonProperty("personality")]
    public PersonalityProfile Personality { get; set; } = new();

    [JsonProperty("drives")]
    public List<Drive> Drives { get; set; } = [];

    [JsonProperty("voice")]
    public CharacterVoice Voice { get; set; } = new();

    [JsonProperty("background")]
    public string Background { get; set; } = "";

    [JsonProperty("latentHooks")]
    public List<LatentHook> LatentHooks { get; set; } = [];

    [JsonProperty("networkLinks")]
    public List<NetworkLink> NetworkLinks { get; set; } = [];

    [JsonProperty("attitude")]
    public int Attitude { get; set; }

    [JsonProperty("attitudeReason")]
    public string AttitudeReason { get; set; } = "";

    [JsonProperty("relationships")]
    public List<WorldRelationship> Relationships { get; set; } = [];
}

/// <summary>
/// World seeding (spec 9.1), run once per Society before its first mystery: every canonical character instantiated for
/// the Society, then a core and a background cast written by the model, tied together and checked by
/// <see cref="WorldValidator"/>. A failing seed is re-rolled with its issues fed back.
/// </summary>
public sealed class WorldSeeder(IMysteryModel model, MysteryOptions options)
{
    public const string StepName = "world";

    public async Task<ResultBase<SocietyWorld>> SeedAsync(ContentPack pack, SocietyHistory history, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyWorld>();
        var canonical = pack.Characters.Select(Instantiate).ToList();
        var system = PromptBuilder.System(pack.Setting);
        var context = Context(pack, canonical);
        var instruction = PromptBuilder.Fill(PromptBuilder.Template("world"), new Dictionary<string, string>
        {
            ["coreCount"] = options.CoreCastSize.ToString(System.Globalization.CultureInfo.InvariantCulture),
            ["backgroundCount"] = options.BackgroundCastSize.ToString(System.Globalization.CultureInfo.InvariantCulture),
            ["institutions"] = string.Join(", ", pack.Setting.Institutions.Select(i => i.Id)),
            ["society"] = $"{history.SocietyName} ({history.SocietyId}). Investigators: {(history.Investigators.Count == 0 ? "not known yet" : string.Join("; ", history.Investigators))}.",
        });

        var log = new List<string>();
        List<CoherenceIssue> issues = [];
        string? previous = null;
        for (var attempt = 0; attempt <= options.MaxStepRetries; attempt++)
        {
            var reply = await model.GenerateAsync<WorldSeedOutput>(new ModelRequest
            {
                Step = StepName,
                Model = options.CreativeModel,
                Effort = options.CreativeEffort,
                MaxTokens = options.MaxOutputTokens,
                System = system,
                Context = context,
                Instruction = PromptBuilder.WithFeedback(instruction, issues, previous),
            }, cancellationToken);

            if (!reply.Success)
            {
                log.Add($"attempt {attempt + 1}: {reply.ErrorMessage}");
                if (reply.ErrorCode is ModelErrorCodes.BadOutput or ModelErrorCodes.Truncated)
                {
                    issues = [CoherenceIssue.Error(IssueCodes.Schema, StepName, reply.ErrorMessage, "Answer with complete JSON that fits the schema; be more concise if needed.")];
                    previous = null;
                    continue;
                }

                return ret.Fail(reply.ErrorMessage, reply.ErrorCode);
            }

            var answer = reply.Result!;
            var world = new SocietyWorld
            {
                SocietyId = history.SocietyId,
                SettingId = pack.Setting.Id,
                SeededAt = DateTime.UtcNow,
                Characters = canonical
                    .Concat(answer.Value.Core.Select(c => FromSeed(c, WorldTiers.Core)))
                    .Concat(answer.Value.Background.Select(c => FromSeed(c, WorldTiers.Background)))
                    .ToList(),
            };

            issues = WorldValidator.Validate(world, pack);
            if (answer.Value.Core.Count < options.CoreCastSize / 2)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, "core", $"Only {answer.Value.Core.Count} core characters; {options.CoreCastSize} were asked for."));
            }

            log.Add($"attempt {attempt + 1}: {answer.Model}, {answer.InputTokens} in ({answer.CacheReadTokens} cached), {answer.OutputTokens} out, {answer.LatencyMs} ms, " +
                $"{answer.Value.Core.Count} core, {answer.Value.Background.Count} background, {issues.Count(i => i.IsError)} errors");
            if (!issues.Exists(i => i.IsError))
            {
                world.SeedLog = log;
                return ret.Ok(world);
            }

            issues = issues.Where(i => i.IsError).ToList();
            previous = answer.RawJson;
        }

        return ret.Fail("The world could not be seeded: " + string.Join(" ", issues.Take(5)), StepErrorCodes.StepFailed);
    }

    /// <summary>A Society's own copy of a canonical character, with the attitude every Society starts from.</summary>
    public static CharacterInstance Instantiate(CharacterDefinition c) => new()
    {
        Id = c.Id,
        DefinitionId = c.Id,
        Name = c.Name,
        Tier = WorldTiers.Canonical,
        Importance = 2,
        Home = c.Identity.Home,
        Identity = c.Identity,
        Personality = c.Personality,
        Drives = c.Drives,
        Voice = c.Voice,
        Background = c.Background,
        LatentHooks = c.LatentHooks,
        NetworkLinks = c.NetworkLinks,
        Attitude = c.StartingAttitude,
        AttitudeReason = c.AttitudeReason,
    };

    private static CharacterInstance FromSeed(SeedCharacter c, string tier) => new()
    {
        Id = c.Id,
        Name = c.Name,
        Tier = tier,
        Importance = Math.Clamp(c.Importance, 0, 3),
        Home = c.Home,
        Identity = c.Identity,
        Personality = c.Personality,
        Drives = c.Drives,
        Voice = c.Voice,
        Background = c.Background,
        LatentHooks = c.LatentHooks,
        NetworkLinks = c.NetworkLinks,
        Attitude = c.Attitude,
        AttitudeReason = c.AttitudeReason,
        Relationships = c.Relationships,
    };

    private static string Context(ContentPack pack, List<CharacterInstance> canonical)
    {
        var s = new StringBuilder();
        s.AppendLine("# Institutions");
        foreach (var i in pack.Setting.Institutions)
        {
            s.AppendLine($"- {i.Id}: {i.Name}. {i.Description}");
        }

        s.AppendLine();
        s.AppendLine("# Canonical characters (already in every Society's world)");
        s.AppendLine(PromptBuilder.Json(canonical.Select(c => new
        {
            id = c.Id,
            name = c.Name,
            c.Identity.Occupation,
            c.Home,
            attitudeToSociety = c.Attitude,
            c.AttitudeReason,
            latentHooks = c.LatentHooks.Select(h => h.Text),
        })));
        return s.ToString();
    }
}
