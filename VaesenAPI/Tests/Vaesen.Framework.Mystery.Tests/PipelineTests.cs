using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Services;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Tests;

/// <summary>The whole pipeline against a scripted model that answers each step with a piece of the sample case.</summary>
public sealed class PipelineTests
{
    private static readonly SocietyHistory History = new() { SocietyId = "society-test", SocietyName = "Test Society" };

    /// <summary>A seed whose assignment matches the sample case: the Näck at Forsby, a violated domain, identity as the puzzle.</summary>
    internal static int MatchingSeed(MysteryOptions options)
    {
        var pack = ContentTests.LoadPack();
        pack.Vaesen = [pack.FindVaesen("nack")!];
        pack.Levels = [pack.FindLevel("level-forsby-october")!];
        return Enumerable.Range(0, 5000).First(seed =>
        {
            var b = Assignment.Assign(pack, History, 10, seed, options);
            return b.ReasonKind == ReasonKinds.DomainViolated && !b.HiddenHand && b.Focus == CaseFocus.What
                && b.ConflictArchetypeIds.Contains("industry-intrudes-on-domain");
        });
    }

    private static (MysteryGenerator generator, ScriptedModel model, FileCasePacketStore store) Build(MysteryOptions options, ScriptedModel model) =>
        Build(options, model, out _);

    public static (MysteryGenerator generator, ScriptedModel model, FileCasePacketStore store) Build(MysteryOptions options, ScriptedModel model, out WorldService worlds)
    {
        options.ContentPath = Path.Combine(AppContext.BaseDirectory, "Content");
        options.CasePacketPath = Directory.CreateTempSubdirectory("cases-").FullName;
        options.WorldPath = Directory.CreateTempSubdirectory("worlds-").FullName;
        options.UseDraftContent = true;
        options.CoreCastSize = 2;
        options.BackgroundCastSize = 1;
        var wrapped = Microsoft.Extensions.Options.Options.Create(options);
        var store = new FileCasePacketStore(wrapped);
        var content = new FileContentStore(wrapped);
        worlds = new WorldService(new FileWorldStore(wrapped), store, content, model, wrapped, NullLogger<WorldService>.Instance);
        var generator = new MysteryGenerator(content, store, model, new CaseValidator(), worlds, wrapped, NullLogger<MysteryGenerator>.Instance);
        return (generator, model, store);
    }

    [Fact]
    public async Task A_case_runs_through_every_step_in_order_and_is_stored()
    {
        var options = new MysteryOptions();
        var (generator, model, store) = Build(options, ScriptedModel.FromSample(criticFailsFirst: true));

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october",
        });

        Assert.True(result.Success, result.ErrorMessage);
        var packet = result.Result!.Case.Packet;
        Assert.True(packet.Validation.Passed);
        Assert.DoesNotContain(packet.Validation.Issues, i => i.IsError);
        Assert.Equal("The Silent Wheel at Forsby", packet.Title);
        Assert.Equal(17, packet.Facts.Count);
        Assert.Equal(result.Result.Case.Brief.HookChannelId, packet.Hook.Channel);

        // Order is fixed, and the critic's first objection sent the clues back for one more round.
        Assert.Equal(["world", "conflict", "escalation", "humans", "clues", "critic", "clues", "critic"], model.Calls);
        Assert.Contains(result.Result.Case.Log, e => e.Step == "critic" && !e.Passed);

        var stored = await store.GetAsync("society-test", packet.MysteryId);
        Assert.True(stored.Success);
        Assert.Equal(packet.Title, stored.Result!.Packet.Title);
        var listed = await store.ListAsync("society-test");
        Assert.Single(listed.Result!);
    }

    [Fact]
    public async Task A_step_with_issues_is_retried_with_the_issues_fed_back()
    {
        var options = new MysteryOptions { UseCritic = false };
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.BreakFirstClues = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(["world", "conflict", "escalation", "humans", "clues", "clues"], model.Calls);
        Assert.Contains("redundancy", model.Instructions[^1], StringComparison.Ordinal);
        var failed = result.Result!.Case.Log.Single(e => e.Step == "clues" && !e.Passed);
        Assert.Contains(failed.Issues, i => i.Code == IssueCodes.Redundancy);
    }

    [Fact]
    public async Task A_case_that_never_validates_fails_visibly_with_its_log()
    {
        var options = new MysteryOptions { UseCritic = false, MaxStepRetries = 1, MaxCaseRetries = 1 };
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.AlwaysBreakClues = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october",
        });

        Assert.False(result.Success);
        Assert.Equal(MysteryGenerator.GenerationFailed, result.ErrorCode);
        Assert.NotNull(result.Result);
        var log = result.Result!.Case.Log;
        Assert.Equal(2, log.Count(e => e.CaseAttempt == 0 && e.Step == "clues"));
        Assert.Equal([0, 1], log.Select(e => e.CaseAttempt).Distinct());
    }

    /// <summary>Answers each step from the sample case; can break the clue step to exercise retries.</summary>
    public sealed class ScriptedModel(CasePacket sample, bool criticFailsFirst) : IMysteryModel
    {
        private int _criticCalls;
        private int _clueCalls;

        public List<string> Calls { get; } = [];

        public List<string> Instructions { get; } = [];

        public List<string> Contexts { get; } = [];

        /// <summary>Answers the seeding step with this instead of <see cref="Seed"/>, once.</summary>
        public WorldSeedOutput? FirstSeed { get; set; }

        public bool BreakFirstClues { get; set; }

        public bool AlwaysBreakClues { get; set; }

        public static ScriptedModel FromSample(bool criticFailsFirst) => new(ValidatorTests.SampleCase(), criticFailsFirst);

        public Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default) where T : class
        {
            Calls.Add(request.Step);
            Instructions.Add(request.Instruction);
            Contexts.Add(request.Context);
            Assert.False(string.IsNullOrWhiteSpace(request.System));
            if (request.Step != WorldSeeder.StepName)
            {
                Assert.Contains("# The creature", request.Context, StringComparison.Ordinal);
            }

            object value = request.Step switch
            {
                WorldSeeder.StepName => TakeSeed(),
                "conflict" => Conflict(),
                "escalation" => new EscalationOutput
                {
                    Events = sample.Truth.Escalation.Select(e => new EscalationDraft
                    {
                        Id = e.Id, Description = e.Description, When = e.When, PlaceId = e.PlaceId, Register = e.Register, Opening = e.Opening,
                    }).ToList(),
                },
                "humans" => new HumansOutput { Cast = sample.Cast.Select(Draft).ToList() },
                "clues" => Clues(),
                "critic" => Critic(),
                _ => throw new InvalidOperationException(request.Step),
            };

            var json = JsonConvert.SerializeObject(value);
            return Task.FromResult(new ResultBase<ModelReply<T>>().Ok(new ModelReply<T>
            {
                Value = JsonConvert.DeserializeObject<T>(json)!, RawJson = json, Model = "scripted",
            }));
        }

        private WorldSeedOutput TakeSeed()
        {
            var seed = FirstSeed ?? Seed();
            FirstSeed = null;
            return seed;
        }

        /// <summary>A small world: two core people tied to a canonical one, and one background face.</summary>
        public static WorldSeedOutput Seed() => new()
        {
            Core =
            [
                SeedPerson("npc-sara-lindqvist", "Sara Lindqvist", 2, [new WorldRelationship { CharacterId = "constable-hagg", Nature = "his niece", Sentiment = 1 }]),
                SeedPerson("npc-otto-berg", "Otto Berg", -2, [new WorldRelationship { CharacterId = "npc-sara-lindqvist", Nature = "rival bookseller", Sentiment = -2 }]),
            ],
            Background = [SeedPerson("npc-gustav-ek", "Gustav Ek", 0, [])],
        };

        private static SeedCharacter SeedPerson(string id, string name, int attitude, List<WorldRelationship> ties) => new()
        {
            Id = id,
            Name = name,
            Home = "Uppsala",
            Importance = 2,
            Identity = new CharacterIdentity { Age = 40, Occupation = "bookseller", Home = "Uppsala" },
            Drives = [new Drive { Id = id + "-d1", Text = "to keep the shop", Strength = 2 }],
            Voice = new CharacterVoice { Register = "dry" },
            LatentHooks = [new LatentHook { Id = id + "-h1", Kind = LatentHookKinds.Debt, Text = "owes rent" }],
            Attitude = attitude,
            AttitudeReason = "first impressions",
            Relationships = ties,
        };

        private ConflictOutput Conflict() => new()
        {
            Title = sample.Title,
            ConflictArchetypeId = sample.Brief.ConflictArchetypeId,
            TruthSummary = sample.Truth.Summary,
            Provocation = sample.Truth.Provocation,
            ProvocationPlaceId = sample.Truth.ProvocationPlaceId,
            ProvocationAffordanceTag = sample.Truth.ProvocationAffordanceTag,
            DecoyReading = sample.Truth.DecoyReading,
        };

        private static CastDraft Draft(CastMember c) => new()
        {
            CharacterId = c.CharacterId, Name = c.Name, InstanceRef = c.InstanceRef, PlotRole = c.PlotRole, Tier = c.Tier,
            RoleSlotId = c.RoleSlotId, Identity = c.Identity, HomePlaceId = c.HomePlaceId, Wants = c.Wants, Secrets = c.Secrets,
            Routine = c.Routine, VoiceNotes = c.VoiceNotes, Relationships = c.Relationships,
        };

        private CluesOutput Clues()
        {
            _clueCalls++;
            var output = new CluesOutput
            {
                Facts = sample.Facts,
                Channels = sample.Channels,
                EventFacts = sample.Truth.Escalation.Select(e => new EventFacts { EventId = e.Id, FactIds = e.FactIds }).ToList(),
                Minds = sample.Cast.Select(c => new CastMind { CharacterId = c.CharacterId, Knows = c.Knows, Thinks = c.Thinks, Lies = c.Lies }).ToList(),
                Decoys = sample.Decoys,
                Beats = sample.Beats,
                Climax = sample.Climax,
                ResolutionLevers = sample.ResolutionLevers,
                Hook = new HookDraft
                {
                    HookGiverId = sample.Hook.HookGiverId, Text = sample.Hook.Text,
                    AssertedFactIds = sample.Hook.AssertedFactIds, OpeningEventIds = sample.Hook.OpeningEventIds,
                },
                PlacesUsed = sample.Level.PlacesUsed,
                SceneDescriptions = sample.Level.SceneDescriptions.Select(p => new SceneDescription { PlaceId = p.Key, Text = p.Value }).ToList(),
                TensionStart = sample.Tension.Start,
                FactWeights = sample.Tension.FactWeights.Select(p => new FactWeight { FactId = p.Key, Weight = p.Value }).ToList(),
            };

            if (AlwaysBreakClues || (BreakFirstClues && _clueCalls == 1))
            {
                output = JsonConvert.DeserializeObject<CluesOutput>(JsonConvert.SerializeObject(output))!;
                output.Channels.RemoveAll(c => c.Id == "c10");
            }

            return output;
        }

        private CriticOutput Critic()
        {
            _criticCalls++;
            var fail = criticFailsFirst && _criticCalls == 1;
            return new CriticOutput
            {
                Overall = "A fair identity case.",
                Checks = CriticChecks.All.Select(name => new CriticCheck
                {
                    Check = name,
                    Passed = !(fail && name == CriticChecks.Hook),
                    Severity = IssueSeverities.Error,
                    Location = "hook",
                    Explanation = fail && name == CriticChecks.Hook ? "The letter leans too hard on the drowning." : "",
                }).ToList(),
            };
        }
    }
}
