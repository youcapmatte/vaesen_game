using Newtonsoft.Json;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

public sealed class ApprovalRequest
{
    [JsonProperty("approved")]
    public bool Approved { get; set; }
}

/// <summary>
/// The host's Admin page: who has signed in, and letting them in. A new sign-in (Discord through Auth0) waits on the
/// Society's pending list until the host approves it; only then is it a member, with the table, the call and its sheet.
/// </summary>
public interface IAdminService
{
    /// <summary>Host only: everyone who has signed in, those waiting first.</summary>
    Task<ResultBase<IReadOnlyList<Account>>> ListAccountsAsync(Account caller, CancellationToken cancellationToken = default);

    /// <summary>Host only: lets an account in, or turns it away (off the Society and out of the call).</summary>
    Task<ResultBase<Account>> SetApprovalAsync(Account caller, string accountId, bool approved, CancellationToken cancellationToken = default);
}

public sealed class AdminService(IAccountStore accounts, ISocietyService society, ICallService call) : IAdminService
{
    public async Task<ResultBase<IReadOnlyList<Account>>> ListAccountsAsync(Account caller, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<IReadOnlyList<Account>>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can manage accounts.", ErrorCodes.Forbidden);
        }

        var current = await society.GetAsync(cancellationToken).ConfigureAwait(false);
        if (!current.Success)
        {
            return ret.Fail(current.ErrorMessage, current.ErrorCode);
        }

        var ids = current.Result!.PendingAccountIds.Concat(current.Result.MemberAccountIds).Distinct(StringComparer.Ordinal).ToList();
        var list = await accounts.GetManyAsync(ids, cancellationToken).ConfigureAwait(false);
        return ret.Ok(list
            .OrderBy(a => a.IsApproved)
            .ThenBy(a => a.DisplayName, StringComparer.OrdinalIgnoreCase)
            .ToList());
    }

    public async Task<ResultBase<Account>> SetApprovalAsync(Account caller, string accountId, bool approved, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Account>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can manage accounts.", ErrorCodes.Forbidden);
        }

        var account = Account.IsValidId<Account>(accountId) ? await accounts.GetAsync(accountId, cancellationToken).ConfigureAwait(false) : null;
        if (account is null)
        {
            return ret.Fail("Account not found.", ErrorCodes.NotFound);
        }

        if (account.Role == AccountRole.Host && !approved)
        {
            return ret.Fail("The host can't be turned away.", ErrorCodes.Invalid);
        }

        if (account.IsApproved != approved)
        {
            account.IsApproved = approved;
            account.UpdatedBy = caller.Id;
            var updated = await accounts.UpdateAsync(account, cancellationToken).ConfigureAwait(false);
            if (!updated.Success)
            {
                return updated;
            }

            account = updated.Result!;
        }

        var membership = await society.SetMembershipAsync(account.Id, approved, cancellationToken).ConfigureAwait(false);
        if (!membership.Success)
        {
            return ret.Fail(membership.ErrorMessage, membership.ErrorCode);
        }

        if (!approved)
        {
            await call.RemoveAsync(account.Id, cancellationToken).ConfigureAwait(false);
        }

        return ret.Ok(account);
    }
}
