using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Security;

namespace Vaesen.Framework.Common.Services;

public static class CommonServiceExtensions
{
    /// <summary>Accounts, guilds, characters and billing. Controllers: AccountController, GuildController, CharacterController, IdentityController, DevController.</summary>
    public static IServiceCollection AddCommonServices(this IServiceCollection services, IConfiguration configuration)
    {
        services.AddSingleton<AccountRepository>();
        services.AddSingleton<IAccountStore>(sp => sp.GetRequiredService<AccountRepository>());
        services.AddSingleton<CharacterRepository>();
        services.AddSingleton<ICharacterStore>(sp => sp.GetRequiredService<CharacterRepository>());
        services.AddSingleton<GuildRepository>();
        services.AddSingleton<IGuildStore>(sp => sp.GetRequiredService<GuildRepository>());

        services.Configure<CharacterOptions>(configuration.GetSection(CharacterOptions.SectionName));
        services.Configure<GuildOptions>(configuration.GetSection(GuildOptions.SectionName));

        services.AddSingleton<IAccountService, AccountService>();
        services.AddSingleton<IGuildService, GuildService>();
        services.AddSingleton<ICharacterService, CharacterService>();

        // No payment provider yet: every player counts as subscribed.
        services.AddSingleton<ISubscriptionService, UnrestrictedSubscriptionService>();

        services.Configure<DevOptions>(configuration.GetSection(DevOptions.SectionName));
        services.AddSingleton<IDevLoginService, DevLoginService>();

        return services;
    }

    /// <summary>Picks DevBearer for tokens issued by the dev login and the regular (Auth0) scheme for everything else.</summary>
    private const string DevOrAuth0Scheme = "DevOrAuth0";

    private const string DevBearerScheme = "DevBearer";

    /// <summary>
    /// Development only: accept tokens from the dev login (DevController) next to Auth0 tokens.
    /// Call after AddCoreServices, and only in the Development environment.
    /// </summary>
    public static IServiceCollection AddDevServices(this IServiceCollection services, IConfiguration configuration)
    {
        var dev = configuration.GetSection(DevOptions.SectionName).Get<DevOptions>() ?? new DevOptions();
        var auth0 = configuration.GetSection(Auth0Options.SectionName).Get<Auth0Options>() ?? new Auth0Options();

        services
            .AddAuthentication(DevOrAuth0Scheme)
            .AddJwtBearer(DevBearerScheme, options =>
            {
                options.MapInboundClaims = false;
                options.TokenValidationParameters = new TokenValidationParameters
                {
                    ValidIssuer = DevLoginService.Issuer,
                    ValidAudience = auth0.Audience,
                    IssuerSigningKey = dev.GetSigningKey(),
                };
            })
            .AddPolicyScheme(DevOrAuth0Scheme, DevOrAuth0Scheme, options =>
            {
                options.ForwardDefaultSelector = context => IsDevToken(context)
                    ? DevBearerScheme
                    : JwtBearerDefaults.AuthenticationScheme;
            });

        return services;
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
        return handler.CanReadToken(raw) && string.Equals(handler.ReadJsonWebToken(raw).Issuer, DevLoginService.Issuer, StringComparison.Ordinal);
    }
}
