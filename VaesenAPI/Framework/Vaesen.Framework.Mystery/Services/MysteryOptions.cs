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

    /// <summary>Effort for the creative steps: low, medium, high, xhigh or max.</summary>
    public string CreativeEffort { get; set; } = "high";

    /// <summary>Model for the critic (judgement checks on the whole case).</summary>
    public string CriticModel { get; set; } = "claude-opus-5-5";

    public string CriticEffort { get; set; } = "high";

    public int MaxOutputTokens { get; set; } = 64000;

    /// <summary>Run the model critic after the structural checks pass.</summary>
    public bool UseCritic { get; set; } = true;

    /// <summary>How many times a failing step is re-rolled with its issues fed back.</summary>
    public int MaxStepRetries { get; set; } = 2;

    /// <summary>How many times the whole case is re-rolled after a step gives up.</summary>
    public int MaxCaseRetries { get; set; } = 1;

    /// <summary>How many recent mysteries a creature is kept out of.</summary>
    public int VaesenCooldown { get; set; } = 3;

    /// <summary>low, medium or high: how much misdirection a case carries (spec 7.7).</summary>
    public string RedHerringDensity { get; set; } = "low";

    /// <summary>The starting in-world month when the Society has no calendar yet.</summary>
    public int DefaultMonth { get; set; } = 10;
}
