using System.Globalization;
using System.Text;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>What the roster call asks for: the people of a level, for one Society.</summary>
public sealed class RosterOutput
{
    [JsonProperty("people")]
    public List<RosterPerson> People { get; set; } = [];
}

public sealed class RosterPerson
{
    /// <summary>"npc-" and a slug of the name.</summary>
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [IdsOf(IdKinds.Place)]
    [JsonProperty("placeId")]
    public string PlaceId { get; set; } = "";

    [OneOf(typeof(RosterRoles))]
    [JsonProperty("role")]
    public string Role { get; set; } = RosterRoles.Resident;

    /// <summary>For the household: whose household (the owner's or keeper's id) and what they are to them.</summary>
    [JsonProperty("householdOf")]
    public string? HouseholdOf { get; set; }

    [JsonProperty("relation")]
    public string Relation { get; set; } = "";

    [JsonProperty("age")]
    public int Age { get; set; }

    [JsonProperty("gender")]
    public string Gender { get; set; } = "";

    [JsonProperty("occupation")]
    public string Occupation { get; set; } = "";

    [JsonProperty("rank")]
    public string Rank { get; set; } = "";

    /// <summary>One line: who they are here, what people say of them.</summary>
    [JsonProperty("line")]
    public string Line { get; set; } = "";

    [JsonProperty("voice")]
    public string Voice { get; set; } = "";

    /// <summary>For residents and some others: a thread a later case may pull.</summary>
    [JsonProperty("latentHook")]
    public string? LatentHook { get; set; }
}

/// <summary>
/// A level's people for one Society (fixes 4, section 12.1), written on the small model the first time a case uses the
/// level: the keeper of every business, the owner household of every farm and manor, the clergy, the local official,
/// the old-ways person and a few residents. Later cases there draw on them instead of inventing a new town.
/// </summary>
public sealed class RosterBuilder(IMysteryModel model, MysteryOptions options)
{
    public const string StepName = "roster";

    /// <summary>The roster and its people, checked; the call is logged in <paramref name="log"/>.</summary>
    public async Task<ResultBase<(LevelRoster Roster, List<CharacterInstance> People)>> BuildAsync(
        ContentPack pack, LevelDefinition level, SocietyWorld world, List<GenerationLogEntry> log, int caseAttempt, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<(LevelRoster, List<CharacterInstance>)>();
        var system = PromptBuilder.System(pack.Setting);
        var context = Context(level);
        var instruction = PromptBuilder.Fill(PromptBuilder.Template("roster"), new Dictionary<string, string>
        {
            ["ranks"] = string.Join(", ", pack.Setting.PeriodConventions.SocialRanks),
            ["usedNames"] = PromptBuilder.UsedNames(world, level.Id),
        });

        List<CoherenceIssue> issues = [];
        string? previous = null;
        for (var attempt = 0; attempt <= options.MaxStepRetries; attempt++)
        {
            var entry = new GenerationLogEntry { CaseAttempt = caseAttempt, Step = StepName, Attempt = attempt, Kind = LogEntryKinds.Full };
            log.Add(entry);
            entry.Instruction = PromptBuilder.WithFeedback(instruction, issues, previous);
            var reply = await model.GenerateAsync<RosterOutput>(new ModelRequest
            {
                Step = StepName,
                Model = string.IsNullOrWhiteSpace(options.LightModel) ? options.CreativeModel : options.LightModel,
                Effort = options.LightEffort,
                MaxTokens = options.PatchMaxOutputTokens,
                System = system,
                Context = context,
                Instruction = entry.Instruction,
                Cache = attempt > 0,
                Ids = new Dictionary<string, IReadOnlyList<string>> { [IdKinds.Place] = level.Places.Select(p => p.Id).ToList() },
            }, cancellationToken);

            if (!reply.Success)
            {
                entry.Error = reply.ErrorMessage;
                if (reply.ErrorCode is ModelErrorCodes.BadOutput or ModelErrorCodes.Truncated)
                {
                    issues = [CoherenceIssue.Error(IssueCodes.Schema, StepName, reply.ErrorMessage, "Answer with complete JSON that fits the schema; be more concise.")];
                    previous = null;
                    continue;
                }

                return ret.Fail(reply.ErrorMessage, reply.ErrorCode);
            }

            var answer = reply.Result!;
            entry.Model = answer.Model;
            entry.InputTokens = answer.InputTokens;
            entry.OutputTokens = answer.OutputTokens;
            entry.CacheReadTokens = answer.CacheReadTokens;
            entry.CacheWriteTokens = answer.CacheWriteTokens;
            entry.LatencyMs = answer.LatencyMs;
            entry.PrefixHash = answer.PrefixHash;
            entry.CostUsd = ModelPrices.Cost(answer.Model, answer.InputTokens, answer.CacheWriteTokens, answer.CacheReadTokens, answer.OutputTokens);
            entry.Output = answer.RawJson;

            issues = Validate(answer.Value, level, world);
            entry.Issues = issues;
            entry.Passed = !issues.Exists(i => i.IsError);
            if (entry.Passed)
            {
                var people = answer.Value.People.Select(p => ToCharacter(p, level, answer.Value.People)).ToList();
                return ret.Ok((new LevelRoster
                {
                    LevelId = level.Id,
                    CreatedAt = DateTime.UtcNow,
                    CharacterIds = people.Select(p => p.Id).ToList(),
                    Log = $"{answer.Model}, {answer.InputTokens} in, {answer.OutputTokens} out, attempt {attempt + 1}",
                }, people));
            }

            issues = issues.Where(i => i.IsError).ToList();
            previous = answer.RawJson;
        }

        return ret.Fail($"The people of {level.Name} could not be written: " + string.Join(" ", issues.Take(5)), StepErrorCodes.StepFailed);
    }

    /// <summary>
    /// Every place with an owner role has exactly one person holding it; ids are new and unique; names are easy to tell
    /// apart from each other and from the Society's own people.
    /// </summary>
    public static List<CoherenceIssue> Validate(RosterOutput output, LevelDefinition level, SocietyWorld world)
    {
        var issues = new List<CoherenceIssue>();
        var ids = new HashSet<string>(StringComparer.Ordinal);
        foreach (var p in output.People)
        {
            var at = $"roster:{p.Id}";
            if (!p.Id.StartsWith("npc-", StringComparison.Ordinal) || !ids.Add(p.Id) || world.Find(p.Id) is not null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, $"The id '{p.Id}' must start with npc-, be unique and not be anyone's in the Society's world.", "Use npc- and a slug of the name."));
            }

            if (level.FindPlace(p.PlaceId) is null)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, at, $"{p.Name}: no place '{p.PlaceId}' in {level.Name}."));
            }

            if (!RosterRoles.All.Contains(p.Role))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.Schema, at, $"Unknown role '{p.Role}'.{CoherenceIssue.Allowed(RosterRoles.All)}"));
            }

            if (p.Role == RosterRoles.Household && !output.People.Exists(o => o.Id == p.HouseholdOf && o.Id != p.Id))
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.UnknownReference, at, $"{p.Name} is of a household, but householdOf '{p.HouseholdOf}' is no one on the roster.", "Give the id of the owner or keeper whose household it is."));
            }
        }

        foreach (var place in level.Places.Where(p => !string.IsNullOrWhiteSpace(p.OwnerRole)))
        {
            var holders = output.People.Where(p => p.PlaceId == place.Id && RosterRoles.Holds(p.Role)).ToList();
            if (holders.Count == 0)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, $"place:{place.Id}", $"No one holds {place.Name} ({place.OwnerRole}).", "Add its owner or keeper (role owner, keeper, clergy or oldWays)."));
            }
            else if (holders.Count > 1)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, $"place:{place.Id}", $"{place.Name} has {holders.Count} holders: {string.Join(", ", holders.Select(h => h.Name))}.", "Keep one as its owner or keeper; make the others household."));
            }
        }

        var others = world.NamesThatMatter(level.Id).ToList();
        for (var i = 0; i < output.People.Count; i++)
        {
            var person = output.People[i];
            foreach (var other in output.People.Skip(i + 1))
            {
                if (NameRules.Clash(person.Name, other.Name) is { } why)
                {
                    issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, $"roster:{other.Id}", $"{person.Name} and {other.Name} have {why}.", $"Rename {other.Name}, keeping the id."));
                }
            }

            if (others.Find(o => NameRules.FirstNameClash(person.Name, o.Name) is not null) is { } clash)
            {
                issues.Add(CoherenceIssue.Error(IssueCodes.CastCoherence, $"roster:{person.Id}", $"{person.Name} sounds like {clash.Name}, already in the Society's world.", $"Rename {person.Name}, keeping the id."));
            }
        }

        return issues;
    }

    /// <summary>A roster person as one of the Society's location residents.</summary>
    public static CharacterInstance ToCharacter(RosterPerson p, LevelDefinition level, List<RosterPerson> all)
    {
        var head = p.HouseholdOf is null ? null : all.Find(o => o.Id == p.HouseholdOf);
        return new CharacterInstance
        {
            Id = p.Id,
            Name = p.Name,
            Tier = WorldTiers.LocationResident,
            Importance = RosterRoles.Holds(p.Role) ? 1 : 0,
            Home = $"{level.Id}/{p.PlaceId}",
            HomeLevelId = level.Id,
            HomePlaceId = p.PlaceId,
            RosterRole = p.Role,
            Identity = new CharacterIdentity
            {
                Age = p.Age,
                Gender = p.Gender,
                Occupation = p.Occupation,
                Rank = p.Rank,
                Home = level.FindPlace(p.PlaceId)?.Name ?? p.PlaceId,
            },
            Voice = new CharacterVoice { Register = string.IsNullOrWhiteSpace(p.Voice) ? "plain speech" : p.Voice },
            Background = p.Line,
            LatentHooks = string.IsNullOrWhiteSpace(p.LatentHook) ? [] : [new LatentHook { Id = $"{p.Id}-hook-1", Kind = LatentHookKinds.Secret, Text = p.LatentHook }],
            Relationships = head is null ? [] : [new WorldRelationship { CharacterId = head.Id, Nature = p.Relation, Sentiment = 1 }],
        };
    }

    private static string Context(LevelDefinition level)
    {
        var s = new StringBuilder();
        s.AppendLine(CultureInfo.InvariantCulture, $"# The level: {level.Name}");
        s.AppendLine(CultureInfo.InvariantCulture, $"{level.Region}. A {level.SettlementType} of size {level.Size} and wealth {level.Wealth} (1-5). {level.Description}");
        s.AppendLine();
        s.AppendLine("# Its places");
        s.AppendLine(PromptBuilder.Json(level.Places.Select(p => new
        {
            id = p.Id,
            name = p.Name,
            kind = p.TemplateId,
            ownerRole = string.IsNullOrWhiteSpace(p.OwnerRole) ? null : p.OwnerRole,
            dwellings = p.AllAffordances().Where(a => Dwellings.Tags.Contains(a.Tag)).Select(a => a.Name),
        })));
        s.AppendLine();
        s.AppendLine("# Things everyone here knows");
        foreach (var fact in level.PublicFacts)
        {
            s.AppendLine(CultureInfo.InvariantCulture, $"- {fact}");
        }

        return s.ToString();
    }
}
