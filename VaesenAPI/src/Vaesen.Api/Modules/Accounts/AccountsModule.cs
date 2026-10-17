using System.Text.RegularExpressions;
using Microsoft.Extensions.DependencyInjection.Extensions;
using Vaesen.Api.Common;
using Vaesen.Api.Modules.Identity;

namespace Vaesen.Api.Modules.Accounts;

/// <summary>
/// Player accounts: username, Auth0 link and subscription.
///
/// Access: open in Development (the game's fake login looks accounts up by username); everywhere else these are
/// back-office endpoints that need the admin scope. Players will reach their own account through Auth0 tokens.
/// </summary>
public static partial class AccountsModule
{
    public static IServiceCollection AddAccountsModule(this IServiceCollection services, IConfiguration configuration)
    {
        services.TryAddSingleton(TimeProvider.System);
        services.AddSingleton<IAccountRepository, DynamoDbAccountRepository>();
        return services;
    }

    public static IEndpointRouteBuilder MapAccountsEndpoints(this IEndpointRouteBuilder routes)
    {
        var group = routes.MapGroup("/accounts").WithTags("Accounts");

        var environment = routes.ServiceProvider.GetRequiredService<IHostEnvironment>();
        if (environment.IsDevelopment())
        {
            group.AllowAnonymous();
        }
        else
        {
            group.RequireAuthorization(Policies.Admin);
        }

        group.MapPost("/", CreateAccount);
        group.MapGet("/{accountId}", GetAccount).WithName("GetAccount");
        group.MapGet("/by-username/{username}", GetAccountByUsername);
        group.MapGet("/by-auth0/{auth0UserId}", GetAccountByAuth0UserId);

        return routes;
    }

    private static async Task<IResult> CreateAccount(
        CreateAccountRequest request,
        IAccountRepository accounts,
        TimeProvider time,
        CancellationToken cancellationToken)
    {
        var username = request.Username?.Trim() ?? "";
        var email = string.IsNullOrWhiteSpace(request.Email) ? null : request.Email.Trim();
        var auth0UserId = string.IsNullOrWhiteSpace(request.Auth0UserId) ? null : request.Auth0UserId.Trim();

        var errors = new Dictionary<string, string[]>();
        if (!UsernamePattern().IsMatch(username))
        {
            errors["username"] = ["3-24 characters: letters, digits, '_' or '-'."];
        }
        if (email is not null && (email.Length > 254 || !email.Contains('@')))
        {
            errors["email"] = ["Not a valid email address."];
        }
        if (auth0UserId is not null && (auth0UserId.Length > 256 || !auth0UserId.Contains('|')))
        {
            errors["auth0UserId"] = ["Expected Auth0's user_id, e.g. 'auth0|65f1c2...'."];
        }
        if (errors.Count > 0)
        {
            return Results.ValidationProblem(errors);
        }

        var account = new Account(
            EntityIds.New(EntityIds.Account),
            username,
            email,
            auth0UserId,
            AccountSubscription.None,
            time.GetUtcNow(),
            Version: 1);

        return await accounts.TryCreateAsync(account, cancellationToken) switch
        {
            CreateAccountResult.Created => Results.CreatedAtRoute("GetAccount", new { accountId = account.Id }, ToResponse(account, time)),
            CreateAccountResult.UsernameTaken => Results.Problem(statusCode: StatusCodes.Status409Conflict, title: "Username is taken."),
            _ => Results.Problem(statusCode: StatusCodes.Status409Conflict, title: "This Auth0 user already has an account."),
        };
    }

    private static async Task<IResult> GetAccount(string accountId, IAccountRepository accounts, TimeProvider time, CancellationToken cancellationToken)
    {
        if (!EntityIds.IsValid(EntityIds.Account, accountId))
        {
            return Results.Problem(statusCode: StatusCodes.Status400BadRequest, title: "Not an account id (expected 'account-{guid}').");
        }
        return ToResult(await accounts.GetAsync(accountId, cancellationToken), time);
    }

    private static async Task<IResult> GetAccountByUsername(string username, IAccountRepository accounts, TimeProvider time, CancellationToken cancellationToken) =>
        ToResult(await accounts.GetByUsernameAsync(username, cancellationToken), time);

    private static async Task<IResult> GetAccountByAuth0UserId(string auth0UserId, IAccountRepository accounts, TimeProvider time, CancellationToken cancellationToken) =>
        ToResult(await accounts.GetByAuth0UserIdAsync(auth0UserId, cancellationToken), time);

    private static IResult ToResult(Account? account, TimeProvider time) =>
        account is null
            ? Results.Problem(statusCode: StatusCodes.Status404NotFound, title: "Account not found.")
            : Results.Ok(ToResponse(account, time));

    private static AccountResponse ToResponse(Account account, TimeProvider time) => new(
        account.Id,
        account.Username,
        account.Email,
        account.Auth0UserId,
        new SubscriptionResponse(
            account.Subscription.Status.ToString(),
            account.Subscription.Plan,
            account.Subscription.CurrentPeriodEnd,
            account.Subscription.IsActive(time.GetUtcNow())),
        account.CreatedAt);

    [GeneratedRegex("^[A-Za-z0-9_-]{3,24}$")]
    private static partial Regex UsernamePattern();
}

public sealed record CreateAccountRequest(string? Username, string? Email, string? Auth0UserId);

public sealed record AccountResponse(
    string Id,
    string Username,
    string? Email,
    string? Auth0UserId,
    SubscriptionResponse Subscription,
    DateTimeOffset CreatedAt);

/// <param name="Status">None, Trialing, Active, PastDue or Canceled.</param>
/// <param name="IsActive">Whether the account may play right now.</param>
public sealed record SubscriptionResponse(string Status, string? Plan, DateTimeOffset? CurrentPeriodEnd, bool IsActive);
