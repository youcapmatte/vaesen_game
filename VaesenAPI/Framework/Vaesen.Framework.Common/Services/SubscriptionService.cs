namespace Vaesen.Framework.Common.Services;

/// <summary>Source of truth for "may this player enter the game". Backed by the payment provider later.</summary>
public interface ISubscriptionService
{
    /// <param name="subject">The caller's token subject ("sub").</param>
    Task<bool> HasActiveSubscriptionAsync(string subject, CancellationToken cancellationToken = default);
}

/// <summary>Placeholder until a payment provider is integrated: every player counts as subscribed.</summary>
public sealed class UnrestrictedSubscriptionService : ISubscriptionService
{
    public Task<bool> HasActiveSubscriptionAsync(string subject, CancellationToken cancellationToken = default) =>
        Task.FromResult(true);
}
