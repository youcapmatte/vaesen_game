using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.IdentityModel.JsonWebTokens;
using Microsoft.IdentityModel.Tokens;
using Nexus.Table.Data;
using Nexus.Table.Data.Dynamo;
using Nexus.Table.Hubs;
using Vaesen.Framework.Core.Security;
using Vaesen.Framework.Mystery.Services;

namespace Nexus.Table.Services;

public static class NexusServiceExtensions
{
    /// <summary>DataService:Provider value for development storage in JSON files instead of DynamoDB.</summary>
    public const string LocalFileProvider = "LocalFile";

    private const string DevOrAuth0Scheme = "DevOrAuth0";
    private const string DevBearerScheme = "DevBearer";

    /// <summary>
    /// Accounts, the Society, characters, dice and the table hub. Call after AddCoreServices.
    /// Controllers: Nexus.Table.Api; hub: <see cref="TableHub"/>.
    /// </summary>
    public static IServiceCollection AddNexusServices(this IServiceCollection services, IConfiguration configuration)
    {
        services.Configure<NexusOptions>(configuration.GetSection(NexusOptions.SectionName));
        services.Configure<LocalDataOptions>(configuration.GetSection(LocalDataOptions.SectionName));
        services.Configure<DevOptions>(configuration.GetSection(DevOptions.SectionName));

        if (string.Equals(configuration["DataService:Provider"], LocalFileProvider, StringComparison.OrdinalIgnoreCase))
        {
            services.AddSingleton<IAccountStore, LocalAccountStore>();
            services.AddSingleton<ISocietyStore, LocalSocietyStore>();
            services.AddSingleton<ICharacterStore, LocalCharacterStore>();
            services.AddSingleton<IRollStore, LocalRollStore>();
            services.AddSingleton<ISessionStore, LocalSessionStore>();
            services.AddSingleton<ISessionEventStore, LocalSessionEventStore>();
        }
        else
        {
            services.AddSingleton<IAccountStore, AccountRepository>();
            services.AddSingleton<ISocietyStore, SocietyRepository>();
            services.AddSingleton<ICharacterStore, CharacterRepository>();
            services.AddSingleton<IRollStore, RollRepository>();
            services.AddSingleton<ISessionStore, SessionRepository>();
            services.AddSingleton<ISessionEventStore, SessionEventRepository>();
        }

        services.AddSingleton<IDiceRoller, CryptoDiceRoller>();
        services.AddSingleton<IRulesService, RulesService>();
        services.AddSingleton<ITableNotifier, SignalRTableNotifier>();
        services.AddSingleton<TablePresence>();
        services.AddSingleton<ISocietyService, SocietyService>();
        services.AddSingleton<IAccountService, AccountService>();
        services.AddSingleton<ICharacterService, CharacterService>();
        services.AddSingleton<ISessionService, SessionService>();
        services.AddSingleton<IDiceService, DiceService>();
        services.AddSingleton<IDevLoginService, DevLoginService>();

        // The mystery engine (Vaesen.Framework.Mystery), and the host's use of it at the table.
        services.AddMysteryServices(configuration);
        // Case files and the Society's world sit with the other local data unless folders are configured.
        var localData = configuration.GetSection(LocalDataOptions.SectionName).Get<LocalDataOptions>() ?? new LocalDataOptions();
        services.PostConfigure<Vaesen.Framework.Mystery.MysteryOptions>(o =>
        {
            if (string.IsNullOrWhiteSpace(o.CasePacketPath))
            {
                o.CasePacketPath = Path.GetFullPath(Path.Combine(localData.Directory, "cases"));
            }

            if (string.IsNullOrWhiteSpace(o.WorldPath))
            {
                o.WorldPath = Path.GetFullPath(Path.Combine(localData.Directory, "worlds"));
            }
        });
        services.AddSingleton<IMysteryService, MysteryService>();

        services.Configure<LiveKitOptions>(configuration.GetSection(LiveKitOptions.SectionName));
        services.AddHttpClient(CallService.HttpClientName, client => client.Timeout = TimeSpan.FromSeconds(10));
        services.AddSingleton<ICallService, CallService>();

        // Browsers can't send headers when opening a WebSocket, so the hub takes the token from the query string.
        services.PostConfigureAll<JwtBearerOptions>(options =>
        {
            var previous = options.Events?.OnMessageReceived;
            options.Events ??= new JwtBearerEvents();
            options.Events.OnMessageReceived = async context =>
            {
                if (previous is not null)
                {
                    await previous(context).ConfigureAwait(false);
                }

                var token = context.Request.Query["access_token"].ToString();
                if (!string.IsNullOrEmpty(token) && context.HttpContext.Request.Path.StartsWithSegments(TableHub.Path))
                {
                    context.Token = token;
                }
            };
        });

        return services;
    }

    /// <summary>
    /// Development only: accept tokens from the dev login (DevController) next to Auth0 tokens.
    /// Call after AddCoreServices, and only in the Development environment.
    /// </summary>
    public static IServiceCollection AddNexusDevAuthentication(this IServiceCollection services, IConfiguration configuration)
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
                options.ForwardDefaultSelector = context => IsDevToken(context) ? DevBearerScheme : JwtBearerDefaults.AuthenticationScheme;
            });

        return services;
    }

    private static bool IsDevToken(HttpContext context)
    {
        var header = context.Request.Headers.Authorization.ToString();
        var raw = header.StartsWith("Bearer ", StringComparison.OrdinalIgnoreCase)
            ? header["Bearer ".Length..].Trim()
            : context.Request.Query["access_token"].ToString();

        var handler = new JsonWebTokenHandler();
        return raw.Length > 0 && handler.CanReadToken(raw) && handler.ReadJsonWebToken(raw).Issuer == DevLoginService.Issuer;
    }
}
