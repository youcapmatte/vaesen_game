using System.Security.Claims;
using System.Text;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.Extensions.Options;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Vaesen.Api.Modules.Identity;

namespace Vaesen.Api.Modules.Dev;

/// <summary>
/// Development-only login: pick a guild and a player from seed data and get a token without a password.
/// The token is validated like an Auth0 token, so every other endpoint behaves exactly as in production.
/// Only registered when the API runs in the Development environment.
/// </summary>
public static class DevModule
{
    public const string AuthenticationScheme = "DevBearer";
    public const string Issuer = "vaesen-dev";

    /// <summary>Picks DevBearer for tokens issued here and the regular (Auth0) scheme for everything else.</summary>
    private const string SelectorScheme = "DevOrAuth0";

    public static IServiceCollection AddDevModule(this IServiceCollection services, IConfiguration configuration)
    {
        services.Configure<DevOptions>(configuration.GetSection(DevOptions.SectionName));
        var dev = configuration.GetSection(DevOptions.SectionName).Get<DevOptions>() ?? new DevOptions();
        var auth0 = configuration.GetSection(Auth0Options.SectionName).Get<Auth0Options>() ?? new Auth0Options();

        services
            .AddAuthentication(SelectorScheme)
            .AddJwtBearer(AuthenticationScheme, options =>
            {
                options.MapInboundClaims = false;
                options.TokenValidationParameters = new TokenValidationParameters
                {
                    ValidIssuer = Issuer,
                    ValidAudience = auth0.Audience,
                    IssuerSigningKey = dev.GetSigningKey(),
                };
            })
            .AddPolicyScheme(SelectorScheme, SelectorScheme, options =>
            {
                options.ForwardDefaultSelector = context => IsDevToken(context)
                    ? AuthenticationScheme
                    : JwtBearerDefaults.AuthenticationScheme;
            });

        return services;
    }

    public static IEndpointRouteBuilder MapDevEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/dev").WithTags("Dev").AllowAnonymous();

        group.MapGet("/guilds", (IOptions<DevOptions> options) => options.Value.Guilds);

        group.MapPost("/login", (DevLoginRequest request, IOptions<DevOptions> options, IOptions<Auth0Options> auth0) =>
        {
            var dev = options.Value;
            var (guild, player) = dev.FindPlayer(request.PlayerId);
            if (player is null)
            {
                return Results.Problem(statusCode: StatusCodes.Status404NotFound, title: $"Unknown dev player '{request.PlayerId}'.");
            }

            var playerId = $"dev|{player.Id}";
            var expires = DateTime.UtcNow.AddHours(dev.TokenLifetimeHours);
            var token = new JsonWebTokenHandler().CreateToken(new SecurityTokenDescriptor
            {
                Issuer = Issuer,
                Audience = auth0.Value.Audience,
                Subject = new ClaimsIdentity(
                [
                    new Claim("sub", playerId),
                    new Claim("name", player.Name),
                    new Claim("guild", guild!.Id),
                ]),
                Expires = expires,
                SigningCredentials = new SigningCredentials(dev.GetSigningKey(), SecurityAlgorithms.HmacSha256),
            });

            return Results.Ok(new DevLoginResponse(token, (int)(expires - DateTime.UtcNow).TotalSeconds, playerId, player.Name, guild.Id));
        });

        return routes;
    }

    private static bool IsDevToken(HttpContext context)
    {
        var header = context.Request.Headers.Authorization.ToString();
        if (!header.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase))
        {
            return false;
        }

        var handler = new JsonWebTokenHandler();
        var raw = header["Bearer ".Length..].Trim();
        return handler.CanReadToken(raw) && handler.ReadJsonWebToken(raw).Issuer == Issuer;
    }
}

public sealed class DevOptions
{
    public const string SectionName = "Dev";

    /// <summary>HMAC key for dev tokens. Development only; at least 32 characters.</summary>
    public string SigningKey { get; set; } = "";

    public int TokenLifetimeHours { get; set; } = 12;

    public List<DevGuild> Guilds { get; set; } = [];

    public SymmetricSecurityKey GetSigningKey() => new(Encoding.UTF8.GetBytes(SigningKey));

    public (DevGuild? Guild, DevPlayer? Player) FindPlayer(string playerId)
    {
        foreach (var guild in Guilds)
        {
            var player = guild.Players.FirstOrDefault(p => string.Equals(p.Id, playerId, StringComparison.OrdinalIgnoreCase));
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
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
    public List<DevPlayer> Players { get; set; } = [];
}

public sealed class DevPlayer
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
}

public sealed record DevLoginRequest(string PlayerId);

public sealed record DevLoginResponse(string AccessToken, int ExpiresIn, string PlayerId, string PlayerName, string GuildId);
