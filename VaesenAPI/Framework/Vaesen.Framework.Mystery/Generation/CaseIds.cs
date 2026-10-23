using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The ids a step may refer to, by <see cref="IdKinds"/> kind, for its output schema's enums (fixes 3, section 1). A
/// step gets only kinds an earlier step fixed: the facts step names its own facts, so its fact ids stay open.
/// </summary>
public static class CaseIds
{
    public static Dictionary<string, IReadOnlyList<string>> For(MysteryDraft draft, string step)
    {
        var packet = draft.Packet;
        var places = draft.Level.Places.Select(p => p.Id).ToList();
        var ids = new Dictionary<string, IReadOnlyList<string>>(StringComparer.Ordinal) { [IdKinds.Place] = places };
        if (step == HumansStep.StepName)
        {
            ids[IdKinds.Affordance] = draft.Level.Places.SelectMany(p => p.AllAffordances()).Where(a => Dwellings.Tags.Contains(a.Tag)).Select(a => a.Id).ToList();
        }

        if (step == FactsStep.StepName)
        {
            ids[IdKinds.Event] = packet.Truth.Escalation.Select(e => e.Id).ToList();
        }

        if (step == CreatureStep.StepName)
        {
            ids[IdKinds.Fact] = packet.Facts.Select(f => f.Id).ToList();
            ids[IdKinds.Deed] = packet.ResolutionLevers.Where(l => l.SolveDepth == SolveDepths.Deep).SelectMany(l => l.RequiredDeeds).Select(d => d.Id).ToList();
        }

        if (step is MindsStep.StepName or ArcStep.StepName or HookStep.StepName)
        {
            var facts = packet.Facts.Select(f => f.Id).ToList();
            ids[IdKinds.Fact] = facts;
            ids[IdKinds.Event] = packet.Truth.Escalation.Select(e => e.Id).ToList();
            ids[IdKinds.Cast] = packet.Cast.Select(c => c.CharacterId).ToList();
            ids[IdKinds.Affordance] = draft.Level.Places.SelectMany(p => p.AllAffordances()).Select(a => a.Id).ToList();
            ids[IdKinds.Requirement] = AccessRequirement.Allowed(facts, places);
        }

        return ids;
    }
}
