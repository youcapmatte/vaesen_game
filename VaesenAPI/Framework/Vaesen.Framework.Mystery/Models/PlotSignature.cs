using Newtonsoft.Json;

namespace Vaesen.Framework.Mystery.Models;

/// <summary>
/// A case's plot in a few values (fixes 7, section 4): the creature, why it came, and four axes read from the truth by
/// the small model. Two plots with the same creature provoked the same way, or the same on three axes, are a repeat.
/// Stored on the packet; an older case without one gets it when it is first remembered.
/// </summary>
public sealed class PlotSignature
{
    [JsonProperty("vaesenId")]
    public string VaesenId { get; set; } = "";

    [JsonProperty("reasonKind")]
    public string ReasonKind { get; set; } = "";

    [JsonProperty("conflictArchetypeId")]
    public string ConflictArchetypeId { get; set; } = "";

    /// <summary>One of <see cref="PlotProvocations"/>.</summary>
    [JsonProperty("provocation")]
    public string Provocation { get; set; } = PlotProvocations.Other;

    /// <summary>One of <see cref="PlotBonds"/>.</summary>
    [JsonProperty("bond")]
    public string Bond { get; set; } = PlotBonds.None;

    /// <summary>One of <see cref="PlotVictims"/>.</summary>
    [JsonProperty("victim")]
    public string Victim { get; set; } = PlotVictims.Other;

    /// <summary>One of <see cref="PlotCures"/>.</summary>
    [JsonProperty("cure")]
    public string Cure { get; set; } = PlotCures.Other;

    /// <summary>The plot in one line, with no names.</summary>
    [JsonProperty("oneLine")]
    public string OneLine { get; set; } = "";

    public override string ToString() => $"{VaesenId}: {Provocation}, bond {Bond}, victim {Victim}, cure {Cure}";
}

/// <summary>What set the creature off.</summary>
public static class PlotProvocations
{
    public const string Trespass = "trespass";
    public const string TakenFromIt = "takenFromIt";
    public const string HarmToItsPlace = "harmToItsPlace";
    public const string BrokenPromise = "brokenPromise";
    public const string BargainSought = "bargainSought";
    public const string BargainComesDue = "bargainComesDue";
    public const string Desire = "desire";
    public const string Revenge = "revenge";
    public const string Insult = "insult";
    public const string Neglect = "neglect";
    public const string Summoned = "summoned";
    public const string Hunger = "hunger";
    public const string Other = "other";

    public static readonly IReadOnlyList<string> All =
        [Trespass, TakenFromIt, HarmToItsPlace, BrokenPromise, BargainSought, BargainComesDue, Desire, Revenge, Insult, Neglect, Summoned, Hunger, Other];
}

/// <summary>What ties people to the creature.</summary>
public static class PlotBonds
{
    public const string None = "none";
    public const string Bargain = "bargain";
    public const string Debt = "debt";
    public const string Gift = "gift";
    public const string Lover = "lover";
    public const string Child = "child";
    public const string Oath = "oath";
    public const string Curse = "curse";
    public const string Holding = "holding";
    public const string Other = "other";

    public static readonly IReadOnlyList<string> All = [None, Bargain, Debt, Gift, Lover, Child, Oath, Curse, Holding, Other];
}

/// <summary>Who suffers most.</summary>
public static class PlotVictims
{
    public const string Child = "child";
    public const string YoungWoman = "youngWoman";
    public const string YoungMan = "youngMan";
    public const string Worker = "worker";
    public const string Household = "household";
    public const string Livestock = "livestock";
    public const string Community = "community";
    public const string Bargainer = "bargainer";
    public const string Outsider = "outsider";
    public const string Other = "other";

    public static readonly IReadOnlyList<string> All = [Child, YoungWoman, YoungMan, Worker, Household, Livestock, Community, Bargainer, Outsider, Other];
}

/// <summary>What the deep ending does.</summary>
public static class PlotCures
{
    public const string Restore = "restore";
    public const string Honour = "honour";
    public const string Release = "release";
    public const string Rite = "rite";
    public const string Truth = "truth";
    public const string Church = "church";
    public const string Ward = "ward";
    public const string Departure = "departure";
    public const string Sacrifice = "sacrifice";
    public const string Other = "other";

    public static readonly IReadOnlyList<string> All = [Restore, Honour, Release, Rite, Truth, Church, Ward, Departure, Sacrifice, Other];
}
