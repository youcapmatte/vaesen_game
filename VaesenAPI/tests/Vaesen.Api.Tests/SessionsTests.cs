using System.Net;
using System.Net.Http.Json;
using Vaesen.Api.Modules.Identity;
using Vaesen.Api.Modules.Sessions;

namespace Vaesen.Api.Tests;

public class SessionsTests
{
    private const string JoinTown = "/api/v1/sessions/worlds/town/join";

    [Fact]
    public async Task Join_WithoutToken_IsUnauthorized()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Unauthorized, response.StatusCode);
    }

    [Fact]
    public async Task Join_WithGameServerToken_IsForbidden()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreateMachineClient(Auth0Options.GameServerScope).PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Forbidden, response.StatusCode);
    }

    [Fact]
    public async Task Join_UnknownWorld_IsNotFound()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreatePlayerClient().PostAsync("/api/v1/sessions/worlds/nowhere/join", null);

        Assert.Equal(HttpStatusCode.NotFound, response.StatusCode);
    }

    [Fact]
    public async Task Join_WorldWithNoSession_StartsWorldAndAsksClientToRetry()
    {
        await using var factory = new VaesenApiFactory();

        var response = await factory.CreatePlayerClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Accepted, response.StatusCode);
        Assert.Equal("2", response.Headers.GetValues("Retry-After").Single());
        var body = await response.Content.ReadFromJsonAsync<JoinWorldResponse>();
        Assert.Equal("starting", body!.Status);
        Assert.Equal("town", Assert.Single(factory.GameLift.CreatedWorlds).Id);
    }

    [Fact]
    public async Task Join_WhileWorldIsActivating_DoesNotStartASecondSession()
    {
        await using var factory = new VaesenApiFactory();
        var client = factory.CreatePlayerClient();

        await client.PostAsync(JoinTown, null);
        await client.PostAsync(JoinTown, null);

        Assert.Single(factory.GameLift.CreatedWorlds);
    }

    [Fact]
    public async Task Join_ActiveWorld_ReturnsConnectUrlWithPlayerSession()
    {
        await using var factory = new VaesenApiFactory();
        factory.GameLift.SessionsByWorld["town"] =
            new GameSessionInfo("gsess-town", GameSessionState.Active, "10.0.0.5", null, 7777);

        var response = await factory.CreatePlayerClient("auth0|abc").PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.OK, response.StatusCode);
        var body = await response.Content.ReadFromJsonAsync<JoinWorldResponse>();
        Assert.Equal("ready", body!.Status);
        Assert.Equal("psess-auth0|abc", body.PlayerSessionId);
        Assert.Equal("10.0.0.5:7777?PlayerSessionId=psess-auth0|abc", body.ConnectUrl);
    }

    [Fact]
    public async Task Join_FullWorld_IsConflict()
    {
        await using var factory = new VaesenApiFactory();
        factory.GameLift.SessionsByWorld["town"] =
            new GameSessionInfo("gsess-town", GameSessionState.Active, "10.0.0.5", null, 7777);
        factory.GameLift.FullSessions.Add("gsess-town");

        var response = await factory.CreatePlayerClient().PostAsync(JoinTown, null);

        Assert.Equal(HttpStatusCode.Conflict, response.StatusCode);
    }

    [Fact]
    public async Task Worlds_ListsConfiguredWorlds()
    {
        await using var factory = new VaesenApiFactory();

        var worlds = await factory.CreatePlayerClient().GetFromJsonAsync<WorldResponse[]>("/api/v1/sessions/worlds");

        Assert.Contains(worlds!, w => w.Id == "town");
    }

    [Fact]
    public async Task Me_ReturnsPlayerIdFromToken()
    {
        await using var factory = new VaesenApiFactory();

        var me = await factory.CreatePlayerClient("auth0|abc").GetFromJsonAsync<MeResponse>("/api/v1/identity/me");

        Assert.Equal("auth0|abc", me!.PlayerId);
    }
}
