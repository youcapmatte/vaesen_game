namespace Vaesen.Framework.Mystery;

/// <summary>Configuration section "Mystery": where content lives, which models to use and how hard to try.</summary>
public sealed class MysteryOptions
{
    public const string SectionName = "Mystery";

    /// <summary>The setting whose content pack to use (Content/{SettingId}/).</summary>
    public string SettingId { get; set; } = "vaesen";

    /// <summary>The content root. Empty: the Content folder next to the library.</summary>
    public string ContentPath { get; set; } = "";

    /// <summary>Let generation use content the owner has not approved yet. On while developing, off for real play.</summary>
    public bool UseDraftContent { get; set; }

    /// <summary>Where case packets are stored by the file store. Empty: .data/cases under the working directory.</summary>
    public string CasePacketPath { get; set; } = "";

    /// <summary>Where Society worlds are stored by the file store. Empty: .data/worlds under the working directory.</summary>
    public string WorldPath { get; set; } = "";

    /// <summary>Seed the Society's world (persistent cast) before its first mystery.</summary>
    public bool SeedWorld { get; set; } = true;

    /// <summary>How many core characters a new world gets besides the canonical ones (spec 9.1: around a dozen).</summary>
    public int CoreCastSize { get; set; } = 10;

    /// <summary>How many background characters a new world gets.</summary>
    public int BackgroundCastSize { get; set; } = 14;

    /// <summary>How many persistent characters a case is offered to draw from, the most important first.</summary>
    public int PersistentCastOffered { get; set; } = 24;

    /// <summary>The Anthropic API key. Empty: the SDK's own lookup (ANTHROPIC_API_KEY and so on).</summary>
    public string AnthropicApiKey { get; set; } = "";

    /// <summary>Model for the creative steps.</summary>
    public string CreativeModel { get; set; } = "claude-opus-5-5";

    /// <summary>
    /// Effort (low, medium, high, xhigh or max) for the calls that decide the case: the conflict, and the first try of
    /// the facts.
    /// </summary>
    public string CreativeEffort { get; set; } = "high";

    /// <summary>
    /// Effort for the first try of the other steps: escalation, humans, minds and channels (bookkeeping the checks verify:
    /// at high effort it spent more than half its output thinking), the arc. Also the critic's second look.
    /// </summary>
    public string StandardEffort { get; set; } = "medium";

    /// <summary>Effort for retries after the first, and for the hook and scenes.</summary>
    public string LightEffort { get; set; } = "low";

    /// <summary>A smaller model for retries after the first; empty: <see cref="CreativeModel"/>.</summary>
    public string RetryModel { get; set; } = "";

    /// <summary>The model for patch calls; empty: <see cref="CreativeModel"/>.</summary>
    public string PatchModel { get; set; } = "";

    public string PatchEffort { get; set; } = "low";

    /// <summary>The small model for the cheap last round that applies the critic's remaining warnings.</summary>
    public string LightModel { get; set; } = "claude-haiku-5-5";

    /// <summary>Output cap (thinking included) of a patch call.</summary>
    public int PatchMaxOutputTokens { get; set; } = 16000;

    /// <summary>A step is patched instead of regenerated when its field-level errors touch at most this share of its items.</summary>
    public double PatchShare { get; set; } = 0.4;

    /// <summary>Model for the critic (judgement checks on the whole case).</summary>
    public string CriticModel { get; set; } = "claude-opus-5-5";

    public string CriticEffort { get; set; } = "high";

    /// <summary>Output cap (thinking included) of a full step call.</summary>
    public int MaxOutputTokens { get; set; } = 64000;

    /// <summary>Run the model critic after the structural checks pass.</summary>
    public bool UseCritic { get; set; } = true;

    /// <summary>How many times a failing step is re-rolled with its issues fed back.</summary>
    public int MaxStepRetries { get; set; } = 2;

    /// <summary>
    /// How many times a case is re-rolled from a new brief, when the conflict step finds nothing plausible fits. A step
    /// that gives up does not re-roll the case: it starts again on its own (fixes, section 2.3).
    /// </summary>
    public int MaxCaseRetries { get; set; } = 1;

    /// <summary>How many times the finished case goes back to the sub-steps its whole-case or critic issues point at.</summary>
    public int MaxReworkRounds { get; set; } = 2;

    /// <summary>
    /// How many of those reworks the critic may cause. After that its objections are kept as warnings on a case that
    /// passes: judgement is advice to the host, and each rejection costs a large rework.
    /// </summary>
    public int MaxCriticReworks { get; set; } = 1;

    /// <summary>How many recent mysteries a creature is kept out of.</summary>
    public int VaesenCooldown { get; set; } = 3;

    /// <summary>
    /// Cache the system prompt and case context (review 2, item 7). Off: a step's prefix includes its schema, whose id
    /// lists change from call to call, and the critic's second look comes after the five-minute cache has gone, so the
    /// writes (at 1.25 times the input price) were never read.
    /// </summary>
    public bool PromptCache { get; set; }

    /// <summary>How many times the last round tries again a fix the verifying look found did not take (review 2, item 1).</summary>
    public int FixRetries { get; set; } = 1;

    /// <summary>
    /// A critic fix that still did not hold after the retries fails the case visibly (review 2, item 1); off, it stays a
    /// warning marked unresolved, never "applied".
    /// </summary>
    public bool UnresolvedFixIsError { get; set; } = true;

    /// <summary>How many of a level's people a case takes as its residents when the Society has a roster there.</summary>
    public int RosterResidents { get; set; } = 4;

    /// <summary>
    /// A level is not used again until the Society has finished this many other mysteries since (fixes 4, section 11);
    /// queued cases count as uses but not as finished.
    /// </summary>
    public int LevelCooldownMysteries { get; set; } = 3;

    /// <summary>...and this many in-world months have passed since.</summary>
    public int LevelCooldownMonths { get; set; } = 3;

    /// <summary>In-world months from one case to the next: the calendar only moves forward.</summary>
    public int MonthsBetweenCases { get; set; } = 1;

    /// <summary>How far ahead the calendar may skip looking for a month with a level and creature that fit.</summary>
    public int MonthsToSearch { get; set; } = 12;

    /// <summary>low, medium or high: how much misdirection a case carries (spec 7.7).</summary>
    public string RedHerringDensity { get; set; } = "low";

    /// <summary>The starting in-world month when the Society has no calendar yet.</summary>
    public int DefaultMonth { get; set; } = 10;
}
