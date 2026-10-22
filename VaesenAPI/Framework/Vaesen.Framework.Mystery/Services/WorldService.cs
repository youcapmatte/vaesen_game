using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Generation;
using Vaesen.Framework.Mystery.Models;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Services;

/// <summary>How the host closes a mystery: the outcome and the lever the Society pulled.</summary>
public sealed class CloseMysteryRequest
{
    /// <summary>"solved" or "failed" (<see cref="CaseStatuses"/>).</summary>
    [JsonProperty("outcome")]
    public string Outcome { get; set; } = CaseStatuses.Solved;

    /// <summary>The resolution lever, if the Society pulled one.</summary>
    [JsonProperty("leverId")]
    public string? LeverId { get; set; }

    /// <summary>The host's own words on how it ended; kept with the case and in the memories it writes.</summary>
    [JsonProperty("notes")]
    public string Notes { get; set; } = "";
}

public interface IWorldService
{
    Task<ResultBase<SocietyWorld>> GetAsync(string societyId, CancellationToken cancellationToken = default);

    /// <summary>The Society's world; seeds it first if this is the Society's first time (spec 9.1).</summary>
    Task<ResultBase<SocietyWorld>> EnsureSeededAsync(SocietyHistory history, ContentPack pack, IProgress<GenerationProgress>? progress = null, CancellationToken cancellationToken = default);

    /// <summary>Closes a stored mystery and writes it back to the Society's world (spec 9.5). Stores the case's new status.</summary>
    Task<ResultBase<StoredCase>> CloseMysteryAsync(string societyId, string mysteryId, CloseMysteryRequest request, CancellationToken cancellationToken = default);
}

public sealed class WorldService(
    IWorldStore worlds,
    ICasePacketStore cases,
    IContentStore content,
    IMysteryModel model,
    IOptions<MysteryOptions> options,
    ILogger<WorldService> logger) : IWorldService
{
    private static readonly SemaphoreSlim SeedGate = new(1, 1);

    public Task<ResultBase<SocietyWorld>> GetAsync(string societyId, CancellationToken cancellationToken = default) =>
        worlds.GetAsync(societyId, cancellationToken);

    public async Task<ResultBase<SocietyWorld>> EnsureSeededAsync(
        SocietyHistory history, ContentPack pack, IProgress<GenerationProgress>? progress = null, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyWorld>();
        var existing = await worlds.GetAsync(history.SocietyId, cancellationToken);
        if (existing.Success || existing.ErrorCode != ErrorCodes.NotFound)
        {
            return existing;
        }

        await SeedGate.WaitAsync(cancellationToken);
        try
        {
            // Someone else may have seeded it while this waited.
            existing = await worlds.GetAsync(history.SocietyId, cancellationToken);
            if (existing.Success)
            {
                return existing;
            }

            progress?.Report(new GenerationProgress(0, WorldSeeder.StepName, "Seeding the Society's world: its own people, met again and again."));
            var seeded = await new WorldSeeder(model, options.Value).SeedAsync(pack, history, cancellationToken);
            if (!seeded.Success)
            {
                return ret.Fail(seeded.ErrorMessage, seeded.ErrorCode);
            }

            logger.LogInformation("Seeded the world of {SocietyId}: {Count} characters", history.SocietyId, seeded.Result!.Characters.Count);
            return await worlds.SaveAsync(seeded.Result!, cancellationToken);
        }
        finally
        {
            SeedGate.Release();
        }
    }

    public async Task<ResultBase<StoredCase>> CloseMysteryAsync(string societyId, string mysteryId, CloseMysteryRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<StoredCase>();
        if (request.Outcome is not (CaseStatuses.Solved or CaseStatuses.Failed))
        {
            return ret.Fail("The outcome must be solved or failed.", ErrorCodes.Invalid);
        }

        var stored = await cases.GetAsync(societyId, mysteryId, cancellationToken);
        if (!stored.Success)
        {
            return ret.Fail(stored.ErrorMessage, stored.ErrorCode);
        }

        var storedCase = stored.Result!;
        if (storedCase.Closing is not null)
        {
            return ret.Fail("This mystery is already closed.", ErrorCodes.Conflict);
        }

        if (!storedCase.Packet.Validation.Passed)
        {
            return ret.Fail("A case that failed its checks was never played and cannot be closed.", ErrorCodes.Invalid);
        }

        var lever = request.LeverId is null ? null : storedCase.Packet.ResolutionLevers.Find(l => l.Id == request.LeverId);
        if (request.LeverId is not null && lever is null)
        {
            return ret.Fail($"The case has no lever '{request.LeverId}'.", ErrorCodes.Invalid);
        }

        var world = await worlds.GetAsync(societyId, cancellationToken);
        if (!world.Success)
        {
            return ret.Fail(world.ErrorMessage, world.ErrorCode);
        }

        var changes = WorldWriteBack.Apply(world.Result!, storedCase, request, lever);
        var pack = await content.GetPackAsync(options.Value.SettingId, cancellationToken);
        if (pack.Success)
        {
            var errors = WorldValidator.Validate(world.Result!, pack.Result!.Usable(includeDrafts: true)).Where(i => i.IsError).ToList();
            if (errors.Count > 0)
            {
                return ret.Fail("The write-back would leave the world inconsistent: " + string.Join(" ", errors.Take(3)), ErrorCodes.Invalid);
            }
        }

        var saved = await worlds.SaveAsync(world.Result!, cancellationToken);
        if (!saved.Success)
        {
            return ret.Fail(saved.ErrorMessage, saved.ErrorCode);
        }

        storedCase.Status = request.Outcome;
        storedCase.Closing = new CaseClosing
        {
            ClosedAt = DateTime.UtcNow,
            Outcome = request.Outcome,
            LeverId = lever?.Id,
            SolveDepth = lever?.SolveDepth,
            Notes = request.Notes,
            Changes = changes,
        };
        return await cases.SaveAsync(storedCase, cancellationToken);
    }
}

/// <summary>
/// What a closed mystery writes back to the Society's world (spec 9.5): the lever's attitude changes, fates and world
/// consequences; memories and knowledge for persistent characters who took part; and the promotion of the case's own
/// people into the persistent cast, so they can be met again.
/// </summary>
public static class WorldWriteBack
{
    public static List<string> Apply(SocietyWorld world, StoredCase stored, CloseMysteryRequest request, ResolutionLever? lever)
    {
        var p = stored.Packet;
        var changes = new List<string>();
        var solved = request.Outcome == CaseStatuses.Solved;
        var when = $"{MonthName(p.Brief.Month)}, {p.Title}";
        var ending = solved
            ? lever is null ? "The Society solved it." : $"The Society ended it: {lever.Description}"
            : "The Society did not resolve it.";

        foreach (var member in p.Cast)
        {
            var person = world.Find(member.CharacterId);
            if (person is null)
            {
                person = Promote(member, p);
                world.Characters.Add(person);
                changes.Add($"{person.Name} joins the Society's world ({person.Tier}).");
            }
            else if (member.InstanceRef == InstanceRefs.Promoted && person.Tier == WorldTiers.Background)
            {
                person.Tier = WorldTiers.Core;
                changes.Add($"{person.Name} rises from the background into the core cast.");
            }

            if (!person.MysteryIds.Contains(p.MysteryId))
            {
                person.MysteryIds.Add(p.MysteryId);
            }

            if (member.Tier == CastTiers.Significant && person.Importance < 3)
            {
                person.Importance++;
            }

            person.Memories.Add(new MemoryEntry
            {
                MysteryId = p.MysteryId,
                When = when,
                Text = $"Was {member.PlotRole} when the Society came. {ending}{(string.IsNullOrWhiteSpace(request.Notes) ? "" : " " + request.Notes)}",
            });

            foreach (var known in member.Knows)
            {
                var fact = p.FindFact(known.FactId);
                if (fact is not null && !person.Knowledge.Exists(k => k.MysteryId == p.MysteryId && k.FactId == fact.Id))
                {
                    person.Knowledge.Add(new WorldKnowledge { MysteryId = p.MysteryId, FactId = fact.Id, Statement = fact.Statement, Source = known.Source });
                }
            }

            foreach (var belief in member.Thinks.Where(b => !person.Beliefs.Contains(b.Belief)))
            {
                person.Beliefs.Add(belief.Belief);
            }
        }

        // Ties to people of the case who are not in the world (never cast) cannot be kept.
        foreach (var person in world.Characters)
        {
            person.Relationships.RemoveAll(r => world.Find(r.CharacterId) is null);
        }

        // The hook-giver asked for help: they remember whether they got it.
        var giver = world.Find(p.Hook.HookGiverId);
        if (giver is not null)
        {
            Shift(giver, solved ? 1 : -1, solved ? $"The Society answered their call in {p.Title}." : $"The Society came, but did not end the trouble in {p.Title}.", changes);
        }

        if (lever is not null)
        {
            foreach (var change in lever.Consequences.Attitudes)
            {
                var person = world.Find(change.CharacterId);
                if (person is not null && change.Delta != 0)
                {
                    Shift(person, change.Delta, change.Change, changes);
                }
            }

            foreach (var fate in lever.Consequences.Fates)
            {
                var person = world.Find(fate.CharacterId);
                if (person is not null && fate.Status is CharacterStatuses.Dead or CharacterStatuses.Departed)
                {
                    person.Status = fate.Status;
                    person.Memories.Add(new MemoryEntry { MysteryId = p.MysteryId, When = when, Text = fate.Reason });
                    changes.Add($"{person.Name} is {(fate.Status == CharacterStatuses.Dead ? "dead" : "gone")}: {fate.Reason}");
                }
            }

            foreach (var line in lever.Consequences.World)
            {
                world.Overlay.Add(new WorldOverlayEntry { MysteryId = p.MysteryId, LevelId = p.Brief.LevelId, Text = line, At = DateTime.UtcNow });
                changes.Add($"The world: {line}");
            }
        }

        world.ClosedMysteryIds.Add(p.MysteryId);
        return changes;
    }

    /// <summary>A case's own person, now part of the Society's world: residents stay with their place, the rest join the background.</summary>
    private static CharacterInstance Promote(CastMember member, CasePacket p)
    {
        var resident = member.Tier == CastTiers.Resident;
        var pivotal = member.CharacterId == p.Hook.HookGiverId || member.CharacterId == p.Truth.PlotCentre.CharacterId;
        return new CharacterInstance
        {
            Id = member.CharacterId,
            Name = member.Name,
            Tier = resident ? WorldTiers.LocationResident : pivotal ? WorldTiers.Core : WorldTiers.Background,
            Importance = 0,
            Home = member.HomePlaceId is null ? p.Brief.LevelId : $"{p.Brief.LevelId}/{member.HomePlaceId}",
            Identity = member.Identity ?? new CharacterIdentity(),
            Drives = (member.Wants.Drives.Count > 0 ? member.Wants.Drives : [$"to get on with life after {p.Title}"])
                .Select((d, i) => new Drive { Id = $"{member.CharacterId}-drive-{i + 1}", Text = d, Strength = 2 })
                .ToList(),
            Voice = new CharacterVoice { Register = string.IsNullOrWhiteSpace(member.VoiceNotes) ? "plain speech" : member.VoiceNotes },
            Background = member.Routine,
            LatentHooks = member.Secrets.Select((secret, i) => new LatentHook { Id = $"{member.CharacterId}-secret-{i + 1}", Kind = LatentHookKinds.Secret, Text = secret }).ToList(),
            Relationships = member.Relationships
                .Select(r => new WorldRelationship { CharacterId = r.CharacterId, Nature = r.Nature, Sentiment = Math.Clamp(r.Sentiment, -3, 3) })
                .ToList(),
            AttitudeReason = $"Met the Society in {p.Title}.",
        };
    }

    private static void Shift(CharacterInstance person, int delta, string reason, List<string> changes)
    {
        var before = person.Attitude;
        person.Attitude = Math.Clamp(person.Attitude + delta, -3, 3);
        if (person.Attitude != before)
        {
            person.AttitudeReason = reason;
            changes.Add($"{person.Name}: attitude {before:+0;-0;0} → {person.Attitude:+0;-0;0} ({reason})");
        }
    }

    private static string MonthName(int month) =>
        month is >= 1 and <= 12 ? System.Globalization.CultureInfo.InvariantCulture.DateTimeFormat.GetMonthName(month) : $"month {month}";
}
