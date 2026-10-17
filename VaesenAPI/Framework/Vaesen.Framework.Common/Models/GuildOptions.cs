namespace Vaesen.Framework.Common.Models;

/// <summary>The "Guilds" configuration section: game rules for guilds.</summary>
public sealed class GuildOptions
{
    public const string SectionName = "Guilds";

    /// <summary>
    /// How long claiming a character (Play) keeps it from other members. Claiming it again renews the claim; after
    /// this long without a renewal the character is free again, so one that's left behind (game closed, crash)
    /// doesn't stay locked. Until game servers renew claims while the player is in a world, this is simply the time
    /// since the player pressed Play.
    /// </summary>
    public TimeSpan CharacterClaimTimeout { get; set; } = TimeSpan.FromMinutes(30);
}
