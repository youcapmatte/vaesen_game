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

        // Each step sees only the canon it works from.
        Assert.DoesNotContain("# The level", model.Contexts[model.Calls.IndexOf("facts")], StringComparison.Ordinal);
        Assert.DoesNotContain("# The creature", model.Contexts[model.Calls.IndexOf("hook")], StringComparison.Ordinal);
        Assert.DoesNotContain("# Persistent characters", model.Contexts[model.Calls.IndexOf("minds")], StringComparison.Ordinal);
        Assert.Contains("# Persistent characters", model.Contexts[model.Calls.IndexOf("humans")], StringComparison.Ordinal);
        Assert.DoesNotContain("\"reveal\"", model.Instructions[model.Calls.IndexOf("hook")], StringComparison.Ordinal);

        // Order is fixed, and the critic's objection to the hook sent back only the hook sub-step.
        Assert.Equal(["world", "conflict", "escalation", "humans", "facts", "minds", "arc", "hook", "critic", "hook", "critic"], model.Calls);
        Assert.Contains(result.Result.Case.Log, e => e.Step == "critic" && !e.Passed);
        Assert.Contains(packet.Facts, f => f.Giveaway && f.Id == "f8");
        Assert.Contains(packet.Facts, f => f.DeepLore && f.Id == "f7");
        Assert.Equal(packet.Facts.Count, packet.Tension.FactWeights.Count);

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

        // The whole-case check found the missing channel and sent back only minds and channels.
        Assert.Equal(["world", "conflict", "escalation", "humans", "facts", "minds", "arc", "hook", "minds"], model.Calls);
        Assert.Contains("redundancy", model.Instructions[^1], StringComparison.Ordinal);
        var failed = result.Result!.Case.Log.Single(e => e.Step == MysteryGenerator.CheckStepName && !e.Passed);
        Assert.Equal(LogEntryKinds.Check, failed.Kind);
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

        // Minds and channels was sent back twice; the steps that passed were never redone, and the case was not re-rolled.
        Assert.Equal(3, log.Count(e => e.Step == "minds"));
        Assert.Equal(3, log.Count(e => e.Step == MysteryGenerator.CheckStepName));
        Assert.Equal(1, log.Count(e => e.Step == "facts"));
        Assert.Equal([0], log.Select(e => e.CaseAttempt).Distinct());
        Assert.Equal(MysteryGenerator.CheckStepName, result.Result.Case.FailedStep);
        Assert.Contains(result.Result.Case.Packet.Validation.Issues, i => i.Code == IssueCodes.Redundancy);
    }

    [Fact]
    public async Task One_bad_value_is_patched_with_one_small_call_not_a_full_retry()
    {
        var options = new MysteryOptions { UseCritic = false };
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.BadFactKindOnce = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });

        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(["world", "conflict", "escalation", "humans", "facts", "facts", "minds", "arc", "hook"], model.Calls);
        var patch = result.Result!.Case.Log.Single(e => e.Step == "facts" && e.Kind == LogEntryKinds.Patch);
        Assert.True(patch.Passed);
        Assert.Contains("# Patch", patch.Instruction, StringComparison.Ordinal);
        Assert.Contains("Allowed: symptom, cause, identity", patch.Instruction, StringComparison.Ordinal);
        Assert.Equal(FactKinds.Colour, result.Result.Case.Packet.FindFact("f10")!.Kind);
    }

    [Fact]
    public async Task A_focus_that_does_not_suit_the_creature_changes_only_the_focus()
    {
        var options = new MysteryOptions { UseCritic = false };
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.FocusUnsuitableOnce = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });

        // The same brief with another focus: the conflict ran again, nothing was re-rolled.
        Assert.Equal(["world", "conflict", "conflict", "escalation", "humans", "facts", "minds", "arc", "hook"], model.Calls);
        Assert.Contains("- Focus: what", model.Contexts[1], StringComparison.Ordinal);
        Assert.Contains("- Focus: how", model.Contexts[2], StringComparison.Ordinal);
        Assert.Equal(CaseFocus.How, result.Result!.Case.Brief.Focus);
        Assert.Equal([0], result.Result.Case.Log.Select(e => e.CaseAttempt).Distinct());
    }

    [Fact]
    public async Task The_critic_sends_the_case_back_once_then_its_objections_are_warnings()
    {
        var options = new MysteryOptions();
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.CriticAlwaysObjects = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });

        // One rework of the clues, never back to the conflict although the critic names the truth; then the case passes.
        Assert.True(result.Success, result.ErrorMessage);
        Assert.Equal(["world", "conflict", "escalation", "humans", "facts", "minds", "arc", "hook", "critic", "minds", "critic"], model.Calls);
        var packet = result.Result!.Case.Packet;
        Assert.True(packet.Validation.Passed);
        var kept = Assert.Single(packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.WorldConsistency);
        Assert.False(kept.IsError);
        Assert.Equal(IssueSources.Critic, kept.Source);
    }

    [Fact]
    public async Task Critic_issues_are_fixed_in_the_steps_that_own_them_and_warnings_in_a_light_last_round()
    {
        var options = new MysteryOptions();
        var model = ScriptedModel.FromSample(criticFailsFirst: false);
        model.TargetedCritic = true;
        var (generator, _, _) = Build(options, model);

        var result = await generator.GenerateAsync(new GenerationRequest
        {
            History = History, Month = 10, Seed = MatchingSeed(options), VaesenId = "nack", LevelId = "level-forsby-october", Store = false,
        });

        Assert.True(result.Success, result.ErrorMessage);

        // The objection is split: the motive to the conflict, the channel to minds and channels, the warning on r1 to the
        // arc, in pipeline order. Nothing downstream is regenerated. The second review's warning goes to the arc, light.
        Assert.Equal(["world", "conflict", "escalation", "humans", "facts", "minds", "arc", "hook", "critic", "conflict", "minds", "arc", "critic", "arc"], model.Calls);
        var log = result.Result!.Case.Log;
        var afterReview = log.SkipWhile(e => e.Step != "critic").Skip(1).Where(e => e.Kind != LogEntryKinds.Check).ToList();
        Assert.Equal([LogEntryKinds.Fix, LogEntryKinds.Patch, LogEntryKinds.Patch], afterReview.Take(3).Select(e => e.Kind));
        Assert.Contains("# Revise: conflict", afterReview[0].Instruction, StringComparison.Ordinal);
        Assert.Equal(options.LightModel, model.Models[^1]);
        var packet = result.Result.Case.Packet;
        Assert.True(packet.Validation.Passed);
        Assert.True(Assert.Single(packet.Validation.Issues, i => i.Code == "critic." + CriticChecks.Solves).Applied);
    }

    /// <summary>Answers each step from the sample case; can break the clue step to exercise retries.</summary>
    public sealed class ScriptedModel(CasePacket sample, bool criticFailsFirst) : IMysteryModel
    {
        private int _criticCalls;
        private int _mindsCalls;

        public List<string> Calls { get; } = [];

        public List<string> Instructions { get; } = [];

        public List<string> Contexts { get; } = [];

        public List<string> Models { get; } = [];

        /// <summary>The first critic objects with targets in two steps and a warning; the second leaves one warning to apply.</summary>
        public bool TargetedCritic { get; set; }

        /// <summary>Answers the seeding step with this instead of <see cref="Seed"/>, once.</summary>
        public WorldSeedOutput? FirstSeed { get; set; }

        /// <summary>Leaves out channel c10 in the first minds-and-channels answer.</summary>
        public bool BreakFirstClues { get; set; }

        /// <summary>Leaves out channel c10 in every minds-and-channels answer.</summary>
        public bool AlwaysBreakClues { get; set; }

        /// <summary>Gives fact f10 the invented kind "lore" in the first facts answer; a patch then answers f10 alone.</summary>
        public bool BadFactKindOnce { get; set; }

        /// <summary>The first conflict answer says the focus does not suit the creature.</summary>
        public bool FocusUnsuitableOnce { get; set; }

        /// <summary>The critic always objects on world consistency, naming the truth among its locations.</summary>
        public bool CriticAlwaysObjects { get; set; }

        public static ScriptedModel FromSample(bool criticFailsFirst) => new(ValidatorTests.SampleCase(), criticFailsFirst);

        public Task<ResultBase<ModelReply<T>>> GenerateAsync<T>(ModelRequest request, CancellationToken cancellationToken = default) where T : class
        {
            Calls.Add(request.Step);
            Models.Add(request.Model);
            Instructions.Add(request.Instruction);
            Contexts.Add(request.Context);
            Assert.False(string.IsNullOrWhiteSpace(request.System));
            if (request.Step != WorldSeeder.StepName)
            {
                Assert.Contains("# The assignment", request.Context, StringComparison.Ordinal);
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
                "facts" => Facts(request.Instruction),
                "minds" => Minds(),
                "arc" => new ArcOutput { Beats = sample.Beats, Climax = sample.Climax, ResolutionLevers = sample.ResolutionLevers },
                "hook" => new HookOutput
                {
                    Hook = new HookDraft
                    {
                        HookGiverId = sample.Hook.HookGiverId, Text = sample.Hook.Text,
                        AssertedFactIds = sample.Hook.AssertedFactIds, OpeningEventIds = sample.Hook.OpeningEventIds,
                    },
                    PlacesUsed = sample.Level.PlacesUsed,
                    SceneDescriptions = sample.Level.SceneDescriptions.Select(p => new SceneDescription { PlaceId = p.Key, Text = p.Value }).ToList(),
                },
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

        private ConflictOutput Conflict()
        {
            if (FocusUnsuitableOnce)
            {
                FocusUnsuitableOnce = false;
                return new ConflictOutput { FocusUnsuitable = true, FocusUnsuitableReason = "Its identity is plain from the first signs." };
            }

            return ConflictFromSample();
        }

        private ConflictOutput ConflictFromSample() => new()
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
            CharacterId = c.CharacterId, Name = c.Name, InstanceRef = c.InstanceRef, PlotRole = c.PlotRole, Importance = c.Importance,
            RoleSlotId = c.RoleSlotId, Identity = c.Identity, HomePlaceId = c.HomePlaceId, Wants = c.Wants, Secrets = c.Secrets,
            Routine = c.Routine, VoiceNotes = c.VoiceNotes, Relationships = c.Relationships,
        };

        private FactsOutput Facts(string instruction)
        {
            var facts = sample.Facts.Select(f => new FactDraft
            {
                Id = f.Id, Statement = f.Statement, LoadBearing = f.LoadBearing, Kind = f.Kind, PointsTo = f.PointsTo, LoreIds = f.LoreIds,
            }).ToList();
            if (instruction.Contains("# Patch", StringComparison.Ordinal))
            {
                return new FactsOutput { Facts = facts.Where(f => f.Id == "f10").ToList() };
            }

            if (BadFactKindOnce)
            {
                BadFactKindOnce = false;
                facts.Single(f => f.Id == "f10").Kind = "lore";
            }

            return new FactsOutput
            {
                Facts = facts,
                Decoys = sample.Decoys,
                EventFacts = sample.Truth.Escalation.Select(e => new EventFacts { EventId = e.Id, FactIds = e.FactIds }).ToList(),
                SeasonThreads = sample.SeasonThreads,
            };
        }

        private MindsOutput Minds()
        {
            _mindsCalls++;
            var output = new MindsOutput
            {
                Minds = sample.Cast.Select(c => new CastMind { CharacterId = c.CharacterId, Knows = c.Knows, Thinks = c.Thinks, Lies = c.Lies }).ToList(),
                Channels = sample.Channels.ToList(),
            };

            if (AlwaysBreakClues || (BreakFirstClues && _mindsCalls == 1))
            {
                output.Channels.RemoveAll(c => c.Id == "c10");
            }

            return output;
        }

        private CriticOutput Critic()
        {
            _criticCalls++;
            if (TargetedCritic)
            {
                CriticCheck Check(string name, bool passed, string severity, List<string> targets) => new()
                {
                    Check = name, Passed = passed, Severity = severity, Location = string.Join(", ", targets), Targets = targets,
                    Explanation = passed ? "" : "Needs a change.", SuggestedFix = passed ? null : "Change it as described.",
                };
                var first = _criticCalls == 1;
                return new CriticOutput
                {
                    Overall = "Nearly there.",
                    Checks = CriticChecks.All.Select(name => name switch
                    {
                        CriticChecks.FairClues when first => Check(name, false, IssueSeverities.Error, ["c10", "truth.hiddenHandMotive"]),
                        CriticChecks.WorldConsistency when first => Check(name, false, IssueSeverities.Warning, ["r1"]),
                        CriticChecks.Solves when !first => Check(name, false, IssueSeverities.Warning, ["r2"]),
                        _ => Check(name, true, IssueSeverities.Error, []),
                    }).ToList(),
                };
            }

            if (CriticAlwaysObjects)
            {
                return new CriticOutput
                {
                    Overall = "Close, but not there.",
                    Checks = CriticChecks.All.Select(name => new CriticCheck
                    {
                        Check = name,
                        Passed = name != CriticChecks.WorldConsistency,
                        Severity = IssueSeverities.Error,
                        Location = "truth.summary; npc-per-holm",
                        Explanation = name == CriticChecks.WorldConsistency ? "The mill's age does not match the summary." : "",
                    }).ToList(),
                };
            }

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
