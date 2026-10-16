namespace Vaesen.Api.Modules.Billing;

/// <summary>
/// Subscriptions and payment-provider webhooks. Source of truth for "does this player have an active subscription".
/// </summary>
public static class BillingModule
{
    public static IServiceCollection AddBillingModule(this IServiceCollection services, IConfiguration configuration)
    {
        // No payment provider yet: every player counts as subscribed.
        services.AddSingleton<ISubscriptionService, UnrestrictedSubscriptionService>();
        return services;
    }

    public static IEndpointRouteBuilder MapBillingEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/billing").WithTags("Billing");
        return routes;
    }
}

public interface ISubscriptionService
{
    Task<bool> HasActiveSubscriptionAsync(string playerId, CancellationToken cancellationToken);
}

/// <summary>Placeholder until a payment provider is integrated.</summary>
public sealed class UnrestrictedSubscriptionService : ISubscriptionService
{
    public Task<bool> HasActiveSubscriptionAsync(string playerId, CancellationToken cancellationToken) =>
        Task.FromResult(true);
}
