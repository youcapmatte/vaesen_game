using System.Net;
using System.Net.Http.Json;
using Vaesen.Framework.Common.Models;
using Vaesen.Framework.Common.Repositorys;
using Vaesen.Framework.Core.Models;

namespace Vaesen.Framework.Tests;

/// <summary>Account endpoints (open in Development, which is what the test host runs as).</summary>
public class AccountsTests
{
    private const string Accounts = "/api/accounts";

    private static object NewAccount(string username, string? email = null, string? auth0UserId = null) =>
        new { username, email, auth0UserId };

    [Fact]
    public async Task Create_ReturnsAccountWithTypedIdAndNoSubscription()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsJsonAsync(Accounts, NewAccount("Ingrid", "ingrid@example.com", "auth0|abc123"));

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
        var result = await response.ReadResultAsync<Account>();
        Assert.True(result.Success);
        var account = result.Result!;
        Assert.True(ObjectCore.IsValidId<Account>(account.Id));
        Assert.Equal("Ingrid", account.Username);
        Assert.Equal("ingrid@example.com", account.Email);
        Assert.Equal("auth0|abc123", account.Auth0UserId);
        Assert.Equal(SubscriptionStatus.None, account.Subscription.Status);
        Assert.Equal(1, account.Version);
        Assert.EndsWith($"{Accounts}/{account.Id}", response.Headers.Location!.ToString(), StringComparison.Ordinal);
    }

    [Fact]
    public async Task Create_ResponseUsesCamelCaseEnvelope()
    {
        await using var factory = new VaesenApiFactory();

        var json = await (await factory.CreateClient().PostAsJsonAsync(Accounts, NewAccount("Astrid"))).Content.ReadAsStringAsync();

        Assert.Contains("\"success\":true", json, StringComparison.Ordinal);
        Assert.Contains("\"result\":{", json, StringComparison.Ordinal);
        Assert.Contains("\"status\":\"None\"", json, StringComparison.Ordinal);
    }

    [Fact]
    public async Task Create_WithTakenUsername_DifferentCase_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        await client.PostAsJsonAsync(Accounts, NewAccount("Johan"));

        var response = await client.PostAsJsonAsync(Accounts, NewAccount("JOHAN"));

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(AccountRepository.UsernameTaken, (await response.ReadResultAsync<Account>()).ErrorCode);
    }

    [Fact]
    public async Task Create_WithTakenAuth0UserId_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        await client.PostAsJsonAsync(Accounts, NewAccount("Astrid", auth0UserId: "auth0|same"));

        var response = await client.PostAsJsonAsync(Accounts, NewAccount("Erik", auth0UserId: "auth0|same"));

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
        Assert.Equal(AccountRepository.Auth0UserTaken, (await response.ReadResultAsync<Account>()).ErrorCode);
    }

    [Theory]
    [InlineData("ab", null, null)]
    [InlineData("has space", null, null)]
    [InlineData("Sigrid", "not-an-email", null)]
    [InlineData("Sigrid", null, "no-provider-separator")]
    public async Task Create_WithInvalidInput_IsBadRequest(string username, string? email, string? auth0UserId)
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsJsonAsync(Accounts, NewAccount(username, email, auth0UserId));

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
        Assert.Equal(ErrorCodes.Invalid, (await response.ReadResultAsync<Account>()).ErrorCode);
    }

    [Fact]
    public async Task Get_ByIdUsernameAndAuth0UserId_ReturnTheSameAccount()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        var created = (await (await client.PostAsJsonAsync(Accounts, NewAccount("Ingrid", auth0UserId: "auth0|xyz"))).ReadResultAsync<Account>()).Result!;

        var byId = (await (await client.GetAsync($"{Accounts}/{created.Id}")).ReadResultAsync<Account>()).Result!;
        var byUsername = (await (await client.GetAsync($"{Accounts}/by-username/ingrid")).ReadResultAsync<Account>()).Result!;
        var byAuth0 = (await (await client.GetAsync($"{Accounts}/by-auth0/{Uri.EscapeDataString("auth0|xyz")}")).ReadResultAsync<Account>()).Result!;

        Assert.Equal(created.Id, byId.Id);
        Assert.Equal(created.Id, byUsername.Id);
        Assert.Equal(created.Id, byAuth0.Id);
    }

    [Fact]
    public async Task Get_Unknown_IsNotFound()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();

        Assert.Equal(HttpStatusCode.NotFound, (await client.GetAsync($"{Accounts}/{new Account().Id}")).StatusCode);
        Assert.Equal(HttpStatusCode.NotFound, (await client.GetAsync($"{Accounts}/by-username/nobody")).StatusCode);
    }

    [Fact]
    public async Task Get_WithMalformedId_IsBadRequest()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().GetAsync($"{Accounts}/character-{Guid.NewGuid()}");

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }
}
