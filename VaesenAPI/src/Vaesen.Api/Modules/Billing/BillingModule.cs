namespace Vaesen.Api.Modules.Billing;

/// <summary>
/// Subscriptions and payment-provider webhooks. Source of truth for "does this player have an active subscription".
/// </summary>
public static class BillingModule
{
    public static IServiceCollection AddBillingModule(this IServiceCollection services, IConfiguration configuration)
    {
        return services;
    }

    public static IEndpointRouteBuilder MapBillingEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/billing").WithTags("Billing");
        return routes;
    }
}
