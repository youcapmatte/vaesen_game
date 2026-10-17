using System.Security.Claims;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Services;

/// <summary>
/// Development-only login: get a token without a password. The token is validated like an Auth0 token
/// (see CommonServiceExtensions.AddDevServices), so every other endpoint behaves exactly as in production.
///
/// Two ways in:
///   { "playerId": "ingrid" }                                 a seed player from the "Dev" config section
///   { "accountId": "account-...", "characterId": "character-..." }  a real account playing one of its guild's
///                                                            characters (it must be assigned to that account)
/// </summary>
public interface IDevLoginService
{
    IReadOnlyList<DevGuild> GetGuilds();

    Task<ResultBase<DevLoginResult>> LoginAsync(DevLoginRequest request, CancellationToken cancellationToken = default);
}

public sealed class DevLoginService : IDevLoginService
{
    public const string Issuer = "vaesen-dev";

    private readonly IOptions<DevOptions> _options;
    private readonly IOptions<Auth0Options> _auth0;
    private readonly TimeProvider _timeProvider;
    private readonly IAccountStore _accounts;
    private readonly ICharacterStore _characters;
    private readonly IGuildStore _guilds;

    public DevLoginService(
        IOptions<DevOptions> options,
        IOptions<Auth0Options> auth0,
        TimeProvider timeProvider,
        IAccountStore accounts,
        ICharacterStore characters,
        IGuildStore guilds)
    {
        _options = options;
        _auth0 = auth0;
        _timeProvider = timeProvider;
        _accounts = accounts;
        _characters = characters;
        _guilds = guilds;
    }

    public IReadOnlyList<DevGuild> GetGuilds() => _options.Value.Guilds;

    public async Task<ResultBase<DevLoginResult>> LoginAsync(DevLoginRequest request, CancellationToken cancellationToken = default)
    {
        if (!string.IsNullOrWhiteSpace(request?.CharacterId))
        {
            return await LoginAsCharacterAsync(request, cancellationToken).ConfigureAwait(false);
        }

        var ret = new ResultBase<DevLoginResult>();
        var (guild, player) = _options.Value.FindPlayer(request?.PlayerId);
        if (guild is null || player is null)
        {
            return ret.Fail($"Unknown dev player '{request?.PlayerId}'.", ErrorCodes.NotFound);
        }

        return ret.Ok(IssueToken($"dev|{player.Id}", player.Name, guild.Id, accountId: null, characterId: null));
    }

    private async Task<ResultBase<DevLoginResult>> LoginAsCharacterAsync(DevLoginRequest request, CancellationToken cancellationToken)
    {
        var ret = new ResultBase<DevLoginResult>();

        var account = Account.IsValidId<Account>(request.AccountId)
            ? await _accounts.GetAsync(request.AccountId!, cancellationToken).ConfigureAwait(false)
            : null;
        if (account is null)
        {
            return ret.Fail("Account not found.", ErrorCodes.NotFound);
        }

        var character = Character.IsValidId<Character>(request.CharacterId)
            ? await _characters.GetAsync(request.CharacterId!, cancellationToken).ConfigureAwait(false)
            : null;
        if (character is null)
        {
            return ret.Fail("Character not found.", ErrorCodes.NotFound);
        }

        var guild = await _guilds.GetAsync(character.GuildId, cancellationToken).ConfigureAwait(false);
        if (guild is null || !string.Equals(guild.GetPlayerOf(character.Id), account.Id, StringComparison.Ordinal))
        {
            return ret.Fail("This account doesn't play this character. Assign it first.", ErrorCodes.Forbidden);
        }

        return ret.Ok(IssueToken($"dev|{account.Id}", character.Name, guild.Id, account.Id, character.Id));
    }

    private DevLoginResult IssueToken(string subject, string name, string guildId, string? accountId, string? characterId)
    {
        var dev = _options.Value;
        var now = _timeProvider.GetUtcNow().UtcDateTime;
        var expires = now.AddHours(dev.TokenLifetimeHours);

        var claims = new List<Claim>
        {
            new("sub", subject),
            new("name", name),
            new("guild", guildId),
        };
        if (characterId is not null)
        {
            claims.Add(new Claim("character", characterId));
        }

        var token = new JsonWebTokenHandler().CreateToken(new SecurityTokenDescriptor
        {
            Issuer = Issuer,
            Audience = _auth0.Value.Audience,
            Subject = new ClaimsIdentity(claims),
            IssuedAt = now,
            NotBefore = now,
            Expires = expires,
            SigningCredentials = new SigningCredentials(dev.GetSigningKey(), SecurityAlgorithms.HmacSha256),
        });

        return new DevLoginResult
        {
            AccessToken = token,
            ExpiresIn = (int)(expires - now).TotalSeconds,
            PlayerId = subject,
            PlayerName = name,
            GuildId = guildId,
            AccountId = accountId,
            CharacterId = characterId,
        };
    }
}
