namespace Vaesen.Api.Modules.Accounts;

/// <summary>A player's account: the login identity that owns characters and a subscription.</summary>
/// <param name="Id">"account-{guid}" (see EntityIds).</param>
/// <param name="Username">Unique, case-insensitive; shown in game and used to log in until Auth0 is in place.</param>
/// <param name="Auth0UserId">
/// Auth0's user_id for this person (the "sub" claim in their tokens), e.g. "auth0|65f1c2..." or "google-oauth2|1234...".
/// Null until the account is linked to Auth0. At most one account per Auth0 user.
/// </param>
/// <param name="Version">Incremented on every update; used to detect concurrent writes.</param>
public sealed record Account(
    string Id,
    string Username,
    string? Email,
    string? Auth0UserId,
    AccountSubscription Subscription,
    DateTimeOffset CreatedAt,
    int Version);

/// <summary>
/// The account's subscription as last reported by the payment provider. The provider is the source of truth;
/// this is the API's copy, used to decide whether the player may enter the game.
/// </summary>
/// <param name="Plan">Plan identifier, e.g. "monthly". Null when there's no subscription.</param>
/// <param name="CurrentPeriodEnd">When the paid period ends (or ended).</param>
public sealed record AccountSubscription(SubscriptionStatus Status, string? Plan, DateTimeOffset? CurrentPeriodEnd)
{
    public static AccountSubscription None { get; } = new(SubscriptionStatus.None, null, null);

    /// <summary>True if the account may play right now.</summary>
    public bool IsActive(DateTimeOffset now) =>
        Status is SubscriptionStatus.Active or SubscriptionStatus.Trialing
        && (CurrentPeriodEnd is null || CurrentPeriodEnd > now);
}

public enum SubscriptionStatus
{
    None,
    Trialing,
    Active,
    PastDue,
    Canceled,
}
