namespace Nexus.Table.Services;

/// <summary>The "Nexus" configuration section.</summary>
public sealed class NexusOptions
{
    public const string SectionName = "Nexus";

    /// <summary>The one Society of the first test. Created on first use with <see cref="SocietyName"/>.</summary>
    public string SocietyId { get; set; } = "society-00000000-0000-0000-0000-000000000001";

    public string SocietyName { get; set; } = "The Society";

    public string Headquarters { get; set; } = "A castle outside Upsala";

    /// <summary>
    /// Token subjects that get the host role, e.g. Mattias's Auth0 user_id ("oauth2|discord|...") or "dev|mattias".
    /// Only the host configures the AI and starts mysteries.
    /// </summary>
    public List<string> HostSubjects { get; set; } = [];

    /// <summary>
    /// In Development, the dev login's seed players ("dev|...") are let in without the host approving them. Tests turn
    /// it off to see an account wait.
    /// </summary>
    public bool AutoApproveDevPlayers { get; set; } = true;

    /// <summary>How many recent rolls the table log shows.</summary>
    public int RecentRolls { get; set; } = 50;

    /// <summary>
    /// Start writing the next mystery in the background as soon as one is closed, so a validated case is ready when the
    /// Society next sits down (generation takes many minutes, too long to wait for at the table).
    /// </summary>
    public bool PreGenerateNextCase { get; set; } = true;
}
