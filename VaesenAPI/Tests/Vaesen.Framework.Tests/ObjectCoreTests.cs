using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests;

public class ObjectCoreTests
{
    [Fact]
    public void NewModel_GetsIdFromItsClassName()
    {
        var account = new Account();
        var character = new Character();

        Assert.Matches("^account-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", account.Id);
        Assert.StartsWith("character-", character.Id, StringComparison.Ordinal);
        Assert.True(ObjectCore.IsValidId<Account>(account.Id));
    }

    [Fact]
    public void Id_IsStableOnceRead()
    {
        var account = new Account();

        Assert.Equal(account.Id, account.Id);
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("account-")]
    [InlineData("account-not-a-guid")]
    [InlineData("character-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab")]
    [InlineData("account-3F2B8C1E-1D2C-4B5A-9E8F-0123456789AB")]
    [InlineData("account-3f2b8c1e1d2c4b5a9e8f0123456789ab")]
    public void IsValidId_RejectsOtherFormats(string? id)
    {
        Assert.False(ObjectCore.IsValidId<Account>(id));
    }
}
