using System.Net;
using System.Net.Http.Json;
using Vaesen.Api.Common;
using Vaesen.Api.Modules.Accounts;

namespace Vaesen.Api.Tests;

/// <summary>Accounts endpoints (open in Development, which is what the test host runs as).</summary>
public class AccountsTests
{
    private const string Accounts = "/api/v1/accounts";

    [Fact]
    public async Task Create_ReturnsAccountWithTypedIdAndNoSubscription()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsJsonAsync(Accounts,
            new CreateAccountRequest("Ingrid", "ingrid@example.com", "auth0|abc123"));

        Assert.Equal(HttpStatusCode.Created, response.StatusCode);
        var account = await response.Content.ReadFromJsonAsync<AccountResponse>();
        Assert.True(EntityIds.IsValid(EntityIds.Account, account!.Id));
        Assert.Equal("Ingrid", account.Username);
        Assert.Equal("ingrid@example.com", account.Email);
        Assert.Equal("auth0|abc123", account.Auth0UserId);
        Assert.Equal("None", account.Subscription.Status);
        Assert.False(account.Subscription.IsActive);
        Assert.EndsWith($"{Accounts}/{account.Id}", response.Headers.Location!.ToString());
    }

    [Fact]
    public async Task Create_WithTakenUsername_DifferentCase_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        await client.PostAsJsonAsync(Accounts, new CreateAccountRequest("Johan", null, null));

        var response = await client.PostAsJsonAsync(Accounts, new CreateAccountRequest("JOHAN", null, null));

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
    }

    [Fact]
    public async Task Create_WithTakenAuth0UserId_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        await client.PostAsJsonAsync(Accounts, new CreateAccountRequest("Astrid", null, "auth0|same"));

        var response = await client.PostAsJsonAsync(Accounts, new CreateAccountRequest("Erik", null, "auth0|same"));

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
    }

    [Theory]
    [InlineData("ab", null, null)]
    [InlineData("has space", null, null)]
    [InlineData("Sigrid", "not-an-email", null)]
    [InlineData("Sigrid", null, "no-provider-separator")]
    public async Task Create_WithInvalidInput_IsBadRequest(string username, string? email, string? auth0UserId)
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsJsonAsync(Accounts, new CreateAccountRequest(username, email, auth0UserId));

        Assert.Equal(HttpStatusCode.BadRequest, response.StatusCode);
    }

    [Fact]
    public async Task Get_ByIdUsernameAndAuth0UserId_ReturnTheSameAccount()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();
        var created = await (await client.PostAsJsonAsync(Accounts, new CreateAccountRequest("Ingrid", null, "auth0|xyz")))
            .Content.ReadFromJsonAsync<AccountResponse>();

        var byId = await client.GetFromJsonAsync<AccountResponse>($"{Accounts}/{created!.Id}");
        var byUsername = await client.GetFromJsonAsync<AccountResponse>($"{Accounts}/by-username/ingrid");
        var byAuth0 = await client.GetFromJsonAsync<AccountResponse>($"{Accounts}/by-auth0/{Uri.EscapeDataString("auth0|xyz")}");

        Assert.Equal(created.Id, byId!.Id);
        Assert.Equal(created.Id, byUsername!.Id);
        Assert.Equal(created.Id, byAuth0!.Id);
    }

    [Fact]
    public async Task Get_Unknown_IsNotFound()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreateClient();

        Assert.Equal(HttpStatusCode.NotFound, (await client.GetAsync($"{Accounts}/{EntityIds.New(EntityIds.Account)}")).StatusCode);
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
