using System.Text;
using Microsoft.IdentityModel.Tokens;
using Newtonsoft.Json;

namespace Vaesen.Framework.Common.Models;

/// <summary>The "Dev" configuration section: seed data and signing key for the development-only login.</summary>
public sealed class DevOptions
{
    public const string SectionName = "Dev";

    /// <summary>HMAC key for dev tokens. Development only; at least 32 characters.</summary>
    public string SigningKey { get; set; } = "";

    public int TokenLifetimeHours { get; set; } = 12;

    public List<DevGuild> Guilds { get; set; } = [];

    public SymmetricSecurityKey GetSigningKey() => new(Encoding.UTF8.GetBytes(SigningKey));

    public (DevGuild? Guild, DevPlayer? Player) FindPlayer(string? playerId)
    {
        foreach (var guild in Guilds)
        {
            var player = guild.Players.Find(p => string.Equals(p.Id, playerId, StringComparison.OrdinalIgnoreCase));
            if (player is not null)
            {
                return (guild, player);
            }
        }

        return (null, null);
    }
}

public sealed class DevGuild
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";

    [JsonProperty("players")]
    public List<DevPlayer> Players { get; set; } = [];
}

public sealed class DevPlayer
{
    [JsonProperty("id")]
    public string Id { get; set; } = "";

    [JsonProperty("name")]
    public string Name { get; set; } = "";
}

public sealed class DevLoginRequest
{
    [JsonProperty("playerId")]
    public string? PlayerId { get; set; }
}

public sealed class DevLoginResult
{
    [JsonProperty("accessToken")]
    public string AccessToken { get; set; } = "";

    [JsonProperty("expiresIn")]
    public int ExpiresIn { get; set; }

    [JsonProperty("playerId")]
    public string PlayerId { get; set; } = "";

    [JsonProperty("playerName")]
    public string PlayerName { get; set; } = "";

    [JsonProperty("guildId")]
    public string GuildId { get; set; } = "";
}
