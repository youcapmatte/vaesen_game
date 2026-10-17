using System.Security.Claims;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Services;

/// <summary>
/// Development-only login: pick a guild and a player from seed data and get a token without a password.
/// The token is validated like an Auth0 token (see CommonServiceExtensions.AddDevServices), so every other
/// endpoint behaves exactly as in production.
/// </summary>
public interface IDevLoginService
{
    IReadOnlyList<DevGuild> GetGuilds();

    ResultBase<DevLoginResult> Login(DevLoginRequest request);
}

public sealed class DevLoginService : IDevLoginService
{
    public const string Issuer = "vaesen-dev";

    private readonly IOptions<DevOptions> _options;
    private readonly IOptions<Auth0Options> _auth0;
    private readonly TimeProvider _timeProvider;

    public DevLoginService(IOptions<DevOptions> options, IOptions<Auth0Options> auth0, TimeProvider timeProvider)
    {
        _options = options;
        _auth0 = auth0;
        _timeProvider = timeProvider;
    }

    public IReadOnlyList<DevGuild> GetGuilds() => _options.Value.Guilds;

    public ResultBase<DevLoginResult> Login(DevLoginRequest request)
    {
        var ret = new ResultBase<DevLoginResult>();
        var dev = _options.Value;

        var (guild, player) = dev.FindPlayer(request?.PlayerId);
        if (guild is null || player is null)
        {
            return ret.Fail($"Unknown dev player '{request?.PlayerId}'.", ErrorCodes.NotFound);
        }

        var subject = $"dev|{player.Id}";
        var now = _timeProvider.GetUtcNow().UtcDateTime;
        var expires = now.AddHours(dev.TokenLifetimeHours);
        var token = new JsonWebTokenHandler().CreateToken(new SecurityTokenDescriptor
        {
            Issuer = Issuer,
            Audience = _auth0.Value.Audience,
            Subject = new ClaimsIdentity(
            [
                new Claim("sub", subject),
                new Claim("name", player.Name),
                new Claim("guild", guild.Id),
            ]),
            IssuedAt = now,
            NotBefore = now,
            Expires = expires,
            SigningCredentials = new SigningCredentials(dev.GetSigningKey(), SecurityAlgorithms.HmacSha256),
        });

        return ret.Ok(new DevLoginResult
        {
            AccessToken = token,
            ExpiresIn = (int)(expires - now).TotalSeconds,
            PlayerId = subject,
            PlayerName = player.Name,
            GuildId = guild.Id,
        });
    }
}
