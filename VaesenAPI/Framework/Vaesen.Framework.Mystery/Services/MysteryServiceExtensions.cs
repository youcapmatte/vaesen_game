using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Vaesen.Framework.Mystery.Ai;
using Vaesen.Framework.Mystery.Data;
using Vaesen.Framework.Mystery.Validation;

namespace Vaesen.Framework.Mystery.Services;

public static class MysteryServiceExtensions
{
    /// <summary>
    /// The mystery engine: content, assignment, the generation pipeline, the validator and the case store. Reads the
    /// "Mystery" configuration section (<see cref="MysteryOptions"/>). Used by Nexus Table now and the game later.
    /// </summary>
    public static IServiceCollection AddMysteryServices(this IServiceCollection services, IConfiguration configuration)
    {
        services.Configure<MysteryOptions>(configuration.GetSection(MysteryOptions.SectionName));
        services.TryAddSingleton<IContentStore, FileContentStore>();
        services.TryAddSingleton<ICasePacketStore, FileCasePacketStore>();
        services.TryAddSingleton<IMysteryModel, AnthropicMysteryModel>();
        services.TryAddSingleton<ICoherenceValidator, CaseValidator>();
        services.TryAddSingleton<IMysteryGenerator, MysteryGenerator>();
        services.TryAddSingleton<ContentGenerator>();
        return services;
    }
}
