using System.Net;

namespace Vaesen.Api.Tests;

public class HealthTests
{
    [Fact]
    public async Task Health_ReturnsOk_WithoutCheckingDependencies()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().GetAsync("/health");

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        Assert.Equal("Healthy", await response.Content.ReadAsStringAsync());
    }
}
