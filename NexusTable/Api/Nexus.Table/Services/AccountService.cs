using System.Security.Claims;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Options;
using Newtonsoft.Json;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;
using Vaesen.Framework.Core.Security;

namespace Nexus.Table.Services;

/// <summary>Who is calling: the token's subject and, when the token carries them, name and picture.</summary>
public sealed record Caller(string Subject, string? Name, string? Picture)
{
    public static Caller From(ClaimsPrincipal user) =>
        new(user.GetSubject(), user.FindFirstValue("name") ?? user.FindFirstValue("nickname"), user.FindFirstValue("picture"));
}

/// <summary>Profile details the web app has from the sign-in (Discord name and avatar), which access tokens lack.</summary>
public sealed class UpdateProfileRequest
{
    [JsonProperty("displayName")]
    public string? DisplayName { get; set; }

    [JsonProperty("avatarUrl")]
    public string? AvatarUrl { get; set; }
}

public interface IAccountService
{
    /// <summary>The caller's account, created on first sign-in, and a member of the Society.</summary>
    Task<ResultBase<Account>> GetOrCreateAsync(Caller caller, CancellationToken cancellationToken = default);

    Task<ResultBase<Account>> UpdateProfileAsync(Caller caller, UpdateProfileRequest request, CancellationToken cancellationToken = default);
}

public sealed class AccountService : IAccountService
{
    private const int MaxNameLength = 64;
    private const int MaxUrlLength = 1024;

    private readonly IAccountStore _accounts;
    private readonly ISocietyService _society;
    private readonly IOptions<NexusOptions> _options;
    private readonly IHostEnvironment _environment;

    public AccountService(IAccountStore accounts, ISocietyService society, IOptions<NexusOptions> options, IHostEnvironment environment)
    {
        _accounts = accounts;
        _society = society;
        _options = options;
        _environment = environment;
    }

    /// <summary>Let in without waiting for the host: the host, and in Development the dev login's seed players.</summary>
    private bool ApprovedFromTheStart(string subject) =>
        _options.Value.HostSubjects.Contains(subject, StringComparer.Ordinal)
        || (_environment.IsDevelopment() && _options.Value.AutoApproveDevPlayers && subject.StartsWith("dev|", StringComparison.Ordinal));

    public async Task<ResultBase<Account>> GetOrCreateAsync(Caller caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Account>();
        var account = await _accounts.GetBySubjectAsync(caller.Subject, cancellationToken).ConfigureAwait(false);
        if (account is null)
        {
            var created = await _accounts.CreateAsync(new Account
            {
                Subject = caller.Subject,
                DisplayName = Clean(caller.Name, MaxNameLength) ?? "Unnamed traveller",
                AvatarUrl = Clean(caller.Picture, MaxUrlLength),
                // A new sign-in waits for the host.
                IsApproved = ApprovedFromTheStart(caller.Subject),
                CreatedBy = caller.Subject,
            }, cancellationToken).ConfigureAwait(false);

            if (created.ErrorCode == NexusErrorCodes.SubjectTaken)
            {
                // Two first requests at once: the other one created it.
                account = await _accounts.GetBySubjectAsync(caller.Subject, cancellationToken).ConfigureAwait(false);
            }
            else if (!created.Success)
            {
                return ret.Fail(created.ErrorMessage, created.ErrorCode);
            }
            else
            {
                account = created.Result;
            }
        }

        if (account is null)
        {
            return ret.Fail("Could not create the account.", ErrorCodes.Conflict);
        }

        // The host list lives in configuration, so the role follows it rather than being stored once.
        var role = _options.Value.HostSubjects.Contains(caller.Subject, StringComparer.Ordinal) ? AccountRole.Host : AccountRole.Player;
        if (account.Role != role || (role == AccountRole.Host && !account.IsApproved))
        {
            account.Role = role;
            // The host is always let in.
            account.IsApproved |= role == AccountRole.Host;
            var updated = await _accounts.UpdateAsync(account, cancellationToken).ConfigureAwait(false);
            if (!updated.Success)
            {
                return ret.Fail(updated.ErrorMessage, updated.ErrorCode);
            }
        }

        var joined = await _society.SetMembershipAsync(account.Id, account.IsApproved, cancellationToken).ConfigureAwait(false);
        return joined.Success ? ret.Ok(account) : ret.Fail(joined.ErrorMessage, joined.ErrorCode);
    }

    public async Task<ResultBase<Account>> UpdateProfileAsync(Caller caller, UpdateProfileRequest request, CancellationToken cancellationToken = default)
    {
        var current = await GetOrCreateAsync(caller, cancellationToken).ConfigureAwait(false);
        if (!current.Success)
        {
            return current;
        }

        var account = current.Result!;
        var name = Clean(request.DisplayName, MaxNameLength);
        var avatar = Clean(request.AvatarUrl, MaxUrlLength);
        if ((name is null || name == account.DisplayName) && (avatar is null || avatar == account.AvatarUrl))
        {
            return current;
        }

        account.DisplayName = name ?? account.DisplayName;
        account.AvatarUrl = avatar ?? account.AvatarUrl;
        account.UpdatedBy = account.Id;
        return await _accounts.UpdateAsync(account, cancellationToken).ConfigureAwait(false);
    }

    private static string? Clean(string? value, int maxLength)
    {
        var trimmed = value?.Trim();
        if (string.IsNullOrEmpty(trimmed))
        {
            return null;
        }

        return trimmed.Length > maxLength ? trimmed[..maxLength] : trimmed;
    }
}
