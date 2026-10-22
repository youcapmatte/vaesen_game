using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Validation;

/// <summary>
/// The case as a graph (spec 6.3): facts are nodes, channels are edges into them with access conditions, and beats
/// put facts into the world once tension is high enough. <see cref="Walk"/> simulates players who follow every open
/// channel until nothing new opens; the structural checks are built on it.
/// </summary>
public sealed class FactGraph
{
    /// <summary>Weight of a fact without one in <see cref="CaseTension.FactWeights"/>.</summary>
    public const double DefaultFactWeight = 0.04;

    private readonly CasePacket _packet;
    private readonly Dictionary<string, AccessRequirement?[]> _requirements;

    public FactGraph(CasePacket packet)
    {
        _packet = packet;
        _requirements = packet.Channels
            .GroupBy(c => c.Id, StringComparer.Ordinal)
            .ToDictionary(g => g.Key, g => g.First().Access.Requires.Select(AccessRequirement.Parse).ToArray(), StringComparer.Ordinal);
    }

    /// <summary>What players know before they go anywhere: the facts the hook states and its opening events.</summary>
    public HashSet<string> OpeningKnowledge()
    {
        var known = new HashSet<string>(_packet.Hook.AssertedFactIds, StringComparer.Ordinal);
        foreach (var eventId in _packet.Hook.OpeningEventIds)
        {
            var opening = _packet.Truth.Escalation.Find(e => e.Id == eventId);
            if (opening is not null)
            {
                known.UnionWith(opening.FactIds);
            }
        }

        return known;
    }

    /// <summary>Facts a channel needs known before it opens ("knowsFact:f7").</summary>
    public IEnumerable<string> FactGates(CaseChannel channel) =>
        (_requirements.TryGetValue(channel.Id, out var reqs) ? reqs : [])
            .Where(r => r?.Kind == AccessRequirement.KnowsFact)
            .Select(r => r!.Value);

    /// <summary>
    /// Walks the graph from the opening knowledge to a fixed point. A channel opens when every fact it is gated on is
    /// known; trust, rolls, times and places are taken as reachable (a failed roll is rerouted, spec 4.6). A beat fires
    /// when the tension the known facts add up to reaches its threshold.
    /// </summary>
    /// <param name="channelFilter">Only channels for which this is true can be used (null: all).</param>
    /// <param name="gateFilter">Only facts for which this is true can open gates (null: all) - e.g. not decoy facts.</param>
    public WalkResult Walk(Func<CaseChannel, bool>? channelFilter = null, Func<string, bool>? gateFilter = null, bool useBeats = true)
    {
        var known = OpeningKnowledge();
        var order = known.ToList();
        var usedChannels = new HashSet<string>(StringComparer.Ordinal);
        var firedBeats = new HashSet<string>(StringComparer.Ordinal);

        var changed = true;
        while (changed)
        {
            changed = false;
            foreach (var channel in _packet.Channels)
            {
                if (usedChannels.Contains(channel.Id) || (channelFilter is not null && !channelFilter(channel)))
                {
                    continue;
                }

                if (FactGates(channel).All(f => known.Contains(f) && (gateFilter is null || gateFilter(f))))
                {
                    usedChannels.Add(channel.Id);
                    if (known.Add(channel.FactId))
                    {
                        order.Add(channel.FactId);
                    }

                    changed = true;
                }
            }

            if (!useBeats)
            {
                continue;
            }

            var tension = Tension(known);
            foreach (var beat in _packet.Beats.Where(b => !firedBeats.Contains(b.Id) && b.Threshold <= tension + 1e-9))
            {
                firedBeats.Add(beat.Id);
                foreach (var fact in beat.EmitsFactIds.Where(known.Add))
                {
                    order.Add(fact);
                }

                changed = true;
            }
        }

        return new WalkResult(known, order, usedChannels, firedBeats, Tension(known));
    }

    /// <summary>The tension dial once <paramref name="known"/> are uncovered.</summary>
    public double Tension(IEnumerable<string> known) =>
        _packet.Tension.Start + known.Sum(f => _packet.Tension.FactWeights.TryGetValue(f, out var w) ? w : DefaultFactWeight);

    /// <summary>
    /// Who or what a channel comes from, so two channels count as independent only if their sources differ: a person,
    /// or a spot in a place. Beats are their own source.
    /// </summary>
    public static string SourceKey(CaseChannel channel) => channel.Kind switch
    {
        ChannelKinds.Testimony or ChannelKinds.Overheard when !string.IsNullOrEmpty(channel.SourceCharacterId) => $"person:{channel.SourceCharacterId}",
        ChannelKinds.Trace or ChannelKinds.Document when !string.IsNullOrEmpty(channel.PlaceId) => $"spot:{channel.PlaceId}/{channel.AffordanceId}",
        _ when !string.IsNullOrEmpty(channel.SourceCharacterId) => $"person:{channel.SourceCharacterId}",
        _ when !string.IsNullOrEmpty(channel.PlaceId) => $"spot:{channel.PlaceId}/{channel.AffordanceId}",
        _ => $"channel:{channel.Id}",
    };

    /// <summary>The independent sources of a fact: its channels' sources plus every beat that emits it.</summary>
    public HashSet<string> SourcesOf(string factId)
    {
        var sources = _packet.Channels.Where(c => c.FactId == factId).Select(SourceKey).ToHashSet(StringComparer.Ordinal);
        foreach (var beat in _packet.Beats.Where(b => b.EmitsFactIds.Contains(factId)))
        {
            sources.Add($"beat:{beat.Id}");
        }

        return sources;
    }
}

/// <summary>The result of a walk: what was learned, in what order, through which channels and beats.</summary>
public sealed record WalkResult(
    HashSet<string> Known,
    List<string> Order,
    HashSet<string> UsedChannels,
    HashSet<string> FiredBeats,
    double FinalTension);
