using Microsoft.Extensions.Options;
using Nexus.Table.Data;
using Nexus.Table.Models;
using Vaesen.Framework.Core.Models;

namespace Nexus.Table.Services;

public interface ISocietyService
{
    /// <summary>The Society, created from configuration on first use.</summary>
    Task<ResultBase<Society>> GetAsync(CancellationToken cancellationToken = default);

    /// <summary>The Society with its members and their active characters, as <paramref name="viewer"/> may see them.</summary>
    Task<ResultBase<SocietyView>> GetViewAsync(Account viewer, CancellationToken cancellationToken = default);

    Task<ResultBase<Society>> AddMemberAsync(string accountId, CancellationToken cancellationToken = default);

    /// <summary>Host only.</summary>
    Task<ResultBase<Society>> UpdateAsync(Account caller, UpdateSocietyRequest request, CancellationToken cancellationToken = default);

    /// <summary>Host only: puts a case on the table (and in the Society's list of mysteries), or clears it with null.</summary>
    Task<ResultBase<Society>> SetAtTableAsync(Account caller, MysteryAtTable? atTable, CancellationToken cancellationToken = default);
}

public sealed class SocietyService : ISocietyService
{
    private const int MaxTextLength = 200;

    private readonly ISocietyStore _societies;
    private readonly IAccountStore _accounts;
    private readonly ICharacterStore _characters;
    private readonly ITableNotifier _notifier;
    private readonly IOptions<NexusOptions> _options;

    public SocietyService(
        ISocietyStore societies,
        IAccountStore accounts,
        ICharacterStore characters,
        ITableNotifier notifier,
        IOptions<NexusOptions> options)
    {
        _societies = societies;
        _accounts = accounts;
        _characters = characters;
        _notifier = notifier;
        _options = options;
    }

    public async Task<ResultBase<Society>> GetAsync(CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Society>();
        var options = _options.Value;
        var society = await _societies.GetAsync(options.SocietyId, cancellationToken).ConfigureAwait(false);
        if (society is not null)
        {
            return ret.Ok(society);
        }

        var created = await _societies.CreateAsync(new Society
        {
            Id = options.SocietyId,
            Name = options.SocietyName,
            Headquarters = options.Headquarters,
        }, cancellationToken).ConfigureAwait(false);
        if (created.Success)
        {
            return created;
        }

        // Created by a request running at the same time.
        society = await _societies.GetAsync(options.SocietyId, cancellationToken).ConfigureAwait(false);
        return society is null ? ret.Fail(created.ErrorMessage, created.ErrorCode) : ret.Ok(society);
    }

    public async Task<ResultBase<SocietyView>> GetViewAsync(Account viewer, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<SocietyView>();
        var society = await GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return ret.Fail(society.ErrorMessage, society.ErrorCode);
        }

        var accounts = await _accounts.GetManyAsync(society.Result!.MemberAccountIds, cancellationToken).ConfigureAwait(false);
        var characters = (await _characters.ListBySocietyAsync(society.Result.Id, cancellationToken).ConfigureAwait(false))
            .ToDictionary(c => c.Id, StringComparer.Ordinal);

        var members = society.Result.MemberAccountIds
            .Select(id => accounts.FirstOrDefault(a => a.Id == id))
            .OfType<Account>()
            .Select(a => new SocietyMember
            {
                AccountId = a.Id,
                DisplayName = a.DisplayName,
                AvatarUrl = a.AvatarUrl,
                Role = a.Role,
                Character = a.ActiveCharacterId is not null && characters.TryGetValue(a.ActiveCharacterId, out var c) ? c.VisibleTo(viewer) : null,
            })
            .ToList();

        return ret.Ok(new SocietyView { Society = society.Result, Members = members });
    }

    public async Task<ResultBase<Society>> AddMemberAsync(string accountId, CancellationToken cancellationToken = default)
    {
        var society = await GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success || society.Result!.MemberAccountIds.Contains(accountId, StringComparer.Ordinal))
        {
            return society;
        }

        society.Result.MemberAccountIds.Add(accountId);
        var updated = await _societies.UpdateAsync(society.Result, cancellationToken).ConfigureAwait(false);
        if (updated.ErrorCode == ErrorCodes.Conflict)
        {
            // Someone else joined at the same moment: read again and retry once.
            return await AddMemberAsync(accountId, cancellationToken).ConfigureAwait(false);
        }

        if (updated.Success)
        {
            await _notifier.SocietyChangedAsync(updated.Result!.Id, cancellationToken).ConfigureAwait(false);
        }

        return updated;
    }

    public async Task<ResultBase<Society>> UpdateAsync(Account caller, UpdateSocietyRequest request, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Society>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can change the Society.", ErrorCodes.Forbidden);
        }

        var society = await GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return society;
        }

        var name = request.Name?.Trim();
        var headquarters = request.Headquarters?.Trim();
        if (name is { Length: 0 or > MaxTextLength } || headquarters is { Length: > MaxTextLength })
        {
            return ret.Fail($"Name is required, and texts can be at most {MaxTextLength} characters.", ErrorCodes.Invalid);
        }

        society.Result!.Name = name ?? society.Result.Name;
        society.Result.Headquarters = headquarters ?? society.Result.Headquarters;
        society.Result.UpdatedBy = caller.Id;
        var updated = await _societies.UpdateAsync(society.Result, cancellationToken).ConfigureAwait(false);
        if (updated.Success)
        {
            await _notifier.SocietyChangedAsync(updated.Result!.Id, cancellationToken).ConfigureAwait(false);
        }

        return updated;
    }

    public async Task<ResultBase<Society>> SetAtTableAsync(Account caller, MysteryAtTable? atTable, CancellationToken cancellationToken = default)
    {
        var ret = new ResultBase<Society>();
        if (caller.Role != AccountRole.Host)
        {
            return ret.Fail("Only the host can choose the mystery at the table.", ErrorCodes.Forbidden);
        }

        var society = await GetAsync(cancellationToken).ConfigureAwait(false);
        if (!society.Success)
        {
            return society;
        }

        society.Result!.AtTable = atTable;
        if (atTable is not null && !society.Result.MysteryIds.Contains(atTable.MysteryId, StringComparer.Ordinal))
        {
            society.Result.MysteryIds.Add(atTable.MysteryId);
        }

        society.Result.UpdatedBy = caller.Id;
        var updated = await _societies.UpdateAsync(society.Result, cancellationToken).ConfigureAwait(false);
        if (updated.ErrorCode == ErrorCodes.Conflict)
        {
            // Changed at the same moment (someone joined): read again and retry.
            return await SetAtTableAsync(caller, atTable, cancellationToken).ConfigureAwait(false);
        }

        if (updated.Success)
        {
            await _notifier.SocietyChangedAsync(updated.Result!.Id, cancellationToken).ConfigureAwait(false);
        }

        return updated;
    }
}

/// <summary>Tells everyone at the table that something changed (SignalR, see Hubs/TableHub).</summary>
public interface ITableNotifier
{
    Task SocietyChangedAsync(string societyId, CancellationToken cancellationToken = default);

    Task CharacterChangedAsync(Character character, CancellationToken cancellationToken = default);

    Task RollMadeAsync(Roll roll, CancellationToken cancellationToken = default);
}
