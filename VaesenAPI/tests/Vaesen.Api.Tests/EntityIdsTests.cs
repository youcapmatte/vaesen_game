using Vaesen.Api.Common;

namespace Vaesen.Api.Tests;

public class EntityIdsTests
{
    [Fact]
    public void New_IsTypeDashLowercaseGuid()
    {
        var id = EntityIds.New(EntityIds.Account);

        Assert.Matches("^account-[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", id);
        Assert.True(EntityIds.IsValid(EntityIds.Account, id));
    }

    [Theory]
    [InlineData(null)]
    [InlineData("")]
    [InlineData("account-")]
    [InlineData("account-not-a-guid")]
    [InlineData("character-3f2b8c1e-1d2c-4b5a-9e8f-0123456789ab")]
    [InlineData("account-3F2B8C1E-1D2C-4B5A-9E8F-0123456789AB")]
    [InlineData("account-3f2b8c1e1d2c4b5a9e8f0123456789ab")]
    public void IsValid_RejectsOtherFormats(string? id)
    {
        Assert.False(EntityIds.IsValid(EntityIds.Account, id));
    }
}
