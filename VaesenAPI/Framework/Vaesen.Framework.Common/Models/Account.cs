using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Common.Models;

/// <summary>
/// A player's account: the login identity that owns characters and a subscription.
/// Id: "account-{guid}". Stored by <see cref="Repositorys.AccountRepository"/>.
/// </summary>
public sealed class Account : BaseCore
{
    /// <summary>Unique, case-insensitive. Shown in game and used to log in until Auth0 is in place.</summary>
    [JsonProperty("username")]
    public string Username { get; set; } = "";

    [JsonProperty("email")]
    public string? Email { get; set; }

    /// <summary>
    /// Auth0's user_id for this person (the "sub" claim in their tokens), e.g. "auth0|65f1c2..." or
    /// "google-oauth2|1234...". Null until the account is linked to Auth0. At most one account per Auth0 user.
    /// </summary>
    [JsonProperty("auth0UserId")]
    public string? Auth0UserId { get; set; }

    [JsonProperty("subscription")]
    public AccountSubscription Subscription { get; set; } = new();
}

/// <summary>
/// The account's subscription as last reported by the payment provider. The provider is the source of truth;
/// this is the API's copy, used to decide whether the player may enter the game.
/// </summary>
public sealed class AccountSubscription
{
    [JsonProperty("status")]
    [JsonConverter(typeof(StringEnumConverter))]
    public SubscriptionStatus Status { get; set; } = SubscriptionStatus.None;

    /// <summary>Plan identifier, e.g. "monthly". Null when there's no subscription.</summary>
    [JsonProperty("plan")]
    public string? Plan { get; set; }

    /// <summary>When the paid period ends (or ended).</summary>
    [JsonProperty("currentPeriodEnd")]
    public DateTime? CurrentPeriodEnd { get; set; }

    /// <summary>True if the account may play at <paramref name="now"/>.</summary>
    public bool IsActive(DateTime now) =>
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
