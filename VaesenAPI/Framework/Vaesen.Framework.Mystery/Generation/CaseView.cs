using Vaesen.Framework.Mystery.Models;

namespace Vaesen.Framework.Mystery.Generation;

/// <summary>
/// The part of the case so far each step needs (fixes 2, section 5), instead of the whole packet every time: later
/// steps would otherwise send tens of thousands of tokens of channels and minds they never use. The critic and the
/// structural checks always see the whole case.
/// </summary>
public static class CaseView
{
    /// <summary>The view for <paramref name="step"/>; null when there is nothing to show yet (the conflict).</summary>
    public static object? For(string step, CasePacket p) => step switch
    {
        ConflictStep.StepName => null,
        EscalationStep.StepName => new { truth = Truth(p, withEscalation: false) },
        HumansStep.StepName => new { truth = Truth(p, withEscalation: true) },
        FactsStep.StepName => new { truth = Truth(p, withEscalation: true), cast = Cast(p, withSecrets: true) },
        MindsStep.StepName => new
        {
            truth = new { p.Truth.Summary, p.Truth.PlotCentre, p.Truth.HiddenHandMotive },
            escalation = Events(p),
            facts = p.Facts,
            decoys = p.Decoys,
            cast = Cast(p, withSecrets: true),
        },
        ArcStep.StepName => new
        {
            truth = new { p.Truth.Summary, p.Truth.Provocation, p.Truth.ProvocationPlaceId, p.Truth.PlotCentre, p.Truth.HiddenHandMotive },
            escalation = Events(p),
            facts = p.Facts.Select(f => new { f.Id, f.Statement, f.Kind, f.LoadBearing, f.PointsTo }),
            places = p.Channels.Select(c => c.PlaceId).OfType<string>().Distinct(),
            cast = Cast(p, withSecrets: false, withActions: true),
        },
        HookStep.StepName => new
        {
            truth = new { p.Truth.Summary, p.Truth.Focus },
            openingEvents = p.Truth.Escalation.Where(e => e.Opening),
            openingFacts = p.Facts.Where(f => p.Truth.Escalation.Exists(e => e.Opening && e.FactIds.Contains(f.Id))),
            otherFacts = p.Facts.Select(f => new { f.Id, f.Kind, f.Giveaway }),
            cast = p.Cast.Select(c => new { c.CharacterId, c.Name, c.Handle, c.PlotRole, c.Importance }),
            placesReferenced = PlacesReferenced(p),
        },
        _ => p,
    };

    private static object Truth(CasePacket p, bool withEscalation) => withEscalation
        ? p.Truth
        : new { p.Truth.Summary, p.Truth.ReasonKind, p.Truth.Origin, p.Truth.PlotCentre, p.Truth.Focus, p.Truth.Provocation, p.Truth.ProvocationPlaceId, p.Truth.HiddenHandMotive, p.Truth.DecoyReading };

    private static object Events(CasePacket p) =>
        p.Truth.Escalation.Select(e => new { e.Id, e.Description, e.When, e.PlaceId, e.Opening, e.FactIds });

    /// <summary>Who the people are, without the voice notes, routines and pressure actions later steps do not use.</summary>
    private static object Cast(CasePacket p, bool withSecrets, bool withActions = false) => p.Cast.Select(c => new
    {
        c.CharacterId,
        c.Name,
        c.Handle,
        c.PlotRole,
        c.Importance,
        c.InstanceRef,
        c.HomePlaceId,
        c.HomeAffordanceId,
        occupation = c.Identity?.Occupation,
        secrets = withSecrets ? c.Secrets : null,
        guilt = withSecrets ? c.Wants.Guilt : null,
        permittedActions = withActions ? c.Wants.PermittedActions.Select(a => new { a.Id, a.Desperation, a.Description }) : null,
    });

    /// <summary>Every place a channel, a beat or the climax uses: what the scenes are written for.</summary>
    private static List<string> PlacesReferenced(CasePacket p) => p.Channels.Select(c => c.PlaceId)
        .Concat(p.Beats.Select(b => b.Requires.Place))
        .Append(p.Climax.Conditions.Place)
        .Append(p.Truth.ProvocationPlaceId)
        .OfType<string>()
        .Where(id => id.Length > 0)
        .Distinct(StringComparer.Ordinal)
        .ToList();
}
